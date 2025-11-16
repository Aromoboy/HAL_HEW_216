#include "AABBCollider.h"


AABBCollider::AABBCollider(Transform a_transform ,bool a_isTrigger, float a_expand, float3 a_offset)
{
	UpdateBounds(a_transform);

	isTrigger = a_isTrigger;
	expand = a_expand;
	offset = a_offset;
}

bool AABBCollider::CheckCollision(const Collision::ICollider& other)
{

	const AABB thisAABB = aabb;
	const AABB otherAABB = other.GetAABB();

	

	bool hitX = otherAABB.max.x > (thisAABB.max.x > otherAABB.min.x ? thisAABB.max.x : otherAABB.min.x);

	// 衝突しているか判定
	if (thisAABB.max.x < otherAABB.min.x || 	  // x座標が重なっていなければ抜ける
		thisAABB.min.x > otherAABB.max.x) return false;

	// 衝突しているか判定
	if (thisAABB.max.y < otherAABB.min.y || 	  // y座標が重なっていなければ抜ける
		thisAABB.min.y > otherAABB.max.y) return false;

	return true;

}

void AABBCollider::UpdateBounds(Transform t)
{
	float3 max;
	float3 min;

	float3 colliderScale;
	colliderScale.x = t.scale.x / 2 * expand;
	colliderScale.y = t.scale.y / 2 * expand;
	colliderScale.z = t.scale.z / 2 * expand;


	max.x = t.position.x + offset.x + colliderScale.x;
	min.x = t.scale.x + offset.x - colliderScale.x;

	max.y = t.position.y + offset.y + colliderScale.y;
	min.y = t.scale.y + offset.y - colliderScale.y;

	max.z = t.position.z + offset.z + colliderScale.z;
	min.z = t.scale.z + offset.z - colliderScale.z;

	aabb = AABB(max, min);
}

bool AABBCollider::IsTrriger()
{
	return isTrigger;
}
const AABB AABBCollider::GetAABB()
{
	return aabb;
}
