#pragma once
#include "Core/UpdateNode.h"
#include <memory>
#include <vector>

namespace Entity
{
    class PhysicalEntity;
}

namespace SceneSpace
{
    using PhysicalEntity = Entity::PhysicalEntity;
}

namespace Systems
{
    class IPhysicsSystem
    {
    public:
        virtual ~IPhysicsSystem() = default;
        virtual void Apply(SceneSpace::PhysicalEntity& entity, float dt) = 0;
    };

    class PhysicsMachine : public Core::UpdateNode
    {
    public:
        using SystemPtr = std::shared_ptr<IPhysicsSystem>;

        PhysicsMachine();
        ~PhysicsMachine() override = default;

        void SetSystem(SystemPtr system);
        SystemPtr GetSystem() const;
        void RegisterBody(SceneSpace::PhysicalEntity& body);
        void UnregisterBody(SceneSpace::PhysicalEntity& body);
        void ClearBodies();
        void OnUpdate(Core::UpdateContext& ctx) override;

    private:
        SystemPtr m_system;
        std::vector<SceneSpace::PhysicalEntity*> m_bodies;
        bool m_loggedMissingSystem;
    };
}