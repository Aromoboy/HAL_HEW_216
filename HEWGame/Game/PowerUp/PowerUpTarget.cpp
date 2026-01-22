#include "PowerUpTarget.h"

PowerUpTarget::PowerUpTarget(const float3& position)
	: Entity("Target", position)
{}

PowerUpTarget::~PowerUpTarget()
{
}

const float3& PowerUpTarget::GetPosition()
{
	return m_transform.position;
}

void PowerUpTarget::MoveTarget(float2 a_position, float2 a_distanse)
{
	// â‘Î’l“¯m‚ğŒvZ‚µ‚ÄˆÚ“®—Ê‚ğ‘ª‚é
	float travelDistance = std::abs(a_position.x - m_transform.position.x) - (std::abs(a_distanse.x));
	if (travelDistance <= 0.05f) return;

	float sign = a_position.x > m_transform.position.x ? 1.0f : -1.0f;

	m_transform.position.x += travelDistance * sign;
}



