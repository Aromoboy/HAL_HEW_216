#pragma once

#include "Core/IUpdatable.h"

namespace Core
{
        class UpdateNode : public IUpdatable
        {
        public:
                explicit UpdateNode(UpdatePhase phase = UpdatePhase::Gameplay, int priority = 0);

                void SetPhase(UpdatePhase phase);
                void SetPriority(int priority);
                void Enable(bool value);

                UpdatePhase GetPhase() const override;
                int GetPriority() const override;
                bool IsEnabled() const override;

        private:
                UpdatePhase m_phase;
                int m_priority;
                bool m_enabled;
        };
}

