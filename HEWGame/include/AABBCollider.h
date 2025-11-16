#pragma once

#include "ICollider.h"
#include "AABB.h"
#include "Transform.h"


class  AABBCollider
{
public:
	AABBCollider(Transform a_transform, bool a_isTrigger, float a_expand, float3 a_offset);
	~ AABBCollider() = default;

	// ICollider‚ªŽÀ‘•‚³‚ê‚½‚ç‚»‚¿‚ç‚É’u‚«Š·‚¦‚é
	bool CheckCollision(const Collision::ICollider& other);
	void UpdateBounds(Transform t);

	bool IsTrriger();
	const AABB GetAABB();

private:
	AABB aabb;
	bool isTrigger;
	float expand;
	float3 offset;

};

 