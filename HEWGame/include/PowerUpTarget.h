#pragma once

#include "Entities/Entity.h"

class PowerUpTarget : public Entities::Entity
{
public:
	PowerUpTarget(const float3& position);
	~PowerUpTarget();

	const float3& GetPosition();
	
	// ©M‚ğˆÚ“®‚³‚¹‚é
	void MoveTarget(float2 position, float2 distanse);

private:

};