#pragma once

#include "Core/UpdateNode.h"

#include <box2d/box2d.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace Entities
{
    class PhysicalEntity;
}

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
        bool continuousCollisionForPlayer = false;
        float creamGlueLinearHertz = 0.0f;
        float creamGlueLinearDampingRatio = 0.7f;
        float creamGlueAngularHertz = 0.0f;
        float creamGlueAngularDampingRatio = 0.7f;
        bool creamGlueCollideConnected = false;
        float creamGluePushOutEpsilon = 0.03f;
        float creamGlueMaxPushOut = 0.25f;
        float creamGlueMaxSpeed = 0.0f;
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
        Core::IUpdatable* GetContactDispatchNode() const;
        const std::vector<Entities::PhysicalEntity*>& GetBodies() const;
        const PhysicsTuningParameters& GetTuningParameters() const;

        void SetGravity(float gravity);
        void RegisterBody(Entities::PhysicalEntity& body);
        void UnregisterBody(Entities::PhysicalEntity& body, bool preserveColliderData = false);
        void ClearBodies();

        b2BodyId FindBody(const Entities::PhysicalEntity& body) const;
        b2WorldId GetWorldId() const;
        b2JointId CreateWeldJoint(const Entities::PhysicalEntity& bodyA,
            const Entities::PhysicalEntity& bodyB,
            const b2Vec2& worldAnchor,
            float linearHertz,
            float linearDampingRatio,
            float angularHertz,
            float angularDampingRatio,
            bool collideConnected = false);
        void DestroyJoint(b2JointId jointId, bool wakeAttached = true);

        void SyncBodies(float dt);
        void OnUpdate(const Core::UpdateContext& ctx) override;

    private:
        class ContactDispatchNode;

        b2BodyId CreateBody(Entities::PhysicalEntity& body);
        void DestroyBody(const Entities::PhysicalEntity& body, bool preserveColliderData);

        std::shared_ptr<Box2DPhysicsSystem> m_box2dSystem;
        SystemPtr m_system;
        ContactListenerPtr m_contactListener;
        std::unique_ptr<ContactDispatchNode> m_contactDispatchNode;
        std::vector<Entities::PhysicalEntity*> m_bodies;
        std::unordered_map<Entities::PhysicalEntity*, b2BodyId> m_bodyMap;
        b2BodyDef m_defaultBodyDef{ b2DefaultBodyDef() };
        b2ShapeDef m_defaultShapeDef{ b2DefaultShapeDef() };
        PhysicsTuningParameters m_tuning{};
        int m_subStepCount = 4;
        bool m_loggedMissingSystem = false;
        bool m_loggedMissingWorld = false;
        bool m_loggedFirstTick = false;
    };
}
