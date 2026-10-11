// SPDX-License-Identifier: GPL-3.0-only
// Captures one explicitly selected Dolphin client HWND, never the desktop.
#include <windows.h>
#include <dwmapi.h>
#include <openvr.h>
#include "core.h"
#include "native_dpi.h"
#include "d3d_upload.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
static std::atomic<bool> running{true};
static BOOL WINAPI stop(DWORD) { running=false; return TRUE; }
static void fail(const char* message) { throw std::runtime_error(message); }
struct Handle {
    HANDLE value{};
    ~Handle() { if(value && value!=INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Session {
    HMODULE dll{}; bool initialized=false; bool hotkey=false;
    vr::IVROverlay* overlay{}; vr::VROverlayHandle_t handle=vr::k_ulOverlayHandleInvalid;
    std::unique_ptr<theatre::D3DUpload> upload;
    ~Session() {
        if(hotkey) UnregisterHotKey(nullptr,1);
        if(overlay && handle!=vr::k_ulOverlayHandleInvalid) {
            overlay->HideOverlay(handle); overlay->ClearOverlayTexture(handle); overlay->DestroyOverlay(handle);
        }
        upload.reset(); // Overlay released its resource before texture/context/device.
        if(initialized) vr::VR_Shutdown();
        if(dll) FreeLibrary(dll);
    }
};
static void vrCheck(vr::EVROverlayError error,const char* operation) {
    if(error!=vr::VROverlayError_None) {
        auto* overlay=vr::VROverlay();
        throw std::runtime_error(theatre::overlayFailure(operation,static_cast<int>(error),
            overlay?overlay->GetOverlayErrorNameFromEnum(error):nullptr));
    }
}
struct ClientWindows { DWORD pid; std::vector<HWND> candidates; };
static BOOL CALLBACK enumerate(HWND window,LPARAM value) {
    auto& found=*reinterpret_cast<ClientWindows*>(value);
    DWORD pid=0; GetWindowThreadProcessId(window,&pid);
    if(theatre::eligible(pid,found.pid,IsWindowVisible(window)!=FALSE,GetWindow(window,GW_OWNER)!=nullptr)) {
        RECT rectangle{};
        if(GetClientRect(window,&rectangle) && rectangle.right>0 && rectangle.bottom>0)
            found.candidates.push_back(window);
    }
    return TRUE;
}
struct CursorOptIn { DWORD pid{}; bool announced=false; };
static void validWindow(HWND window,DWORD pid,HANDLE process,CursorOptIn& cursor) {
    DWORD current=0; GetWindowThreadProcessId(window,&current);
    if(WaitForSingleObject(process,0)!=WAIT_TIMEOUT || !IsWindow(window) || current!=pid)
        fail("Selected Dolphin process/window closed or became stale");
    if(!IsWindowVisible(window) || IsIconic(window)) fail("Selected Dolphin window is hidden/minimized");
    // A window DC can expose composited/occluded pixels on some drivers. Refuse
    // any intersecting higher window, including another Dolphin dialog.
    RECT client{};
    if(!GetClientRect(window,&client)) fail("Cannot read selected client bounds");
    POINT origin{0,0};
    if(!ClientToScreen(window,&origin)) fail("Cannot locate selected client bounds");
    OffsetRect(&client,origin.x,origin.y);
    for(HWND above=GetTopWindow(nullptr);above && above!=window;above=GetWindow(above,GW_HWNDNEXT)) {
        if(!IsWindowVisible(above) || IsIconic(above)) continue;
        DWORD cloaked=0;
        const bool cloakKnown=SUCCEEDED(DwmGetWindowAttribute(above,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)));
        // Positively cloaked windows are not rendered by DWM. Unknown state is
        // never treated as permission to capture through another window.
        if(cloakKnown && cloaked) continue;
        RECT bounds{};
        const bool frameKnown=SUCCEEDED(DwmGetWindowAttribute(above,DWMWA_EXTENDED_FRAME_BOUNDS,&bounds,sizeof(bounds))) &&
                              bounds.left<bounds.right && bounds.top<bounds.bottom;
        if(!frameKnown && !GetWindowRect(above,&bounds))
            fail("Cannot query potential occluder bounds; capture refused");
        const theatre::Rect candidate{bounds.left,bounds.top,bounds.right,bounds.bottom};
        const theatre::Rect selected{client.left,client.top,client.right,client.bottom};
        if(theatre::occludes(true,false,cloakKnown,cloaked!=0,candidate,selected)) {
            DWORD otherPid=0; GetWindowThreadProcessId(above,&otherPid);
            wchar_t className[256]{};
            const std::wstring name=GetClassNameW(above,className,256)>0?className:L"<unavailable>";
            std::string diagnostic=theatre::occlusionDiagnostic(otherPid,name,candidate,selected,cloakKnown,cloaked,frameKnown);
            if(cursor.pid && otherPid==cursor.pid) {
                const LONG_PTR style=GetWindowLongPtrW(above,GWL_EXSTYLE);
                const bool layered=(style&WS_EX_LAYERED)!=0;
                const bool transparent=(style&WS_EX_TRANSPARENT)!=0;
                const bool noActivate=(style&WS_EX_NOACTIVATE)!=0;
                std::ostringstream styles;
                styles << " ex-style=0x" << std::hex << style << std::dec
                       << " layered=" << layered << " transparent=" << transparent << " noactivate=" << noActivate;
                if(theatre::cursorException(cursor.pid,otherPid,name,layered,transparent,noActivate)) {
                    if(!cursor.announced) {
                        std::cout << "Explicit cursor-overlay opt-in allowed: " << diagnostic << styles.str() << std::endl;
                        cursor.announced=true;
                    }
                    continue; // Only the explicit per-run PID/class/style exception.
                }
                diagnostic += styles.str()+"; cursor-overlay opt-in refused (PID/class/all3 styles must match)";
            }
            throw std::runtime_error(diagnostic);
        }
    }
}
struct Capture {
    HWND window; HDC source{},memory{}; HBITMAP bitmap{}; HGDIOBJ previous{};
    unsigned w{},h{}; void* pixels{};
    explicit Capture(HWND target):window(target) {
        RECT rect{};
        if(!GetClientRect(window,&rect) || rect.left!=0 || rect.top!=0) fail("Cannot read client rectangle");
        w=static_cast<unsigned>(rect.right); h=static_cast<unsigned>(rect.bottom);
        theatre::imageBytes(w,h);
    }
    ~Capture() {
        if(previous && memory) SelectObject(memory,previous);
        if(bitmap) DeleteObject(bitmap);
        if(memory) DeleteDC(memory);
        if(source) ReleaseDC(window,source);
    }
    void copy() {
        source=GetDC(window); if(!source) fail("Cannot get selected client DC");
        memory=CreateCompatibleDC(source); if(!memory) fail("Cannot create capture DC");
        BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=static_cast<LONG>(w);
        info.bmiHeader.biHeight=-static_cast<LONG>(h); // Top-down BGRA, no vertical flip.
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
        bitmap=CreateDIBSection(source,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        if(!bitmap || !pixels) fail("Cannot allocate client DIB");
        previous=SelectObject(memory,bitmap);
        if(!previous || previous==HGDI_ERROR) fail("Cannot select client DIB");
        if(!BitBlt(memory,0,0,static_cast<int>(w),static_cast<int>(h),source,0,0,SRCCOPY))
            fail("Selected client BitBlt failed; no desktop capture fallback");
        GdiFlush();
        auto* bytes=static_cast<std::uint8_t*>(pixels);
        for(std::size_t i=3;i<theatre::imageBytes(w,h);i+=4) bytes[i]=255;
    }
};
static theatre::Matrix pose(vr::IVRSystem* system) {
    vr::TrackedDevicePose_t head{};
    system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding,0,&head,1);
    if(!head.bPoseIsValid || !head.bDeviceIsConnected) fail("HMD pose unavailable for fixed plane");
    theatre::Matrix result{};
    std::memcpy(result.data(),head.mDeviceToAbsoluteTracking.m,sizeof(head.mDeviceToAbsoluteTracking.m));
    return result;
}
static void writeHead(const fs::path& path,const theatre::Matrix& head,unsigned revision) {
    auto temporary=path; temporary+=L".tmp";
    if(fs::exists(temporary)) fail("Calibration temporary file already exists");
    try {
        std::ofstream output(temporary,std::ios::binary);
        output << std::setprecision(9) << "{\"schema\":1,\"revision\":" << revision
               << ",\"width\":3.2,\"height\":1.8,\"distance\":2.0,\"head_valid\":true,\"head\":[";
        for(std::size_t i=0;i<head.size();++i) output << (i?",":"") << head[i];
        output << "]}\n"; output.close();
        if(!output) fail("Cannot write shared calibration");
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            fail("Cannot publish shared calibration");
    } catch(...) { fs::remove(temporary); throw; }
}
static fs::path calibrationPath(const std::wstring& text) {
    auto path=fs::absolute(text).lexically_normal();
    bool local=false;
    fs::path ancestor;
    for(const auto& part:path.parent_path()) {
        ancestor/=part;
        if(part==L".local") local=true;
        const auto attrs=GetFileAttributesW(ancestor.c_str());
        if(attrs==INVALID_FILE_ATTRIBUTES || (attrs&FILE_ATTRIBUTE_REPARSE_POINT))
            fail("Calibration parent must exist without linked ancestors");
    }
    if(!local || fs::exists(path)) fail("--head-file must be a fresh file under .local");
    return path;
}
int wmain(int argc,wchar_t** argv) {
    try {
        unsigned pid=0,seconds=0; fs::path headFile; CursorOptIn cursor;
        for(int i=1;i<argc;++i) {
            const std::wstring flag=argv[i];
            if(i+1>=argc) fail("Options need values");
            if(flag==L"--pid") pid=theatre::number(argv[++i],MAXDWORD);
            else if(flag==L"--cursor-overlay-pid") cursor.pid=theatre::number(argv[++i],MAXDWORD);
            else if(flag==L"--seconds") seconds=theatre::number(argv[++i],600);
            else if(flag==L"--head-file") headFile=calibrationPath(argv[++i]);
            else fail("Unknown option");
        }
        if(!pid || !seconds || headFile.empty())
            fail("Use --pid <explicit Dolphin PID> --seconds <1..600> --head-file <fresh .local JSON>");
        PhysicalPixels physicalPixels; // DWM and client bounds must share physical pixels.
        Handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid)};
        if(!process.value) fail("Cannot open the selected Dolphin process");
        wchar_t image[32768]{}; DWORD length=32768;
        if(!QueryFullProcessImageNameW(process.value,0,image,&length) ||
           _wcsicmp(fs::path(image).filename().c_str(),L"Dolphin.exe")!=0)
            fail("Selected PID is not Dolphin.exe");
        ClientWindows windows{pid,{}};
        EnumWindows(enumerate,reinterpret_cast<LPARAM>(&windows));
        if(windows.candidates.size()!=1) fail("Expected one visible unowned Dolphin window; close dialogs first");
        HWND window=windows.candidates.front();
        validWindow(window,pid,process.value,cursor);
        // Preflight GDI pixels before any VR initialization. A black render is a blocker.
        { Capture capture(window); capture.copy(); validWindow(window,pid,process.value,cursor);
          if(!theatre::containsColour(static_cast<const std::uint8_t*>(capture.pixels),theatre::imageBytes(capture.w,capture.h)))
              fail("Selected client capture is black; actual GDI game capture is unverified. No alternate capture used"); }
        wchar_t executable[32768]{};
        if(!GetModuleFileNameW(nullptr,executable,32768)) fail("Cannot resolve viewer directory");
        Session session;
        auto dll=fs::path(executable).parent_path()/L"openvr_api.dll";
        session.dll=LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!session.dll) fail("Adjacent OpenVR DLL unavailable");
        vr::EVRInitError init{};
        auto* system=vr::VR_Init(&init,vr::VRApplication_Overlay);
        if(init!=vr::VRInitError_None) fail(vr::VR_GetVRInitErrorAsEnglishDescription(init));
        session.initialized=true; session.overlay=vr::VROverlay();
        if(!system || !session.overlay) fail("SteamVR overlay interfaces unavailable");
        int32_t adapter=-1; system->GetDXGIOutputInfo(&adapter);
        session.upload=theatre::D3DUpload::onAdapter(adapter);
        std::cout << "D3D11 overlay upload ready on SteamVR adapter index=" << adapter << std::endl;
        std::string key="aladdinscastle.dolphin.theatre."+std::to_string(GetCurrentProcessId());
        vrCheck(session.overlay->CreateOverlay(key.c_str(),"AladdinsCastle 3D theatre",&session.handle),"create overlay");
        vrCheck(session.overlay->SetOverlayFlag(session.handle,vr::VROverlayFlags_SideBySide_Parallel,true),"SBS parallel");
        vrCheck(session.overlay->SetOverlayWidthInMeters(session.handle,static_cast<float>(theatre::width)),"plane width");
        session.hotkey=RegisterHotKey(nullptr,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'C')!=0;
        if(!session.hotkey) fail("Ctrl+Alt+C recenter hotkey unavailable");
        auto recenter=[&](unsigned revision) {
            const auto head=pose(system); const auto fixed=theatre::plane(head);
            vr::HmdMatrix34_t transform{};
            std::memcpy(transform.m,fixed.data(),sizeof(transform.m));
            vrCheck(session.overlay->SetOverlayTransformAbsolute(session.handle,vr::TrackingUniverseStanding,&transform),"fixed plane");
            writeHead(headFile,head,revision);
            std::cout << "centred revision=" << revision << " (bridge must consume shared head file)\n";
        };
        unsigned revision=1; recenter(revision);
        vrCheck(session.overlay->ShowOverlay(session.handle),"show overlay");
        SetConsoleCtrlHandler(stop,TRUE);
        const auto start=Clock::now(); auto blackSince=Clock::time_point{};
        std::uint64_t frames=0;
        while(running && Clock::now()-start<std::chrono::seconds(seconds)) {
            validWindow(window,pid,process.value,cursor);
            MSG message{};
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))
                if(message.message==WM_HOTKEY && message.wParam==1) recenter(++revision);
            vr::VREvent_t event{};
            while(system->PollNextEvent(&event,sizeof(event))) if(event.eventType==vr::VREvent_Quit) running=false;
            if(!running) break;
            Capture capture(window); capture.copy();
            validWindow(window,pid,process.value,cursor);
            const auto bytes=theatre::imageBytes(capture.w,capture.h);
            if(!theatre::containsColour(static_cast<const std::uint8_t*>(capture.pixels),bytes)) {
                if(blackSince==Clock::time_point{}) blackSince=Clock::now();
                if(Clock::now()-blackSince>std::chrono::seconds(2)) fail("Client capture remained black for2s; stopping");
            } else blackSince={};
            vrCheck(session.overlay->SetOverlayTexelAspect(session.handle,theatre::texelAspect(capture.w,capture.h)),"eye aspect");
            if(session.upload->texture() && !session.upload->matches(capture.w,capture.h))
                vrCheck(session.overlay->ClearOverlayTexture(session.handle),"release old overlay texture before resize");
            session.upload->upload(capture.w,capture.h,capture.pixels,bytes);
            const vr::Texture_t texture{session.upload->texture(),vr::TextureType_DirectX,vr::ColorSpace_Gamma};
            vrCheck(session.overlay->SetOverlayTexture(session.handle,&texture),"BGRA8 D3D11 overlay submit");
            ++frames;
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
        SetConsoleCtrlHandler(stop,FALSE);
        std::cout << "Stopped; captured/submitted=" << frames << "; overlay cleanup on exit\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << "Theatre: " << e.what() << '\n'; return 1; }
}
