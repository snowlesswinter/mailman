#include "dispatcher.h"

#include <string>

#include <process.h>
#include <windows.h>

namespace {
class AutoLock
{
public:
    AutoLock(CRITICAL_SECTION* lock)
        : lock_(lock)
    {
        ::EnterCriticalSection(lock_);
    }

    ~AutoLock()
    {
        ::LeaveCriticalSection(lock_);
    }

private:
    CRITICAL_SECTION* lock_;
};

void DestroyEvent(void* event)
{
    if (event)
        CloseHandle(event);
}

CRITICAL_SECTION* CreateCriticalSection()
{
    CRITICAL_SECTION* result = new CRITICAL_SECTION();
    ::InitializeCriticalSectionAndSpinCount(result, 2000);
    return result;
}

void DestroyCriticalSection(void* critical_section)
{
    CRITICAL_SECTION* typed =
        reinterpret_cast<CRITICAL_SECTION*>(critical_section);
    if (typed) {
        ::DeleteCriticalSection(typed);
        delete typed;
        critical_section = nullptr;
    }
}

void StopThread(void* thread_handle)
{
    if (thread_handle != INVALID_HANDLE_VALUE) {
        ::WaitForSingleObject(thread_handle, -1);
        CloseHandle(thread_handle);
        thread_handle = INVALID_HANDLE_VALUE;
    }
}
}

Dispatcher::Dispatcher()
    : start_event_(::CreateEventW(nullptr, TRUE, FALSE, L""), &DestroyEvent)
    , quit_event_(::CreateEventW(nullptr, TRUE, FALSE, L""), &DestroyEvent)
    , lock_(CreateCriticalSection(), &DestroyCriticalSection)
    , thread_(INVALID_HANDLE_VALUE, &StopThread)
    , quit_(false)
{
}

Dispatcher::~Dispatcher()
{
    Stop();
    ::SetEvent(quit_event_.get());
}

void Dispatcher::Start()
{
    if (thread_.get() == INVALID_HANDLE_VALUE) {
        thread_.reset(
            reinterpret_cast<void*>(
                _beginthreadex(nullptr, 0, &Dispatcher::ThreadProcStatic, this,
                               0, nullptr)));
        ::WaitForSingleObject(start_event_.get(), -1);
    }
}

void Dispatcher::Stop()
{
    quit_ = true;
}

unsigned int Dispatcher::ThreadProcStatic(void* param)
{
    return reinterpret_cast<Dispatcher*>(param)->ThreadProc();
}

extern std::string GetPublicIP();
extern void SendMail(const std::string& subject, const std::string& message);

unsigned int Dispatcher::ThreadProc()
{
    ::SetEvent(start_event_.get());
    while (!quit_) {
        //ReadMail();
        const std::string ip_addr_text = GetPublicIP();
        SendMail("Komm, Freunde!", "ip updated: " + ip_addr_text);

        const int interval = 30 * 60 * 1000; // 30 minutes.
        ::WaitForSingleObject(quit_event_.get(), interval);
    }
    return 0;
}