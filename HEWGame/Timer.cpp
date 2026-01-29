#include "Timer.h"
#include <algorithm>

Timer::Timer()
	:m_maxTime(5.0f)
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
