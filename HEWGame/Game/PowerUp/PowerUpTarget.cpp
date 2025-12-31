// 強化対象となる内容や状態を扱う
// 強化選択の計算から参照される

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
	// ΒlvZĈړʂ𑪂
	float travelDistance = std::abs(a_position.x - m_transform.position.x) - (std::abs(a_distanse.x));
	if (travelDistance <= 0.05f) return;

	float sign = a_position.x > m_transform.position.x ? 1.0f : -1.0f;

	m_transform.position.x += travelDistance * sign;
}



