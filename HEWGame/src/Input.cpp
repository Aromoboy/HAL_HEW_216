#include "Core/Input.h"

namespace
{
        constexpr BYTE kKeyPressedMask = 0x80;
        constexpr float kStickNorm = 32767.0f;
}

namespace Core
{
        Gamepad::Gamepad()
                : m_state{}
                , m_prevState{}
                , m_connected(false)
        {
        }

        void Gamepad::Poll(int index)
        {
                m_prevState = m_state;
                const bool wasConnected = m_connected;

                XINPUT_STATE state{};
                DWORD result = XInputGetState(index, &state);

                if (result == ERROR_SUCCESS)
                {
                        m_connected = true;
                        m_state = state;
                }
                else
                {
                        m_connected = false;
                        ZeroMemory(&m_state, sizeof(XINPUT_STATE));
                }

                if (m_connected != wasConnected)
                {
                        OutputDebugStringA(m_connected
                                ? "[Core::Gamepad] Controller connected.\n"
                                : "[Core::Gamepad] Controller disconnected; cleared state.\n");
                }
        }

        bool Gamepad::IsConnected() const
        {
                return m_connected;
        }

        bool Gamepad::IsButtonDown(WORD button) const
        {
                return m_connected && (m_state.Gamepad.wButtons & button) != 0;
        }

        bool Gamepad::WasButtonDown(WORD button) const
        {
                return m_connected && (m_prevState.Gamepad.wButtons & button) != 0;
        }

        float Gamepad::AxisLX() const { return m_connected ? Normalize(m_state.Gamepad.sThumbLX) : 0.0f; }
        float Gamepad::AxisLY() const { return m_connected ? Normalize(m_state.Gamepad.sThumbLY) : 0.0f; }
        float Gamepad::AxisRX() const { return m_connected ? Normalize(m_state.Gamepad.sThumbRX) : 0.0f; }
        float Gamepad::AxisRY() const { return m_connected ? Normalize(m_state.Gamepad.sThumbRY) : 0.0f; }

        float Gamepad::Normalize(short value) const
        {
                return static_cast<float>(value) / kStickNorm;
        }

        Input::Input()
                : UpdateNode(UpdatePhase::Input, 0)
                , m_currentKeys{}
                , m_previousKeys{}
                , m_initialized(false)
        {
        }

        HRESULT Input::Initialize()
        {
                PollKeyboardState();
                m_previousKeys = m_currentKeys;

                PollPadState();

                m_initialized = true;
                return S_OK;
        }

        void Input::Shutdown()
        {
                m_initialized = false;
                m_currentKeys.fill(0);
                m_previousKeys.fill(0);

                m_pad = Gamepad();
        }

        void Input::OnUpdate(UpdateContext& ctx)
        {
                if (!m_initialized)
                {
                        return;
                }

                if (&ctx.input != this)
                {
                        OutputDebugStringA("[Core::Input] UpdateContext input reference mismatch.\n");
                }

                m_previousKeys = m_currentKeys;
                PollKeyboardState();

                PollPadState();
        }

        float Input::GetAxis(ActionId action) const
        {
                return m_map.GetAxis(action, *this, m_pad);
        }

        bool Input::IsAction(ActionId action) const
        {
                return m_map.Is(action, *this, m_pad);
        }

        void Input::Rebind(ActionId action, const Binding& binding)
        {
                m_map.Rebind(action, binding);
        }

        void Input::PollKeyboardState()
        {
                if (!::GetKeyboardState(m_currentKeys.data()))
                {
                        m_currentKeys.fill(0);
                        OutputDebugStringA("[Core::Input] GetKeyboardState failed; clearing keyboard state.\n");
                }
        }

        void Input::PollPadState(int index)
        {
                m_pad.Poll(index);
        }

        bool Input::IsKeyDown(BYTE key) const
        {
                return (m_currentKeys[key] & kKeyPressedMask) != 0;
        }

        bool Input::WasKeyDown(BYTE key) const
        {
                return (m_previousKeys[key] & kKeyPressedMask) != 0;
        }
}

