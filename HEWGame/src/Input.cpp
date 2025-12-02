#include "Core/Input.h"

#include <algorithm>
#include <cmath>

namespace
{
        constexpr BYTE kKeyPressedMask = 0x80;
        constexpr float kStickNorm = 32767.0f;
        constexpr float kTriggerNorm = 255.0f;
}

namespace Core
{
        Gamepad::Gamepad()
                : m_connected(false)
                , m_wasConnected(false)
        {
        }

        void Gamepad::Update(int index)
        {
                m_prevState = m_state;
                m_wasConnected = m_connected;

                XINPUT_STATE state{};
                const DWORD result = XInputGetState(index, &state);
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
        }

        bool Gamepad::IsConnected() const
        {
                return m_connected;
        }

        bool Gamepad::WasConnected() const
        {
                return m_wasConnected;
        }

        bool Gamepad::IsButtonDown(WORD button) const
        {
                return m_connected && (m_state.Gamepad.wButtons & button) != 0;
        }

        bool Gamepad::WasButtonDown(WORD button) const
        {
                return m_wasConnected && (m_prevState.Gamepad.wButtons & button) != 0;
        }

        float Gamepad::GetAxis(Axis axis) const
        {
                if (!m_connected)
                {
                        return 0.0f;
                }

                switch (axis)
                {
                case Axis::LeftX:
                        return NormalizeStick(m_state.Gamepad.sThumbLX);
                case Axis::LeftY:
                        return NormalizeStick(m_state.Gamepad.sThumbLY);
                case Axis::RightX:
                        return NormalizeStick(m_state.Gamepad.sThumbRX);
                case Axis::RightY:
                        return NormalizeStick(m_state.Gamepad.sThumbRY);
                case Axis::LeftTrigger:
                        return NormalizeTrigger(m_state.Gamepad.bLeftTrigger);
                case Axis::RightTrigger:
                        return NormalizeTrigger(m_state.Gamepad.bRightTrigger);
                default:
                        return 0.0f;
                }
        }

        float Gamepad::NormalizeStick(SHORT value) const
        {
                return static_cast<float>(value) / kStickNorm;
        }

        float Gamepad::NormalizeTrigger(BYTE value) const
        {
                return static_cast<float>(value) / kTriggerNorm;
        }

        InputMap::InputMap()
                : m_bindings{}
                , m_axisDeadzone(0.15f)
        {
        }

        void InputMap::Clear()
        {
                m_bindings.clear();
        }

        void InputMap::Rebind(ActionId action, const BindingList& bindings)
        {
                m_bindings[action] = bindings;
        }

        void InputMap::AddBinding(ActionId action, const Binding& binding)
        {
                m_bindings[action].push_back(binding);
        }

        float InputMap::GetAxis(ActionId action, const std::array<BYTE, 256>& keys, const Gamepad& pad) const
        {
                const auto it = m_bindings.find(action);
                if (it == m_bindings.end())
                {
                        return 0.0f;
                }

                float value = 0.0f;
                for (const auto& binding : it->second)
                {
                        const bool hasAxis = static_cast<Gamepad::Axis>(binding.axis) != Gamepad::Axis::None;
                        float contribution = 0.0f;

                        if (hasAxis)
                        {
                                contribution = pad.GetAxis(static_cast<Gamepad::Axis>(binding.axis)) * binding.sign;
                                if (std::fabs(contribution) < m_axisDeadzone)
                                {
                                        contribution = 0.0f;
                                }
                        }
                        else
                        {
                                if (binding.key >= 0 && (keys[static_cast<size_t>(binding.key)] & kKeyPressedMask))
                                {
                                        contribution += binding.sign;
                                }
                                if (binding.btn >= 0 && pad.IsButtonDown(static_cast<WORD>(binding.btn)))
                                {
                                        contribution += binding.sign;
                                }
                        }

                        value += contribution;
                }

                return std::clamp(value, -1.0f, 1.0f);
        }

