#pragma once

#include "Core/UpdateNode.h"

#include <memory>
#include <vector>

#include "PhysicalEntity.h"

namespace Systems
// PhysicsMachine  Entity ̊S`QƂ悤ɍXV
// sS^G[邽߁ASceneSpace::PhysicalEntity 𒼐ڈ
{
    class IPhysicsSystem
 // VXe PhysicalEntity 𒼐ڑ
// ȑO Entity::PhysicalEntity  SceneSpace ɓ


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
        const std::vector<SceneSpace::PhysicalEntity*>& GetBodies() const;


// o^{fB SceneSpace OԂɓ
// Entity ̍č\ɍ킹ύX

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