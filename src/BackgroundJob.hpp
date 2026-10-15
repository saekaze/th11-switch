#pragma once
// Switch port: one job at a time on another CPU core (start-up runs the
// first-launch glyph bake on it while the main thread draws the loading
// screen). Same helper as th15-switch.
#include <functional>
#ifdef __SWITCH__
#include <switch.h>
#else
#include <thread>
#endif
namespace th11::sdl {
class BackgroundJob {
    std::function<void()> job;
#ifdef __SWITCH__
    Thread thread{};bool running=false;
    static void entry(void* self){static_cast<BackgroundJob*>(self)->job();}
#else
    std::thread thread;
#endif
public:
    BackgroundJob()=default;BackgroundJob(const BackgroundJob&)=delete;BackgroundJob& operator=(const BackgroundJob&)=delete;
    ~BackgroundJob(){wait();}
    // Waits for the previous job, then starts this one (runs it inline if no
    // thread can be created).
    void run(std::function<void()> next){
        wait();job=std::move(next);
#ifdef __SWITCH__
        // Core 2, away from the game thread on core 0.
        if(R_SUCCEEDED(threadCreate(&thread,entry,this,nullptr,512*1024,0x2C,2))){
            if(R_SUCCEEDED(threadStart(&thread))){running=true;return;}
            threadClose(&thread);
        }
        job();
#else
        thread=std::thread([this]{job();});
#endif
    }
    void wait(){
#ifdef __SWITCH__
        if(running){threadWaitForExit(&thread);threadClose(&thread);running=false;}
#else
        if(thread.joinable())thread.join();
#endif
    }
};
}
