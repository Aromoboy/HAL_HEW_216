#pragma once

#include "Core/UpdateNode.h"

#include <box2d/box2d.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "Entities/PhysicalEntity.h"

namespace Systems
// PhysicsMachine は Entities 階層に属する物理ボディをまとめて管理する
// UpdateNode。旧 SceneSpace 由来の型は使用せず、Entity 派生のみを受け付ける。
{
    class IPhysicsSystem
    {
    public:
        virtual ~IPhysicsSystem() = default;
        virtual void Apply(Entities::PhysicalEntity& entity, float dt) = 0;
    };

    class IntegrateSystem;

    class PhysicsMachine : public Core::UpdateNode
    {
    public:
        using SystemPtr = std::shared_ptr<IPhysicsSystem>;
        using IntegratorPtr = std::shared_ptr<IntegrateSystem>;

        explicit PhysicsMachine(float gravity = 9.8f);
        ~PhysicsMachine() override;

        void SetSystem(SystemPtr system);
        void SetIntegrator(IntegratorPtr integrator);
        SystemPtr GetSystem() const;
        IntegratorPtr GetIntegrator() const;
        const std::vector<Entities::PhysicalEntity*>& GetBodies() const;

        void SetGravity(float gravity);



        void RegisterBody(Entities::PhysicalEntity& body);
        void UnregisterBody(Entities::PhysicalEntity& body);
        void ClearBodies();

        b2BodyId FindBody(const Entities::PhysicalEntity& body) const;
        b2WorldId GetWorldId() const;

        void OnUpdate(Core::UpdateContext& ctx) override;

    private:
        b2BodyId CreateBody(Entities::PhysicalEntity& body);
        void DestroyBody(const Entities::PhysicalEntity& body);
        void DestroyWorld();
        void LogMeshContacts();

        SystemPtr m_system;
        IntegratorPtr m_integrator;
        std::vector<Entities::PhysicalEntity*> m_bodies;
        std::unordered_map<Entities::PhysicalEntity*, b2BodyId> m_bodyMap;
        b2WorldDef m_worldDef{};
        b2WorldId m_worldId{ b2_nullWorldId };
        bool m_loggedMissingSystem = false;
        bool m_loggedMissingIntegrator = false;
        bool m_loggedMissingWorld = false;
        bool m_logMeshContacts = true;
    };
}
