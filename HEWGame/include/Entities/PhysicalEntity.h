#pragma once

#include <vector>

#include <box2d/box2d.h>

#include "Entities/CollisionEntity.h"
#include "Physical.h"

namespace Systems
{
    class PhysicsMachine;
}

namespace Entities
{
    class PhysicalEntity : public CollisionEntity
    {
    public:
        PhysicalEntity(const Transform& transform,
            std::shared_ptr<Collision::ICollider> collider = nullptr,
            Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
            Physics::Physical physical = Physics::Physical{},
            std::shared_ptr<Render::Model> model = nullptr,
            Systems::PhysicsMachine* physicsMachine = nullptr,
            Systems::ContactLiteSystem* contactSystem = nullptr,
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0,
            std::string tag = {});
        PhysicalEntity(const std::string& tag,
            const float3& pos,
            const float3& rot,
            const float3& scl,
            std::shared_ptr<Collision::ICollider> collider,
            Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
            const float3& velocity = float3{ 0.0f, 0.0f, 0.0f },
            const float3& force = float3{ 0.0f, 0.0f, 0.0f },
            float weight = 1.0f,
            float damping = 0.98f,
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0);
        ~PhysicalEntity() override = default;

        Physics::Physical& GetPhysical();
        const Physics::Physical& GetPhysical() const;

        Systems::PhysicsMachine* GetPhysicsMachine() const;

        void SetBox2DMesh(const std::vector<std::vector<b2Vec2>>& polygons, const std::vector<b2Vec2>& boundary);
        bool HasBox2DMesh() const;
        const std::vector<std::vector<b2Vec2>>& GetBox2DPolygons() const;
        const std::vector<b2Vec2>& GetBox2DBoundary() const;

    protected:
        Physics::Physical m_physical;
        Systems::PhysicsMachine* m_physicsMachine;

        std::vector<std::vector<b2Vec2>> m_box2dPolygons;
        std::vector<b2Vec2> m_box2dBoundary;
    };
}

namespace Entities
{
    void TranslatePhysicalEntity(PhysicalEntity& entity, const float3& delta);
}

