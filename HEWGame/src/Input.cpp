#include "Core/Input.h"

namespace Core
{
        namespace
        {
                constexpr BYTE kKeyPressedMask = 0x80;
                constexpr float kStickNorm = 32767.0f;
                constexpr float kTriggerNorm = 255.0f;
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

                //  pad 
                ZeroMemory(&m_padState, sizeof(XINPUT_STATE));
                ZeroMemory(&m_prevPadState, sizeof(XINPUT_STATE));
                PollPadState();

                m_initialized = true;
                return S_OK;
        }

        void Input::Shutdown()
        {
                m_initialized = false;
                m_currentKeys.fill(0);
                m_previousKeys.fill(0);

                ZeroMemory(&m_padState, sizeof(XINPUT_STATE));
                ZeroMemory(&m_prevPadState, sizeof(XINPUT_STATE));
                m_padConnected = false;
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

                m_prevPadState = m_padState;
                PollPadState();
        }

        bool Input::IsPress(BYTE key) const
        {
                return IsKeyPress(key);
        }

        bool Input::IsTrigger(BYTE key) const
        {
                return IsKeyTrigger(key);
        }

        bool Input::IsRelease(BYTE key) const
        {
                return IsKeyRelease(key);
        }

        bool Input::IsAction(ActionId action) const
        {
                switch (action)
                {
                case ActionId::Confirm:
                        return IsPress(VK_RETURN) || IsPadPress(XINPUT_GAMEPAD_START);
                default:
                        return false;
                }
        }

        bool Input::IsTrigger(ActionId action) const
        {
                switch (action)
                {
                case ActionId::Confirm:
                        return IsTrigger(VK_RETURN) || IsPadTrigger(XINPUT_GAMEPAD_START);
                default:
                        return false;
                }
        }

        bool Input::IsKeyPress(BYTE key) const
        {
                return (m_currentKeys[key] & kKeyPressedMask) != 0;
        }

        bool Input::IsKeyTrigger(BYTE key) const
        {
                return ((m_currentKeys[key] ^ m_previousKeys[key]) & m_currentKeys[key] & kKeyPressedMask) != 0;
        }

        bool Input::IsKeyRelease(BYTE key) const
        {
                return ((m_currentKeys[key] ^ m_previousKeys[key]) & m_previousKeys[key] & kKeyPressedMask) != 0;
        }

        bool Input::IsKeyRepeat(BYTE) const
        {
                return false;
        }

        void Input::PollKeyboardState()
        {
                if (!::GetKeyboardState(m_currentKeys.data()))
                {
                        m_currentKeys.fill(0);
                        OutputDebugStringA("[Core::Input] GetKeyboardState failed; clearing keyboard state.\n");
                }
        }

        // Gamepad

        void Input::PollPadState(int index)
        {
            XINPUT_STATE state{};
            DWORD result = XInputGetState(index, &state);

            if (result == ERROR_SUCCESS)
            {
                m_padConnected = true;
                m_padState = state;
            }
            else
            {
                m_padConnected = false;
                ZeroMemory(&m_padState, sizeof(XINPUT_STATE));
            }
        }

        bool Input::IsPadConnected(int) const
        {
            return m_padConnected;
        }

        bool Input::IsPadPress(WORD button, int) const
        {
            return m_padConnected &&
                (m_padState.Gamepad.wButtons & button) != 0;
        }

        bool Input::IsPadTrigger(WORD button, int) const
        {
            return m_padConnected &&
                !(m_prevPadState.Gamepad.wButtons & button) &&
                (m_padState.Gamepad.wButtons & button);
        }

        bool Input::IsPadRelease(WORD button, int) const
        {
            return m_padConnected &&
                (m_prevPadState.Gamepad.wButtons & button) &&
                !(m_padState.Gamepad.wButtons & button);
        }

        // Stick

        float Input::GetPadLX(int) const
        {
            if (!m_padConnected) return 0.0f;
            return m_padState.Gamepad.sThumbLX / kStickNorm;
        }

        float Input::GetPadLY(int) const
        {
            if (!m_padConnected) return 0.0f;
            return m_padState.Gamepad.sThumbLY / kStickNorm;
        }

        float Input::GetPadRX(int) const
        {
            if (!m_padConnected) return 0.0f;
            return m_padState.Gamepad.sThumbRX / kStickNorm;
        }

        float Input::GetPadRY(int) const
        {
            if (!m_padConnected) return 0.0f;
            return m_padState.Gamepad.sThumbRY / kStickNorm;
        }

        // Trigger (0.0 - 1.0)

        float Input::GetPadLeftTrigger(int) const
        {
            if (!m_padConnected) return 0.0f;
            return m_padState.Gamepad.bLeftTrigger / kTriggerNorm;
        }

        float Input::GetPadRightTrigger(int) const
        {
            if (!m_padConnected) return 0.0f;
            return m_padState.Gamepad.bRightTrigger / kTriggerNorm;
        }
}

