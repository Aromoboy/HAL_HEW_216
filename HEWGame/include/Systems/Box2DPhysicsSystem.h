#pragma once

#include "Core/UpdateNode.h"
#include "Systems/PhysicsMachine.h"

#include <memory>

#include <box2d/box2d.h>

namespace Systems
{
        class Box2DContactListener;
        class PhysicsMachine;

        class Box2DPhysicsSystem : public Core::UpdateNode, public IPhysicsSystem
        {
        public:
                explicit Box2DPhysicsSystem(float gravity = 9.8f);
                ~Box2DPhysicsSystem() override;

                void Apply(float dt) override;
                void OnUpdate(Core::UpdateContext& ctx) override;

                void SetGravity(float gravity);
                void SetSubStepCount(int subStepCount);
                b2WorldId GetWorldId() const;
                void SetContactListener(std::shared_ptr<Box2DContactListener> listener);
                void SetPhysicsMachine(PhysicsMachine* physicsMachine);
                PhysicsMachine* GetPhysicsMachine() const;

        private:
                void DestroyWorld();

                b2WorldDef m_worldDef{};
                b2WorldId m_worldId{ b2_nullWorldId };
                std::shared_ptr<Box2DContactListener> m_contactListener;
                PhysicsMachine* m_physicsMachine = nullptr;
                int m_subStepCount = 4;
                bool m_loggedMissingWorld = false;
        };
}

