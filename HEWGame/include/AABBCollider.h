#pragma once

#include "ICollider.h"
#include "AABB.h"
#include "Transform.h"

namespace Collision
{

        class  AABBCollider : public Collision::ICollider
        {
        public:
                AABBCollider(Transform a_transform,
                             bool a_isTrigger = false,
                             float3 a_expand = float3{ 1.0f,1.0f,1.0f },
                             float3 a_offset = float3{ 0.0f, 0.0f, 0.0f },
                             CollisionLayer layer = CollisionLayer::Default,
                             float3 a_centroidOffset = float3{ 0.0f, 0.0f, 0.0f });
                ~AABBCollider() = default;

                // ICollider
                bool CheckCollision(const Collision::ICollider& other) override;
                void UpdateBounds(const Transform& transform) override;

                bool IsTrriger();
                AABB GetAABB() const override;

        private:
                AABB aabb;
                bool isTrigger;
                float3 expand;
                float3 offset;
                float3 centroidOffset;
        };

}
