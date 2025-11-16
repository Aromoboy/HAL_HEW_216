#include "AABBCollider.h"


Collision::AABBCollider::AABBCollider(Transform a_transform ,bool a_isTrigger, float a_expand, float3 a_offset)
{
	isTrigger = a_isTrigger;
	expand = a_expand;
	offset = a_offset;

	UpdateBounds(a_transform);
}

bool Collision::AABBCollider::CheckCollision(const Collision::ICollider& other) 
{

	const AABB thisAABB = aabb;
	const AABB otherAABB = other.GetAABB();


	// 衝突しているか判定
	if (thisAABB.max.x < otherAABB.min.x || 	  // x座標が重なっていなければ抜ける
		thisAABB.min.x > otherAABB.max.x) return false;

	// 衝突しているか判定
	if (thisAABB.max.y < otherAABB.min.y || 	  // y座標が重なっていなければ抜ける
		thisAABB.min.y > otherAABB.max.y) return false;

	return true;

}

void Collision::AABBCollider::UpdateBounds(const Transform& transform)
{
	float3 max;
	float3 min;

	float3 colliderScale;
	colliderScale.x = transform.scale.x / 2 * expand;
	colliderScale.y = transform.scale.y / 2 * expand;
	colliderScale.z = transform.scale.z / 2 * expand;


	max.x = transform.position.x + offset.x + colliderScale.x;
	min.x = transform.position.x + offset.x - colliderScale.x;

	max.y = transform.position.y + offset.y + colliderScale.y;
	min.y = transform.position.y + offset.y - colliderScale.y;

	max.z = transform.position.z + offset.z + colliderScale.z;
	min.z = transform.position.z + offset.z - colliderScale.z;

	aabb = AABB(max, min);
}

bool Collision::AABBCollider::IsTrriger()
{
	return isTrigger;
}

AABB Collision::AABBCollider::GetAABB() const
{
	return aabb;
}
