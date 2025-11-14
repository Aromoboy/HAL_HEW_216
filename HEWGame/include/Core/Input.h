#pragma once

#include <Windows.h>
#include <array>

#include "Core/UpdateNode.h"

namespace Core
{
        class Input : public UpdateNode
        {
        public:
                Input();

                HRESULT Initialize();
                void Shutdown();

                void OnUpdate(UpdateContext& ctx) override;

                bool IsKeyPress(BYTE key) const;
                bool IsKeyTrigger(BYTE key) const;
                bool IsKeyRelease(BYTE key) const;
                bool IsKeyRepeat(BYTE key) const;

        private:
                void PollKeyboardState();

                std::array<BYTE, 256> m_currentKeys;
                std::array<BYTE, 256> m_previousKeys;
                bool m_initialized;
        };
}

