#pragma once

#include "Systems/PhysicsMachine.h"

#include <box2d/box2d.h>

namespace Systems
{
        class Box2DPhysicsSystem : public IPhysicsSystem
        {
        public:
                explicit Box2DPhysicsSystem(float gravity = 9.8f);
                ~Box2DPhysicsSystem() override;

                void Apply(float dt) override;

                void SetGravity(float gravity);
                b2WorldId GetWorldId() const;

        private:
                void DestroyWorld();

                b2WorldDef m_worldDef{};
                b2WorldId m_worldId{ b2_nullWorldId };
                bool m_loggedMissingWorld = false;
        };
}

