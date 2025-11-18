#pragma once

#include "ICollider.h"
#include "AABB.h"
#include "Transform.h"

namespace Collision
{

	class  AABBCollider : public Collision::ICollider
	{
	public:
		AABBCollider(Transform a_transform, bool a_isTrigger = false, float a_expand = 1.0f, float3 a_offset = float3{ 0.0f, 0.0f, 0.0f });
		~AABBCollider() = default;

		// IColliderが実装されたらそちらに置き換える
		bool CheckCollision(const Collision::ICollider& other) override;
		void UpdateBounds(const Transform& transform) override;

		bool IsTrriger();
		AABB GetAABB() const override;

	private:
		AABB aabb;
		bool isTrigger;
		float expand;
		float3 offset;

	};

}
