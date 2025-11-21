#pragma once

#include "ICollider.h"
#include "AABB.h"
#include "Transform.h"

namespace Collision
{

        class  AABBCollider : public Collision::ICollider
        {
        public:
                AABBCollider(Transform a_transform, bool a_isTrigger = false, float3 a_expand = float3{ 1.0f,1.0f,1.0f } , float3 a_offset = float3{ 0.0f, 0.0f, 0.0f }, CollisionLayer layer = CollisionLayer::Default);
                ~AABBCollider() = default;

                ColliderShape GetShape() const override { return ColliderShape::Box; }
                void UpdateFromTransform(const Transform& transform) override;
                ContactManifold ComputeContact(const Collision::ICollider& other) const override;

                bool IsTrriger();
                AABB GetAABB() const override;

        private:
                AABB aabb;
                bool isTrigger;
                float3 expand;
                float3 offset;

        };

}
