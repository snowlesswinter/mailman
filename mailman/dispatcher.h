#ifndef _DISPATCHER_H_
#define _DISPATCHER_H_

#include <functional>
#include <memory>

class Dispatcher
{
public:
    Dispatcher();
    ~Dispatcher();

    void Start();
    void Stop();

private:
    static unsigned int __stdcall ThreadProcStatic(void* param);
    unsigned int ThreadProc();

    std::unique_ptr<void, std::function<void (void*)>> start_event_;
    std::unique_ptr<void, std::function<void (void*)>> quit_event_;
    std::unique_ptr<void, std::function<void (void*)>> lock_;
    std::unique_ptr<void, std::function<void (void*)>> thread_;
    bool quit_;
};

#endif  // _DISPATCHER_H_