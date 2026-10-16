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
qint64 msSinceEpoch(const FILETIME &time){
    ULARGE_INTEGER value;value.LowPart=time.dwLowDateTime;value.HighPart=time.dwHighDateTime;
    return qint64(value.QuadPart/10000ULL)-11644473600000LL;
}
}
ProcessTree::ProcessTree(){job_=CreateJobObjectW(nullptr,nullptr);endWithHub();}
ProcessTree::~ProcessTree(){if(job_)CloseHandle(job_);}
bool ProcessTree::adopt(qint64 pid){
    if(pid<=0)return false;
    bool held=false;
    if(job_)if(const HANDLE process=OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE,FALSE,DWORD(pid))){held=AssignProcessToJobObject(job_,process)!=0;CloseHandle(process);}
    resumeThreads(DWORD(pid));
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
void ProcessTree::closeWindows()const{closeProcessWindows(members());}
void ProcessTree::end(){
    if(!job_)return;
    // A game that started SteamVR itself has it in its tree: everything else ends, SteamVR never.
    const auto pids=members();QList<qint64> spared;
    for(const auto &p:listProcesses())if(pids.contains(p.pid)&&neverTouch(p))spared<<p.pid;
    if(spared.isEmpty()){TerminateJobObject(job_,1);return;}
    QList<qint64> rest;for(const auto pid:pids)if(!spared.contains(pid))rest<<pid;
    endProcesses(rest);
}
void ProcessTree::endWithHub(){
    if(!job_)return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job_,JobObjectExtendedLimitInformation,&limits,sizeof limits);
}
QList<qint64> strayHelpers(const QStringList &programNames,qint64 sinceMs){
    QList<qint64> result;if(programNames.isEmpty())return result;
    const HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)return result;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof entry;
    for(BOOL more=Process32FirstW(snapshot,&entry);more;more=Process32NextW(snapshot,&entry)){
        const auto name=QString::fromWCharArray(entry.szExeFile);
        bool wanted=false;for(const auto &program:programNames)wanted=wanted||name.compare(program,Qt::CaseInsensitive)==0;
        if(!wanted)continue;
        if(const HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID)){
            FILETIME created,exited,kernel,user;
            if(GetProcessTimes(process,&created,&exited,&kernel,&user)&&msSinceEpoch(created)>=sinceMs)result<<qint64(entry.th32ProcessID);
            CloseHandle(process);
        }
    }
    CloseHandle(snapshot);
    return result;
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
ProcessTree::ProcessTree()=default;
ProcessTree::~ProcessTree()=default;
bool ProcessTree::adopt(qint64){return false;}
QList<qint64> ProcessTree::members()const{return {};}
void ProcessTree::closeWindows()const{}
void ProcessTree::end(){}
void ProcessTree::endWithHub(){}
QList<qint64> strayHelpers(const QStringList &,qint64){return {};}
void closeProcessWindows(const QList<qint64> &){}
void endProcesses(const QList<qint64> &){}
bool processAlive(qint64 pid){return pid>0&&QFileInfo::exists("/proc/"+QString::number(pid));}
#endif
}
