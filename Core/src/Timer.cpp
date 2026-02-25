#include "Core/CorePch.h"
#include "Core/Timer.h"
#include "Core/Log.h"

namespace Rebel::Core
{
	
Timer::Timer()
{
	Reset();
}

void Timer::Reset()
{
	m_Start = std::chrono::high_resolution_clock::now();
}

float Timer::Elapsed()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - m_Start).count() * 0.001f * 0.001f * 0.001f;
}

float Timer::ElapsedMillis()
{
	return Elapsed() * 1000.0f;
}

ScopedTimer::ScopedTimer(const String& name): m_Name(name)
{
	
}

ScopedTimer::~ScopedTimer()
{
	Float time = m_Timer.ElapsedMillis();
	RB_LOG(s_ScopedTimer, info, "{} - {}ms", m_Name, time, "")
}


}
