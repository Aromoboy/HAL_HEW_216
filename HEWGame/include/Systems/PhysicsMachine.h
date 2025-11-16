#pragma once

#include "Core/UpdateNode.h"

#include <memory>
#include <vector>
#include "PhysicalEntity.h"

namespace Systems
{
        class IPhysicsSystem
        {
        public:
                virtual ~IPhysicsSystem() = default;
                virtual void Apply(Entity::PhysicalEntity& entity, float dt) = 0;
        };

        class PhysicsMachine : public Core::UpdateNode
        {
        public:
                using SystemPtr = std::shared_ptr<IPhysicsSystem>;

                PhysicsMachine();
                ~PhysicsMachine() override = default;

                void SetSystem(SystemPtr system);
                SystemPtr GetSystem() const;

                void RegisterBody(Entity::PhysicalEntity& body);
                void UnregisterBody(Entity::PhysicalEntity& body);
                void ClearBodies();

                void OnUpdate(Core::UpdateContext& ctx) override;

        private:
                SystemPtr m_system;
                std::vector<Entity::PhysicalEntity*> m_bodies;
                bool m_loggedMissingSystem;
        };
}