        bool InputMap::Is(ActionId action, const std::array<BYTE, 256>& keys, const Gamepad& pad) const
        {
                const auto it = m_bindings.find(action);
                if (it == m_bindings.end())
                {
                        return false;
                }

                for (const auto& binding : it->second)
                {
                        const bool hasAxis = static_cast<Gamepad::Axis>(binding.axis) != Gamepad::Axis::None;
                        if (hasAxis)
                        {
                                const float axisValue = pad.GetAxis(static_cast<Gamepad::Axis>(binding.axis)) * binding.sign;
                                if (std::fabs(axisValue) > m_axisDeadzone)
                                {
                                        return true;
                                }
                        }
                        else
                        {
                                if (binding.key >= 0 && (keys[static_cast<size_t>(binding.key)] & kKeyPressedMask))
                                {
                                        return true;
                                }
                                if (binding.btn >= 0 && pad.IsButtonDown(static_cast<WORD>(binding.btn)))
                                {
                                        return true;
                                }
                        }
                }

                return false;
        }

        Input::Input()
                : UpdateNode(UpdatePhase::Input, 0)
                , m_currentKeys{}
                , m_previousKeys{}
                , m_currentActions{}
                , m_previousActions{}
                , m_initialized(false)
                , m_gamepad()
                , m_map()
        {
                ConfigureDefaultBindings();
        }

        HRESULT Input::Initialize()
        {
                PollKeyboardState();
                m_previousKeys = m_currentKeys;
                m_gamepad.Update();

                m_currentActions.fill(false);
                m_previousActions.fill(false);
                m_initialized = true;
                return S_OK;
        }

        void Input::Shutdown()
        {
                m_initialized = false;
                m_currentKeys.fill(0);
                m_previousKeys.fill(0);
                m_currentActions.fill(false);
                m_previousActions.fill(false);
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

                m_gamepad.Update();

                m_previousActions = m_currentActions;
                for (size_t i = 0; i < kActionCount; ++i)
                {
                        const ActionId action = static_cast<ActionId>(i);
                        m_currentActions[i] = m_map.Is(action, m_currentKeys, m_gamepad);
                }
        }

        float Input::GetAxis(ActionId action) const
        {
                return m_map.GetAxis(action, m_currentKeys, m_gamepad);
        }

        bool Input::IsAction(ActionId action) const
        {
                return m_currentActions[static_cast<size_t>(action)];
        }

        bool Input::IsTrigger(ActionId action) const
        {
                const size_t index = static_cast<size_t>(action);
                return m_currentActions[index] && !m_previousActions[index];
        }

        bool Input::IsPress(int code) const
        {
                bool down = false;
                if (code >= 0 && code < 256)
                {
                        down = IsKeyDown(static_cast<BYTE>(code), m_currentKeys);
                }
                if (!down && code >= 0)
                {
                        down = m_gamepad.IsButtonDown(static_cast<WORD>(code));
                }
                return down;
        }

        bool Input::IsTrigger(int code) const
        {
                const bool current = IsPress(code);
                const bool previous = (code >= 0 && code < 256)
                        ? IsKeyDown(static_cast<BYTE>(code), m_previousKeys)
                        : m_gamepad.WasButtonDown(static_cast<WORD>(code));
                return current && !previous;
        }

        bool Input::IsRelease(int code) const
        {
                const bool current = IsPress(code);
                const bool previous = (code >= 0 && code < 256)
                        ? IsKeyDown(static_cast<BYTE>(code), m_previousKeys)
                        : m_gamepad.WasButtonDown(static_cast<WORD>(code));
                return !current && previous;
        }

        bool Input::IsKeyPress(BYTE key) const
        {
                return IsPress(key);
        }

        bool Input::IsKeyTrigger(BYTE key) const
        {
                return IsTrigger(key);
        }

        bool Input::IsKeyRelease(BYTE key) const
        {
                return IsRelease(key);
        }

        bool Input::IsKeyRepeat(BYTE) const
        {
                return false;
        }

        bool Input::IsPadConnected(int) const
        {
                return m_gamepad.IsConnected();
        }

        bool Input::IsPadPress(WORD button, int) const
        {
                return m_gamepad.IsButtonDown(button);
        }

        bool Input::IsPadTrigger(WORD button, int) const
        {
                return m_gamepad.IsButtonDown(button) && !m_gamepad.WasButtonDown(button);
        }

        bool Input::IsPadRelease(WORD button, int) const
        {
                return !m_gamepad.IsButtonDown(button) && m_gamepad.WasButtonDown(button);
        }

        float Input::GetPadLX(int) const
        {
                return m_gamepad.GetAxis(Gamepad::Axis::LeftX);
        }

        float Input::GetPadLY(int) const
        {
                return m_gamepad.GetAxis(Gamepad::Axis::LeftY);
        }

        float Input::GetPadRX(int) const
        {
                return m_gamepad.GetAxis(Gamepad::Axis::RightX);
        }

