#include "Systems/Physics/PhysicsBinder.h"

#include <string>
#include <utility>

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

namespace
{
    constexpr const char* kLogPrefix = "[PhysicsBinder] ";

    void DebugTrace(const std::string& message)
    {
#if defined(_WIN32)
#if defined(_DEBUG)
        ::OutputDebugStringA(message.c_str());
#else
        (void)message;
#endif
#else
        (void)message;
#endif
    }
}

namespace Systems
{
    PhysicsBinder::PhysicsBinder(const PhysicsTuningParameters& tuning)
        : m_physicsMachine(std::make_unique<PhysicsMachine>(tuning))
    {
        if (!m_physicsMachine)
        {
            DebugTrace(std::string(kLogPrefix) + "Physics machine creation failed.\n");
            return;
        }

        m_registration = std::make_unique<PhysicsRegistration>(*m_physicsMachine);
    }

    PhysicsMachine* PhysicsBinder::GetPhysicsMachine() const
    {
        return m_physicsMachine.get();
    }

    void PhysicsBinder::SetPlayer(Entities::PhysicalEntity* playerEntity)
    {
        if (m_registration)
        {
            m_registration->SetPlayer(playerEntity);
        }
    }

    void PhysicsBinder::Enable(bool enabled)
    {
        if (m_physicsMachine)
        {
            m_physicsMachine->Enable(enabled);
        }
    }

    void PhysicsBinder::RegisterPhysicsBody(Entities::PhysicalEntity& entity)
    {
        if (m_registration)
        {
            m_registration->RegisterPhysicsBody(entity);
        }
    }

    void PhysicsBinder::UnregisterPhysicsBody(Entities::PhysicalEntity* entity)
    {
        if (m_registration)
        {
            m_registration->UnregisterPhysicsBody(entity);
        }
    }

    void PhysicsBinder::UnregisterAllPhysicsBodies()
    {
        if (m_registration)
        {
            m_registration->UnregisterAllPhysicsBodies();
        }
    }

    void PhysicsBinder::ValidatePlayerPhysicsRegistration()
    {
        if (m_registration)
        {
            m_registration->ValidatePlayerPhysicsRegistration();
        }
    }

    void PhysicsBinder::AddContactHandler(ContactCallback onBegin, ContactCallback onEnd, HitContactCallback onHit)
    {
        if (m_registration)
        {
            m_registration->AddContactHandler(std::move(onBegin), std::move(onEnd), std::move(onHit));
        }
    }
}
