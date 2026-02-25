#pragma once

#include <fstream>
#include <mutex>

#ifndef ANIM_DIAGNOSTICS
#define ANIM_DIAGNOSTICS 1
#endif

namespace AnimationDiagnostics
{
#if ANIM_DIAGNOSTICS
inline std::ofstream gAnimLog("AnimationDiagnostics.txt", std::ios::out | std::ios::trunc);
inline std::mutex gAnimLogMutex;
#endif
}

#if ANIM_DIAGNOSTICS
#define ANIM_LOG(x)                                                                                 \
    do                                                                                              \
    {                                                                                               \
        std::lock_guard<std::mutex> _animDiagLock(AnimationDiagnostics::gAnimLogMutex);            \
        if (AnimationDiagnostics::gAnimLog.is_open())                                               \
            AnimationDiagnostics::gAnimLog << x << std::endl;                                       \
    } while (0)

#define ANIM_LOG_FLUSH()                                                                            \
    do                                                                                              \
    {                                                                                               \
        std::lock_guard<std::mutex> _animDiagLock(AnimationDiagnostics::gAnimLogMutex);            \
        if (AnimationDiagnostics::gAnimLog.is_open())                                               \
            AnimationDiagnostics::gAnimLog.flush();                                                 \
    } while (0)
#else
#define ANIM_LOG(x) do {} while (0)
#define ANIM_LOG_FLUSH() do {} while (0)
#endif

