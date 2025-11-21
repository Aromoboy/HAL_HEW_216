#pragma once

#include <vector>

#include "ICollider.h"

namespace Collision
{
        class Prism2DCollider : public ICollider
        {
        public:
                Prism2DCollider(std::vector<float3> localVertices, float depth, float3 offset = float3{ 0.0f, 0.0f, 0.0f },
                        ColliderShape shape = ColliderShape::Prism2D, CollisionLayer layer = CollisionLayer::Default);

                ColliderShape GetShape() const override { return m_shape; }
                AABB GetAABB() const override { return m_aabb; }
                void UpdateFromTransform(const Transform& transform) override;
                ContactManifold ComputeContact(const Collision::ICollider& other) const override;

                const std::vector<float3>& GetWorldVertices() const { return m_worldVertices; }

        private:
                ContactManifold ComputePrismContact(const Prism2DCollider& other) const;

                std::vector<float3> m_localVertices;
                std::vector<float3> m_worldVertices;
                float m_depth;
                float3 m_offset;
                AABB m_aabb;
                float3 m_center;
                ColliderShape m_shape;
        };

        class BoxCollider : public Prism2DCollider
        {
        public:
                BoxCollider(const float3& halfExtents, const float3& offset = float3{ 0.0f, 0.0f, 0.0f },
                        CollisionLayer layer = CollisionLayer::Default);
        };
}
