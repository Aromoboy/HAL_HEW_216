#include "Timer.h"
#include <algorithm>

Timer::Timer(float a_maxTime)
	:m_maxTime(a_maxTime)
{
}

Timer::~Timer()
{
}

void Timer::StepTime(float a_deltatime)
{
	m_maxTime -= a_deltatime;
	m_maxTime = std::max(m_maxTime, 0.0f);
}

bool Timer::GetCount()
{
	if (m_maxTime == 0.0f)
	{
		ResetTime();
		return true;
	}
	return false;
}

void Timer::ResetTime()
{
	m_maxTime = 5.0f;
}
