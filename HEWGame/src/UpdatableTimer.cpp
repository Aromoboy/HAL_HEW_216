#include "UpdatableTimer.h"

UpdatableTimer::UpdatableTimer()
	: m_count(0.0f)
{
}

void UpdatableTimer::OnUpdate(Core::UpdateContext& ctx)
{
	m_count -= ctx.dt;

	if (m_count < 0)
	{
		m_count = 0;
	}
}

float UpdatableTimer::GetCount()
{
	return m_count;
}

void UpdatableTimer::SetCount(float a_count)
{
	m_count = a_count;
}

void UpdatableTimer::AddCount(float a_count)
{
	m_count += a_count;
}
