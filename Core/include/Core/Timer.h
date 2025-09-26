#pragma once
#include <chrono>
#include <string>

#include "CoreTypes.h"
#include "Log.h"

namespace Rebel::Core
{
    class Timer
    {
    public:
        Timer();

        void Reset();

        float Elapsed();

        float ElapsedMillis();

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> m_Start; 
    };
    inline LogCategory s_ScopedTimer{"ScopedTimer"};
    class ScopedTimer
    {
    public:
        ScopedTimer(const String& name);
       
        ~ScopedTimer();

    private:
        String m_Name;
        Timer m_Timer;
    };
}

#define PROFILE_SCOPE(name) Rebel::Core::ScopedTimer timer##__LINE__(name);
