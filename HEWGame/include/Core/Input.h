#pragma once

#include <Windows.h>
#include <array>
#include <Xinput.h>

#include "Core/UpdateNode.h"

#pragma comment(lib, "xinput.lib")

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

                bool IsPadConnected(int index = 0) const;
                bool IsPadPress(WORD button, int index = 0) const;
                bool IsPadTrigger(WORD button, int index = 0) const;
                bool IsPadRelease(WORD button, int index = 0) const;

                float GetPadLX(int index = 0) const;
                float GetPadLY(int index = 0) const;
                float GetPadRX(int index = 0) const;
                float GetPadRY(int index = 0) const;

                float GetPadLeftTrigger(int index = 0) const;   // 0.0 - 1.0
                float GetPadRightTrigger(int index = 0) const;  // 0.0 - 1.0

        private:
                void PollKeyboardState();
                void PollPadState(int index = 0);

                std::array<BYTE, 256> m_currentKeys;
                std::array<BYTE, 256> m_previousKeys;
                bool m_initialized;

                XINPUT_STATE m_padState{};
                XINPUT_STATE m_prevPadState{};
                bool m_padConnected = false;
        };
}

