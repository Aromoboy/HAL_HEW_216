#pragma once

#include <memory>

#include "Systems/IPhysicsBinder.h"
#include "Systems/PhysicsMachine.h"
#include "Systems/PhysicsRegistration.h"

namespace Systems
{
    class PhysicsBinder : public IPhysicsBinder
    {
    public:
        explicit PhysicsBinder(const PhysicsTuningParameters& tuning = {});

        PhysicsMachine* GetPhysicsMachine() const override;
        void SetPlayer(Entities::PhysicalEntity* playerEntity) override;
        void Enable(bool enabled) override;
        void RegisterPhysicsBody(Entities::PhysicalEntity& entity) override;
        void UnregisterPhysicsBody(Entities::PhysicalEntity* entity) override;
        void UnregisterAllPhysicsBodies() override;
        void ValidatePlayerPhysicsRegistration() override;
        void AddContactHandler(ContactCallback onBegin, ContactCallback onEnd) override;

        std::unique_ptr<PhysicsMachine> m_physicsMachine;
        std::unique_ptr<PhysicsRegistration> m_registration;
    };
}
