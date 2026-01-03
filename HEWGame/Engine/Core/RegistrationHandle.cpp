#include "Core/RegistrationHandle.h"

#include "Core/UpdateScheduler.h"

namespace Core
{
    RegistrationHandle::RegistrationHandle(UpdateScheduler* scheduler, NodeId nodeId)
        : m_scheduler(scheduler)
        , m_nodeId(nodeId)
    {
    }

    RegistrationHandle::~RegistrationHandle()
    {
        Reset();
    }

    RegistrationHandle::RegistrationHandle(RegistrationHandle&& other) noexcept
        : m_scheduler(other.m_scheduler)
        , m_nodeId(other.m_nodeId)
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
        m_nodeId = other.m_nodeId;
        other.Release();
        return *this;
    }

    bool RegistrationHandle::IsValid() const
    {
        return m_scheduler && m_nodeId != 0;
    }

    void RegistrationHandle::Reset()
    {
        if (IsValid())
        {
            if (m_scheduler->IsTicking())
            {
                m_scheduler->UnregisterByIdNoDeref(m_nodeId);
            }
            else
            {
                m_scheduler->UnregisterById(m_nodeId);
            }
        }
        Release();
    }

    void RegistrationHandle::Release()
    {
        m_scheduler = nullptr;
        m_nodeId = 0;
    }
}
