#pragma once

#include <optional>
#include <vector>

#include <box2d/box2d.h>

#include "Entities/CollisionEntity.h"
#include "Physical.h"

namespace Systems
{
    class PhysicsMachine;
}

namespace Physics
{
    struct Box2DShapeConfig
    {
        enum class Type
        {
            None,
            Box,
            Polygon,
            Circle,
        };

        Type type{ Type::None };
        std::vector<b2Vec2> vertices{};
        float radius{ 0.0f };
        float density{ 1.0f };
        float friction{ 0.6f };
        float restitution{ 0.0f };
    };
}

namespace Entities
{
    class PhysicalEntity : public CollisionEntity
    {
    public:
        PhysicalEntity(const Transform& transform,
            std::shared_ptr<Collision::ICollider> collider = nullptr,
            Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
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
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0);
        ~PhysicalEntity() override;

        Physics::Physical& GetPhysical();
        const Physics::Physical& GetPhysical() const;

        Systems::PhysicsMachine* GetPhysicsMachine() const;

        virtual void SetBox2DBodyId(b2BodyId bodyId);
        virtual b2BodyId GetBox2DBodyId() const;
        bool HasBox2DBody() const;

        void SyncTransformFromBox2DBody();
        void SyncBox2DBodyFromTransform();

        static b2Vec2 ToBox2DPosition(const float3& worldPosition);
        static float3 ToWorldPosition(const b2Vec2& box2dPosition);
        static b2Vec2 ToBox2DVector(const float3& worldVector);
        static float3 ToWorldVector(const b2Vec2& box2dVector);
        static float ToBox2DAngle(float worldRadians);
        static float ToWorldAngle(float box2dRadians);
        static float ToBox2DLength(float worldLength);
        static float ToWorldLength(float box2dLength);

        void SetBox2DMesh(const std::vector<std::vector<b2Vec2>>& polygons, const std::vector<b2Vec2>& boundary);
        bool HasBox2DMesh() const;
        const std::vector<std::vector<b2Vec2>>& GetBox2DPolygons() const;
        const std::vector<b2Vec2>& GetBox2DBoundary() const;

        void SetBox2DShapeConfig(std::optional<Physics::Box2DShapeConfig> config);
        const std::optional<Physics::Box2DShapeConfig>& GetBox2DShapeConfig() const { return m_box2dShapeConfig; }

        void AttachPhysicsMachine(Systems::PhysicsMachine* physicsMachine);
        void DetachPhysicsMachine();

    protected:
        Physics::Physical m_physical;
        Systems::PhysicsMachine* m_physicsMachine;

        b2BodyId m_box2dBodyId{ b2_nullBodyId };

        std::vector<std::vector<b2Vec2>> m_box2dPolygons;
        std::vector<b2Vec2> m_box2dBoundary;
        std::optional<Physics::Box2DShapeConfig> m_box2dShapeConfig;

    private:
        void RecreateBox2DBody();
    };
}

namespace Entities
{
    void TranslatePhysicalEntity(PhysicalEntity& entity, const float3& delta);
}

