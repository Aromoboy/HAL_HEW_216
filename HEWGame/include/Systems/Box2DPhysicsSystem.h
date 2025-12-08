#pragma once

#include "Systems/PhysicsMachine.h"

#include <memory>

#include <box2d/box2d.h>

namespace Systems
{
        class Box2DContactListener;

        class Box2DPhysicsSystem : public IPhysicsSystem
        {
        public:
                explicit Box2DPhysicsSystem(float gravity = 9.8f);
                ~Box2DPhysicsSystem() override;

                void Apply(float dt) override;

                void SetGravity(float gravity);
                b2WorldId GetWorldId() const;
                void SetContactListener(std::shared_ptr<Box2DContactListener> listener);

        private:
                void DestroyWorld();

                b2WorldDef m_worldDef{};
                b2WorldId m_worldId{ b2_nullWorldId };
                std::shared_ptr<Box2DContactListener> m_contactListener;
                bool m_loggedMissingWorld = false;
        };
}

