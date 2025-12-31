#include "Core/RegistrationHandle.h"

#include "Core/UpdateScheduler.h"

namespace Core
{
    RegistrationHandle::RegistrationHandle(UpdateScheduler* scheduler, IUpdatable* node)
        : m_scheduler(scheduler)
        , m_node(node)
        , m_active(scheduler != nullptr && node != nullptr)
    {
    }

    RegistrationHandle::~RegistrationHandle()
    {
        Reset();
    }

    RegistrationHandle::RegistrationHandle(RegistrationHandle&& other) noexcept
        : m_scheduler(other.m_scheduler)
        , m_node(other.m_node)
        , m_active(other.m_active)
    {
        other.Release();
    }

    RegistrationHandle& RegistrationHandle::operator=(RegistrationHandle&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        Reset();

        m_scheduler = other.m_scheduler;
        m_node = other.m_node;
        m_active = other.m_active;
        other.Release();
        return *this;
    }

    bool RegistrationHandle::IsValid() const
    {
        return m_active && m_scheduler && m_node;
    }

    void RegistrationHandle::Reset()
    {
        if (IsValid())
        {
            m_scheduler->Unregister(*m_node);
        }
        Release();
    }

    void RegistrationHandle::Release()
    {
        m_scheduler = nullptr;
        m_node = nullptr;
        m_active = false;
    }
}
