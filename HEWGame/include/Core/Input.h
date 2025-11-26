#pragma once

#include <Windows.h>
#include <array>
#include <Xinput.h>

#include "Core/InputMap.h"
#include "Core/UpdateNode.h"

#pragma comment(lib, "xinput.lib")

namespace Core
{
        class Gamepad
        {
        public:
                Gamepad();

                void Poll(int index = 0);

                bool IsConnected() const;
                bool IsButtonDown(WORD button) const;
                bool WasButtonDown(WORD button) const;

                float AxisLX() const;
                float AxisLY() const;
                float AxisRX() const;
                float AxisRY() const;

                const XINPUT_STATE& GetPreviousState() const { return m_prevState; }

        private:
                float Normalize(short value) const;

                XINPUT_STATE m_state{};
                XINPUT_STATE m_prevState{};
                bool m_connected = false;
        };

        class Input : public UpdateNode
        {
        public:
                Input();

                HRESULT Initialize();
                void Shutdown();

                void OnUpdate(UpdateContext& ctx) override;

                float GetAxis(ActionId action) const;
                bool IsPress(ActionId action) const;
                bool IsTrigger(ActionId action) const;
                bool IsRelease(ActionId action) const;
                bool IsAction(ActionId action) const;
                void Rebind(ActionId action, const Binding& binding);

                bool IsPress(int vk) const;
                bool IsTrigger(int vk) const;
                bool IsRelease(int vk) const;

        private:
                void PollKeyboardState();
                void PollPadState(int index = 0);

                bool IsKeyDown(BYTE key) const;
                bool WasKeyDown(BYTE key) const;

                std::array<BYTE, 256> m_currentKeys;
                std::array<BYTE, 256> m_previousKeys;
                bool m_initialized;

                Gamepad m_pad;
                InputMap m_map;

                friend class InputMap;
        };
}

