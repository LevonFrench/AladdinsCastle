// SPDX-License-Identifier: GPL-3.0-only
// Captures one explicitly selected Dolphin client HWND, never the desktop.
#include <windows.h>
#include <dwmapi.h>
#include <openvr.h>
#include "core.h"
#include "native_dpi.h"
#include "d3d_upload.h"
#include "wgc_capture.h"
#include "product_title.h"
#include <winrt/base.h>
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
struct ClientWindows { DWORD pid; std::vector<HWND> candidates; std::wstring gameId; };
static BOOL CALLBACK enumerate(HWND window,LPARAM value) {
    auto& found=*reinterpret_cast<ClientWindows*>(value);
    DWORD pid=0; GetWindowThreadProcessId(window,&pid);
    if(theatre::eligible(pid,found.pid,IsWindowVisible(window)!=FALSE,GetWindow(window,GW_OWNER)!=nullptr)) {
        if(!found.gameId.empty()) {
            wchar_t title[2048]{};
            if(!GetWindowTextW(window,title,2048) || !theatre::gameWindowTitle(title,found.gameId)) return TRUE;
        }
        RECT rectangle{};
        if(GetClientRect(window,&rectangle) && rectangle.right>0 && rectangle.bottom>0)
            found.candidates.push_back(window);
    }
    return TRUE;
}
static void validWindow(HWND window,DWORD pid,HANDLE process) {
    DWORD current=0; GetWindowThreadProcessId(window,&current);
    theatre::selectedWindow(WaitForSingleObject(process,0)==WAIT_TIMEOUT,IsWindow(window)!=FALSE,current,pid,IsIconic(window)!=FALSE);
    // WGC captures this window directly; overlap and other windows are irrelevant.
}
struct CaptureApartment {
    CaptureApartment() { winrt::init_apartment(winrt::apartment_type::multi_threaded); }
    ~CaptureApartment() { winrt::uninit_apartment(); }
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
    if(!local || fs::exists(path)) fail("Output target must be a fresh path under .local");
    return path;
}
int wmain(int argc,wchar_t** argv) {
    fs::path diagnosticDir;
    Handle diagnosticDirectory;
    std::unique_ptr<theatre::FailureDiagnostic> diagnostic;
    auto failed=[&](const std::string& error,const char* prefix="Theatre: ") {
        if(diagnostic) {
            try {
                // Metadata precedes samples; partial export is never proof.
                const auto json=diagnostic->json(error);
                theatre::writeNewPrivateFile(diagnosticDir/L"failure.json",reinterpret_cast<const std::uint8_t*>(json.data()),json.size());
                CaptureApartment failureApartment;
                if(diagnostic->anchor) theatre::writeFailurePng(diagnosticDir/fs::path(diagnostic->anchorFile()),*diagnostic->anchor);
                if(diagnostic->separateLatest()) theatre::writeFailurePng(diagnosticDir/L"latest-raw.png",*diagnostic->latest);
            } catch(const std::exception& outputError) { std::cerr << "Failure diagnostic export: " << outputError.what() << '\n'; }
              catch(...) { std::cerr << "Failure diagnostic export failed; original error retained\n"; }
        }
        std::cerr << prefix << error << '\n'; return 1;
    };
    try {
        unsigned pid=0,seconds=0,startupSeconds=30; fs::path headFile,proofDir; bool captureOnly=false;
        auto recenterInput=theatre::RecenterInput::StartupOnly;
        bool recenterOptionSeen=false;
        std::wstring gameId;
        for(int i=1;i<argc;++i) {
            const std::wstring flag=argv[i];
            if(flag==L"--capture-only") { captureOnly=true; continue; }
            if(i+1>=argc) fail("Options need values");
            if(flag==L"--pid") pid=theatre::number(argv[++i],MAXDWORD);
            else if(flag==L"--seconds") seconds=theatre::number(argv[++i],600);
            else if(flag==L"--startup-seconds") startupSeconds=theatre::number(argv[++i],90);
            else if(flag==L"--game-id") gameId=argv[++i];
            else if(flag==L"--desktop-recenter-hotkey") {
                if(recenterOptionSeen) fail("Desktop recenter hotkey may be specified only once");
                recenterOptionSeen=true;recenterInput=theatre::recenterInput(argv[++i]);
            }
            else if(flag==L"--head-file") headFile=calibrationPath(argv[++i]);
            else if(flag==L"--proof-dir") proofDir=calibrationPath(argv[++i]);
            else if(flag==L"--failure-diagnostic-dir") {
                if(!fs::path(argv[i+1]).is_absolute()) fail("Failure diagnostic directory must be absolute");
                diagnosticDir=calibrationPath(argv[++i]);
            }
            else fail("Unknown option");
        }
        if(captureOnly && recenterInput==theatre::RecenterInput::DesktopHotkey)
            fail("Desktop recenter hotkey is only available in live mode");
        if(!pid || seconds<3 || (captureOnly?proofDir.empty():headFile.empty()))
            fail("Use --pid <Dolphin PID> --seconds <3..600>, plus --capture-only --proof-dir <fresh .local directory> OR --head-file <fresh .local JSON>");
        if(!captureOnly && !proofDir.empty()) fail("Proof directory is for capture-only mode");
        if(captureOnly && !theatre::validGameId(gameId)) fail("Capture-only needs --game-id <six-character disc header ID>");
        if(!gameId.empty() && !theatre::validGameId(gameId)) fail("Invalid game header ID");
        if(!diagnosticDir.empty()) {
            if(!captureOnly || diagnosticDir==proofDir) fail("Failure diagnostics need a separate capture-only directory");
            if(!CreateDirectoryW(diagnosticDir.c_str(),nullptr)) fail("Cannot reserve fresh failure diagnostic directory");
            diagnosticDirectory.value=CreateFileW(diagnosticDir.c_str(),FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
            BY_HANDLE_FILE_INFORMATION info{};
            if(diagnosticDirectory.value==INVALID_HANDLE_VALUE || !GetFileInformationByHandle(diagnosticDirectory.value,&info) ||
               (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) fail("Failure directory must remain an unlinked directory");
            diagnostic=std::make_unique<theatre::FailureDiagnostic>(startupSeconds,seconds);
        }
        PhysicalPixels physicalPixels;
        Handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid)};
        if(!process.value) fail("Cannot open the selected Dolphin process");
        wchar_t image[32768]{}; DWORD length=32768;
        if(!QueryFullProcessImageNameW(process.value,0,image,&length) ||
           _wcsicmp(fs::path(image).filename().c_str(),L"Dolphin.exe")!=0)
            fail("Selected PID is not Dolphin.exe");
        const auto startupDeadline=Clock::now()+std::chrono::seconds(startupSeconds);
        ClientWindows windows{pid,{},gameId};
        SetConsoleCtrlHandler(stop,TRUE);
        do {
            if(!running || WaitForSingleObject(process.value,0)!=WAIT_TIMEOUT) fail("Stopped before game render window appeared");
            windows.candidates.clear();
            EnumWindows(enumerate,reinterpret_cast<LPARAM>(&windows));
            if(windows.candidates.size()>1) fail("Ambiguous game render windows; stop for owner");
            if(windows.candidates.size()==1) break;
            if(Clock::now()>=startupDeadline) fail("No owned game render window before startup timeout");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } while(true);
        HWND window=windows.candidates.front();
        validWindow(window,pid,process.value);
        if(diagnostic) {
            RECT client{};
            if(!GetClientRect(window,&client) || client.right<0 || client.bottom<0) client={};
            diagnostic->select(pid,reinterpret_cast<std::uintptr_t>(window),static_cast<unsigned>(client.right),static_cast<unsigned>(client.bottom),true);
            diagnostic->stage=theatre::FailureStage::CaptureStart;
        }
        CaptureApartment apartment;
        auto deadline=startupDeadline;
        theatre::WindowCapture capture(window);
        if(diagnostic) { diagnostic->capture=capture.lastObservation(); diagnostic->stage=theatre::FailureStage::FirstCandidate; }
        auto nextFrame=[&]() {
            while(running && Clock::now()<deadline) {
                validWindow(window,pid,process.value);
                std::optional<theatre::CapturedFrame> frame;
                try { frame=capture.take(); }
                catch(...) { if(diagnostic) diagnostic->observePoll(capture.lastObservation()); throw; }
                if(diagnostic) diagnostic->observePoll(capture.lastObservation());
                if(frame) {
                    const theatre::FrameView view{frame->width,frame->height,frame->pixels.data(),frame->pixels.size(),frame->timeMs};
                    theatre::SpatialStatistics statistics;
                    const bool candidate=theatre::spatialCandidate(view,diagnostic?&statistics:nullptr);
                    if(diagnostic) diagnostic->observeFrame(view,statistics,candidate);
                    if(candidate) return *frame;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            fail("No spatially varied WGC client frame before bounded wait ended");
            return theatre::CapturedFrame{};
        };
        auto first=nextFrame();
        deadline=Clock::now()+std::chrono::seconds(seconds);
        if(diagnostic) diagnostic->stage=theatre::FailureStage::CompareChange;
        auto second=nextFrame();
        bool changed=false;
        while(!changed) {
            if(second.timeMs-first.timeMs<1000) {
                if(diagnostic) diagnostic->comparison(false);
                second=nextFrame(); continue;
            }
            changed=theatre::framesDiffer(
            {first.width,first.height,first.pixels.data(),first.pixels.size(),first.timeMs},
            {second.width,second.height,second.pixels.data(),second.pixels.size(),second.timeMs});
            if(diagnostic) diagnostic->comparison(true,changed);
            if(!changed) second=nextFrame();
        }
        if(captureOnly) {
            if(!fs::create_directory(proofDir)) fail("Cannot create fresh private proof directory");
            theatre::writeFramePng(proofDir/L"frame-1.png",first);
            theatre::writeFramePng(proofDir/L"frame-2.png",second);
            std::ofstream receipt(proofDir/L"capture.json");
            receipt << "{\"schema\":1,\"capture\":\"Windows.Graphics.Capture\",\"width\":" << first.width
                    << ",\"height\":" << first.height << ",\"interval_ms\":" << second.timeMs-first.timeMs
                    << ",\"different_rgb\":" << (changed?"true":"false")
                    << ",\"spatial_candidate\":true,\"game_window_selected\":true,\"sbs_visual_check\":\"required\""
                    << ",\"clock_metadata\":{\"schema\":1,\"compositor_unit\":\"100ns-qpc\",\"exact_gpu_frame_sync\":false,\"frames\":[";
            const auto clockFrame=[&](const theatre::CapturedFrame& frame) {
                receipt << "{\"compositor_time_100ns\":" << frame.compositorTime100ns << ',';
                ACTraceClock::Fields(receipt,frame.acquisitionClock);
                receipt << '}';
            };
            clockFrame(first); receipt << ','; clockFrame(second); receipt << "]}}\n";
            receipt.close(); if(!receipt) fail("Cannot write capture receipt");
        }
        if(!changed) fail("WGC frames one second apart have identical client RGB; gate failed");
        std::cout << "WGC two-frame gate passed: " << first.width << 'x' << first.height
                  << "; actual SBS game image requires visual inspection" << std::endl;
        if(captureOnly) { SetConsoleCtrlHandler(stop,FALSE); return 0; }
        if(!running || Clock::now()>=deadline) {
            SetConsoleCtrlHandler(stop,FALSE);
            std::cout << "Stopped after WGC gate before overlay initialization\n";
            return 0;
        }
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
        std::string key="aladdinscastle.dolphin.theatre."+std::to_string(GetCurrentProcessId());
        vrCheck(session.overlay->CreateOverlay(key.c_str(),theatre::productTitle,&session.handle),"create overlay");
        vrCheck(session.overlay->SetOverlayFlag(session.handle,vr::VROverlayFlags_SideBySide_Parallel,true),"SBS parallel");
        vrCheck(session.overlay->SetOverlayWidthInMeters(session.handle,static_cast<float>(theatre::width)),"plane width");
        session.hotkey=theatre::reserveRecenterHotkey(recenterInput,[] {
            return RegisterHotKey(nullptr,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'C')!=0;
        });
        std::cout << "Desktop recenter hotkey=" << (session.hotkey?"on":"off") << '\n';
        auto recenter=[&](unsigned revision) {
            const auto head=pose(system); const auto fixed=theatre::plane(head);
            vr::HmdMatrix34_t transform{}; std::memcpy(transform.m,fixed.data(),sizeof(transform.m));
            vrCheck(session.overlay->SetOverlayTransformAbsolute(session.handle,vr::TrackingUniverseStanding,&transform),"fixed plane");
            writeHead(headFile,head,revision);
            std::cout << "centred revision=" << revision << " (bridge must consume shared head file)\n";
        };
        unsigned revision=1; recenter(revision);
        if(!running || Clock::now()>=deadline) {
            SetConsoleCtrlHandler(stop,FALSE);
            std::cout << "Stopped after WGC gate before overlay display\n";
            return 0;
        }
        std::uint64_t frames=0;
        auto submitFrame=[&](const theatre::CapturedFrame& frame) {
            vrCheck(session.overlay->SetOverlayTexelAspect(session.handle,theatre::texelAspect(frame.width,frame.height)),"eye aspect");
            if(session.upload->texture() && !session.upload->matches(frame.width,frame.height))
                vrCheck(session.overlay->ClearOverlayTexture(session.handle),"release old overlay texture before resize");
            session.upload->upload(frame.width,frame.height,frame.pixels.data(),frame.pixels.size());
            const vr::Texture_t texture{session.upload->texture(),vr::TextureType_DirectX,vr::ColorSpace_Gamma};
            vrCheck(session.overlay->SetOverlayTexture(session.handle,&texture),"BGRA8 D3D11 overlay submit");
            ++frames;
        };
        submitFrame(second); // Preserve the proven client image if no later frame arrives.
        vrCheck(session.overlay->ShowOverlay(session.handle),"show overlay");
        while(theatre::liveFrameStep(running,Clock::now()<deadline,false)!=theatre::LiveFrameStep::Stop) {
            validWindow(window,pid,process.value);
            MSG message{};
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))
                if(session.hotkey && message.message==WM_HOTKEY && message.wParam==1) recenter(++revision);
            vr::VREvent_t event{};
            while(system->PollNextEvent(&event,sizeof(event))) if(event.eventType==vr::VREvent_Quit) running=false;
            if(!running) break;
            // TryGetNextFrame does not wait. Keep servicing recenter, quit and
            // the bounded stop even while Dolphin produces no new image.
            auto frame=capture.take();
            const auto step=theatre::liveFrameStep(running,Clock::now()<deadline,frame.has_value());
            if(step==theatre::LiveFrameStep::Stop) break;
            if(step==theatre::LiveFrameStep::Wait) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue; // Keep the last submitted overlay texture visible.
            }
            submitFrame(*frame);
        }
        SetConsoleCtrlHandler(stop,FALSE);
        std::cout << "Stopped; submitted=" << frames << "; capture/overlay cleanup on exit\n";
        return 0;
    } catch(const winrt::hresult_error& error) {
        std::ostringstream message;
        message << "HRESULT=0x" << std::hex << static_cast<unsigned long>(error.code())
                << ' ' << winrt::to_string(error.message());
        return failed(message.str(),"WGC: ");
    }
      catch(const std::exception& error) { return failed(error.what()); }
}
