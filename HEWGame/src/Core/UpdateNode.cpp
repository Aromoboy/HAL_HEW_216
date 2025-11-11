#include "Core/UpdateNode.h"

namespace Core
{
        UpdateNode::UpdateNode(UpdatePhase phase, int priority)
                : m_phase(phase)
                , m_priority(priority)
                , m_enabled(true)
        {
        }

        void UpdateNode::SetPhase(UpdatePhase phase)
        {
                m_phase = phase;
        }

        void UpdateNode::SetPriority(int priority)
        {
                m_priority = priority;
        }

        void UpdateNode::Enable(bool value)
        {
                m_enabled = value;
        }

        UpdatePhase UpdateNode::GetPhase() const
        {
                return m_phase;
        }

        int UpdateNode::GetPriority() const
        {
                return m_priority;
        }

        bool UpdateNode::IsEnabled() const
        {
                return m_enabled;
        }
}