        float Input::GetPadRY(int) const
        {
                return m_gamepad.GetAxis(Gamepad::Axis::RightY);
        }

        float Input::GetPadLeftTrigger(int) const
        {
                return m_gamepad.GetAxis(Gamepad::Axis::LeftTrigger);
        }

        float Input::GetPadRightTrigger(int) const
        {
                return m_gamepad.GetAxis(Gamepad::Axis::RightTrigger);
        }

        void Input::PollKeyboardState()
        {
                if (!::GetKeyboardState(m_currentKeys.data()))
                {
                        m_currentKeys.fill(0);
                        OutputDebugStringA("[Core::Input] GetKeyboardState failed; clearing keyboard state.\n");
                }
        }

        bool Input::IsKeyDown(BYTE key, const KeyState& state) const
        {
                return (state[static_cast<size_t>(key)] & kKeyPressedMask) != 0;
        }

        void Input::ConfigureDefaultBindings()
        {
                using Axis = Gamepad::Axis;

                m_map.Clear();

                // Movement
                m_map.AddBinding(ActionId::MoveX, Binding{ 'A', -1, static_cast<int>(Axis::None), -1.0f });
                m_map.AddBinding(ActionId::MoveX, Binding{ 'D', -1, static_cast<int>(Axis::None), 1.0f });
                m_map.AddBinding(ActionId::MoveX, Binding{ -1, -1, static_cast<int>(Axis::LeftX), 1.0f });

                m_map.AddBinding(ActionId::MoveY, Binding{ 'S', -1, static_cast<int>(Axis::None), -1.0f });
                m_map.AddBinding(ActionId::MoveY, Binding{ 'W', -1, static_cast<int>(Axis::None), 1.0f });
                m_map.AddBinding(ActionId::MoveY, Binding{ -1, -1, static_cast<int>(Axis::LeftY), 1.0f });

                // Gameplay actions
                m_map.AddBinding(ActionId::Drop, Binding{ 'Q', static_cast<int>(XINPUT_GAMEPAD_B), static_cast<int>(Axis::None), 1.0f });
                m_map.AddBinding(ActionId::Pickup, Binding{ 'E', static_cast<int>(XINPUT_GAMEPAD_X), static_cast<int>(Axis::None), 1.0f });
                m_map.AddBinding(ActionId::Confirm, Binding{ VK_RETURN, static_cast<int>(XINPUT_GAMEPAD_START), static_cast<int>(Axis::None), 1.0f });
                m_map.AddBinding(ActionId::Cancel, Binding{ VK_ESCAPE, static_cast<int>(XINPUT_GAMEPAD_BACK), static_cast<int>(Axis::None), 1.0f });
                m_map.AddBinding(ActionId::Pause, Binding{ 'P', static_cast<int>(XINPUT_GAMEPAD_START), static_cast<int>(Axis::None), 1.0f });
                m_map.AddBinding(ActionId::Retry, Binding{ 'R', static_cast<int>(XINPUT_GAMEPAD_Y), static_cast<int>(Axis::None), 1.0f });

                m_map.AddBinding(ActionId::SpeedBoost, Binding{ '1', static_cast<int>(XINPUT_GAMEPAD_RIGHT_SHOULDER), static_cast<int>(Axis::None), 1.0f });
                m_map.AddBinding(ActionId::SpeedReset, Binding{ '2', static_cast<int>(XINPUT_GAMEPAD_LEFT_SHOULDER), static_cast<int>(Axis::None), 1.0f });

                // Debug/utility
                m_map.AddBinding(ActionId::ToggleDebugConsole, Binding{ VK_F1, -1, static_cast<int>(Axis::None), 1.0f });
                m_map.AddBinding(ActionId::ToggleCameraAuto, Binding{ VK_RETURN, -1, static_cast<int>(Axis::None), 1.0f });
                m_map.AddBinding(ActionId::ToggleCameraElevation, Binding{ VK_SPACE, -1, static_cast<int>(Axis::None), 1.0f });
                m_map.AddBinding(ActionId::ToggleCameraMove, Binding{ VK_RETURN, -1, static_cast<int>(Axis::None), 1.0f });
                m_map.AddBinding(ActionId::DebugNudgeUp, Binding{ VK_F2, -1, static_cast<int>(Axis::None), 1.0f });
                m_map.AddBinding(ActionId::DebugNudgeDown, Binding{ VK_F3, -1, static_cast<int>(Axis::None), 1.0f });
        }
}
