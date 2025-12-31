// 強化対象となる内容や状態を扱う
// 強化選択の計算から参照される

#pragma once

#include "Entities/Entity.h"

class PowerUpTarget : public Entities::Entity
{
public:
	PowerUpTarget(const float3& position);
	~PowerUpTarget();

	const float3& GetPosition();
	
	// Mړ
	void MoveTarget(float2 position, float2 distanse);

private:

};