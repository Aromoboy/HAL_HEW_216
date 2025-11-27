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
        struct Contact2D
        {
                float2 normal{ 0.0f, 1.0f };
                float penetration{ 0.0f };
        };

        class CircleCollider2D;
        class ConvexHullCollider2D;
        class CompoundCollider2D;

        class Collider2DBase : public Collision::ICollider
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

                bool ComputeContact(const CircleCollider2D& other, Contact2D* outContact = nullptr) const;
                bool ComputeContact(const ConvexHullCollider2D& hull, Contact2D* outContact = nullptr) const;

                float2 GetCenter() const { return m_center; }
                float GetRadius() const { return m_radius; }

        private:
                float2 m_offset;
                float m_radius;
                float2 m_center;
        };

        class ConvexHullCollider2D : public Collider2DBase
        {
        public:
                explicit ConvexHullCollider2D(std::vector<float2> points, float2 offset = float2{ 0.0f, 0.0f }, float thickness = 0.1f,
                        CollisionLayer layer = CollisionLayer::Default);

                bool CheckCollision(const Collision::ICollider& other) override;
                void UpdateBounds(const Transform& transform) override;

                bool ComputeContact(const ConvexHullCollider2D& other, Contact2D* outContact = nullptr) const;
                bool ComputeContact(const CircleCollider2D& circle, Contact2D* outContact = nullptr) const;

                const std::vector<float2>& GetWorldPoints() const { return m_worldPoints; }

        private:
                std::vector<float2> m_points;
                std::vector<float2> m_worldPoints;
                float2 m_offset;
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

                const std::vector<std::shared_ptr<Collision::ICollider>>& GetChildren() const { return m_children; }

        private:
                std::vector<std::shared_ptr<Collision::ICollider>> m_children;
        };
}

