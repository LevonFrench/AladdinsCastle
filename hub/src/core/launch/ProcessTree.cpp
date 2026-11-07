// SPDX-License-Identifier: GPL-3.0-only
#include "ProcessTree.h"
#include "Watchdog.h"
#include <QFileInfo>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#endif
namespace ac::launch {
#ifdef Q_OS_WIN
namespace {
void resumeThreads(DWORD pid){
    const HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    if(snapshot==INVALID_HANDLE_VALUE)return;
    THREADENTRY32 entry{};entry.dwSize=sizeof entry;
    for(BOOL more=Thread32First(snapshot,&entry);more;more=Thread32Next(snapshot,&entry)){
        if(entry.th32OwnerProcessID!=pid)continue;
        if(const HANDLE thread=OpenThread(THREAD_SUSPEND_RESUME,FALSE,entry.th32ThreadID)){
            // A thread suspended at creation needs one resume; never resume beyond that.
            if(ResumeThread(thread)>1)SuspendThread(thread);
            CloseHandle(thread);
        }
    }
    CloseHandle(snapshot);
}
struct WindowSearch { QList<qint64> pids; };
BOOL CALLBACK postClose(HWND window,LPARAM parameter){
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(reinterpret_cast<WindowSearch *>(parameter)->pids.contains(qint64(pid))&&GetWindow(window,GW_OWNER)==nullptr)PostMessageW(window,WM_CLOSE,0,0);
    return TRUE;
}
}
ProcessTree::ProcessTree(std::function<bool(qint64)> adoptionGate):adoptionGate_(std::move(adoptionGate)){job_=CreateJobObjectW(nullptr,nullptr);endWithHub();}
ProcessTree::~ProcessTree(){
    if(!job_)return;
    // Explicit teardown must preserve neverTouch members even when the last job
    // handle closes. Crash cleanup remains enabled until this orderly teardown.
    end();CloseHandle(job_);
}
bool ProcessTree::adopt(qint64 pid){
    if(pid<=0)return false;
    bool held=false;
    if(job_&&(!adoptionGate_||adoptionGate_(pid)))if(const HANDLE process=OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE,FALSE,DWORD(pid))){held=AssignProcessToJobObject(job_,process)!=0;CloseHandle(process);}
    if(held)resumeThreads(DWORD(pid));
    return held;
}
QList<qint64> ProcessTree::members()const{
    QList<qint64> result;if(!job_)return result;
    std::vector<unsigned char> buffer(sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST)+1024*sizeof(ULONG_PTR));
    auto *list=reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST *>(buffer.data());
    if(QueryInformationJobObject(job_,JobObjectBasicProcessIdList,list,DWORD(buffer.size()),nullptr)||GetLastError()==ERROR_MORE_DATA)
        for(DWORD i=0;i<list->NumberOfProcessIdsInList;++i)result<<qint64(list->ProcessIdList[i]);
    return result;
}
void ProcessTree::closeWindows()const{
    QList<qint64> pids;QList<HANDLE> handles;
    for(const auto pid:members()){
        const HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,DWORD(pid));
        if(!process)continue;
        BOOL owned=FALSE;wchar_t path[32768];DWORD size=32768;
        if(IsProcessInJob(process,job_,&owned)&&owned&&QueryFullProcessImageNameW(process,0,path,&size)){
            ProcessInfo info;info.pid=pid;info.path=QString::fromWCharArray(path,int(size));info.name=QFileInfo(info.path).fileName();
            if(!neverTouch(info)){pids<<pid;handles<<process;continue;}
        }
        CloseHandle(process);
    }
    // Retained handles prevent a PID from being reused while windows are enumerated.
    closeProcessWindows(pids);for(const auto handle:handles)CloseHandle(handle);
}
void ProcessTree::end(){
    if(!job_)return;
    QList<HANDLE> safe;bool selective=false;
    for(const auto pid:members()){
        const HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_TERMINATE,FALSE,DWORD(pid));
        if(!process){selective=true;continue;}
        BOOL owned=FALSE;wchar_t path[32768];DWORD size=32768;
        if(IsProcessInJob(process,job_,&owned)&&owned&&QueryFullProcessImageNameW(process,0,path,&size)){
            ProcessInfo info;info.pid=pid;info.path=QString::fromWCharArray(path,int(size));info.name=QFileInfo(info.path).fileName();
            if(!neverTouch(info)){safe<<process;continue;}
        }
        // Unknown identities and protected members prohibit indiscriminate job
        // termination. A failed identity check never authorizes a process kill.
        selective=true;CloseHandle(process);
    }
    if(!selective){
        // Preserve the atomic job boundary for ordinary games, including any
        // descendants born after enumeration but before termination. Keep the
        // kill-on-close fallback active through the final handle close as well.
        TerminateJobObject(job_,1);
    }else{
        // Kill-on-close cannot express exclusions. Disable it only for jobs
        // requiring selective cleanup, before any final job handle can close.
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        SetInformationJobObject(job_,JobObjectExtendedLimitInformation,&limits,sizeof limits);
        for(const auto process:safe)TerminateProcess(process,1);
    }
    for(const auto process:safe)CloseHandle(process);
}
void ProcessTree::endWithHub(){
    if(!job_)return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job_,JobObjectExtendedLimitInformation,&limits,sizeof limits);
}
void closeProcessWindows(const QList<qint64> &pids){if(pids.isEmpty())return;WindowSearch search{pids};EnumWindows(postClose,reinterpret_cast<LPARAM>(&search));}
void endProcesses(const QList<qint64> &pids){
    for(const auto pid:pids)if(const HANDLE process=OpenProcess(PROCESS_TERMINATE,FALSE,DWORD(pid))){TerminateProcess(process,1);CloseHandle(process);}
}
bool processAlive(qint64 pid){
    const HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,DWORD(pid));if(!process)return false;
    const bool alive=WaitForSingleObject(process,0)==WAIT_TIMEOUT;CloseHandle(process);return alive;
}
#else
ProcessTree::ProcessTree(std::function<bool(qint64)> adoptionGate):adoptionGate_(std::move(adoptionGate)){}
ProcessTree::~ProcessTree()=default;
bool ProcessTree::adopt(qint64){return false;}
QList<qint64> ProcessTree::members()const{return {};}
void ProcessTree::closeWindows()const{}
void ProcessTree::end(){}
void ProcessTree::endWithHub(){}
void closeProcessWindows(const QList<qint64> &){}
void endProcesses(const QList<qint64> &){}
bool processAlive(qint64 pid){return pid>0&&QFileInfo::exists("/proc/"+QString::number(pid));}
#endif
}
