#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "AABB.h"
#include "ICollider.h"
#include "Transform.h"
#include "Vector.h"

namespace Collision
{
        struct ContactManifold2D
        {
                float2 normal{ 0.0f, 1.0f };
                float penetration{ 0.0f };
                float2 contactPoint{ 0.0f, 0.0f };
        };

        class CircleCollider2D;
        class ConvexHullCollider2D;
        class CompoundCollider2D;

        class ICollider2D : public Collision::ICollider
        {
        public:
                virtual bool ComputeContact(const ICollider2D& other, ContactManifold2D& manifold) const = 0;
        };

        class Collider2DBase : public Collision::ICollider2D
        {
        public:
                Collider2DBase() = default;
                explicit Collider2DBase(float thickness)
                        : m_halfThickness(std::max(thickness * 0.5f, 0.0f))
                {
                }

                AABB GetAABB() const override { return m_aabb; }

        protected:
                void UpdateZBounds(const Transform& transform, float thicknessScale);

                AABB m_aabb{};
                float m_halfThickness{ 0.05f };
        };

        class CircleCollider2D : public Collider2DBase
        {
        public:
                CircleCollider2D(float radius, float2 offset = float2{ 0.0f, 0.0f }, float thickness = 0.1f,
                        CollisionLayer layer = CollisionLayer::Default);

                bool CheckCollision(const Collision::ICollider& other) override;
                void UpdateBounds(const Transform& transform) override;

                bool ComputeContact(const ICollider2D& other, ContactManifold2D& manifold) const override;

                bool ComputeContact(const CircleCollider2D& other, ContactManifold2D* outContact = nullptr) const;
                bool ComputeContact(const ConvexHullCollider2D& hull, ContactManifold2D* outContact = nullptr) const;

                float2 GetCenter() const { return m_center; }
                float GetRadius() const { return m_scaledRadius; }

        private:
                float2 m_offset;
                float m_radius;
                float m_scaledRadius{ 0.0f };
                float m_radiusScale{ 1.0f };
                float2 m_center;
        };

        class ConvexHullCollider2D : public Collider2DBase
        {
        public:
                explicit ConvexHullCollider2D(std::vector<float2> points, float2 offset = float2{ 0.0f, 0.0f }, float thickness = 0.1f,
                        CollisionLayer layer = CollisionLayer::Default, float2 centroidOffset = float2{ 0.0f, 0.0f });

                bool CheckCollision(const Collision::ICollider& other) override;
                void UpdateBounds(const Transform& transform) override;

                bool ComputeContact(const ICollider2D& other, ContactManifold2D& manifold) const override;

                bool ComputeContact(const ConvexHullCollider2D& other, ContactManifold2D* outContact = nullptr) const;
                bool ComputeContact(const CircleCollider2D& circle, ContactManifold2D* outContact = nullptr) const;

                const std::vector<float2>& GetWorldPoints() const { return m_worldPoints; }

        private:
                std::vector<float2> m_points;
                std::vector<float2> m_worldPoints;
                float2 m_offset;
                float2 m_centroidOffset;
                float m_rotation;
        };

        class CompoundCollider2D : public Collider2DBase
        {
        public:
                CompoundCollider2D(std::vector<std::shared_ptr<Collision::ICollider>> children,
                        float thickness = 0.1f,
                        CollisionLayer layer = CollisionLayer::Default);

                bool CheckCollision(const Collision::ICollider& other) override;
                void UpdateBounds(const Transform& transform) override;

                bool ComputeContact(const ICollider2D& other, ContactManifold2D& manifold) const override;

                const std::vector<std::shared_ptr<Collision::ICollider>>& GetChildren() const { return m_children; }

        private:
                std::vector<std::shared_ptr<Collision::ICollider>> m_children;
        };
}

