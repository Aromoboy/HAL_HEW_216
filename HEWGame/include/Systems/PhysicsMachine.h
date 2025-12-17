#pragma once

#include "Core/UpdateNode.h"

#include <box2d/box2d.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "Entities/PhysicalEntity.h"

namespace Systems
// PhysicsMachine は Entities 階層に属する物理ボディをまとめて管理する
// UpdateNode 旧 SceneSpace 由来の型は使用せず、Entity 派生のみを受け付ける 
{
    struct PhysicsTuningParameters
    {
        float gravity = 9.8f;
        int subStepCount = 4;
        float defaultLinearDamping = 0.0f;
        float defaultAngularDamping = 0.0f;
        float defaultFriction = 0.6f;
        float defaultRestitution = 0.0f;
        float restitutionThreshold = 1.0f;
        float hitEventThreshold = 0.0f;
        float maxStepDt = 1.0f / 60.0f;
        int maxAccumulatedSteps = 8;
        bool continuousCollisionForSweets = false;
        bool continuousCollisionForSmallDynamics = false;
        float smallDynamicContinuousThreshold = 0.0f;
        bool continuousCollisionForPlayerProjectiles = false;
    };

    class IPhysicsSystem
    {
    public:
        virtual ~IPhysicsSystem() = default;
        virtual void Apply(float dt) = 0;
    };

    class Box2DPhysicsSystem;
    class Box2DContactListener;

    class PhysicsMachine : public Core::UpdateNode
    {
    public:
        using SystemPtr = std::shared_ptr<IPhysicsSystem>;
        using ContactListenerPtr = std::shared_ptr<Box2DContactListener>;

        explicit PhysicsMachine(const PhysicsTuningParameters& tuning = {});
        ~PhysicsMachine() override;

        void SetSystem(SystemPtr system);
        void SetContactListener(ContactListenerPtr listener);
        void SetSubStepCount(int subStepCount);
        SystemPtr GetSystem() const;
        std::shared_ptr<Box2DPhysicsSystem> GetBox2DSystem() const;
        const std::vector<Entities::PhysicalEntity*>& GetBodies() const;

        void SetGravity(float gravity);
        void RegisterBody(Entities::PhysicalEntity& body);
        void UnregisterBody(Entities::PhysicalEntity& body);
        void ClearBodies();

        b2BodyId FindBody(const Entities::PhysicalEntity& body) const;
        b2WorldId GetWorldId() const;

        void SyncBodies(float dt);
        void OnUpdate(Core::UpdateContext& ctx) override;

    private:
        b2BodyId CreateBody(Entities::PhysicalEntity& body);
        void DestroyBody(const Entities::PhysicalEntity& body);

        std::shared_ptr<Box2DPhysicsSystem> m_box2dSystem;
        SystemPtr m_system;
        ContactListenerPtr m_contactListener;
        std::vector<Entities::PhysicalEntity*> m_bodies;
        std::unordered_map<Entities::PhysicalEntity*, b2BodyId> m_bodyMap;
        b2BodyDef m_defaultBodyDef{ b2DefaultBodyDef() };
        b2ShapeDef m_defaultShapeDef{ b2DefaultShapeDef() };
        PhysicsTuningParameters m_tuning{};
        int m_subStepCount = 4;
        bool m_loggedMissingSystem = false;
        bool m_loggedMissingWorld = false;
        bool m_loggedFirstTick = false;
        int m_missingShapeLogCount = 0;
    };
}
