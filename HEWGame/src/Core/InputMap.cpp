#include "Core/InputMap.h"
#include "Core/Input.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>

#ifndef VK_A
#define VK_A 0x41
#endif
#ifndef VK_D
#define VK_D 0x44
#endif
#ifndef VK_S
#define VK_S 0x53
#endif
#ifndef VK_W
#define VK_W 0x57
#endif


namespace
{
    constexpr float kAxisTriggerThreshold = 0.15f;
}

namespace Core
{
    namespace
    {
        Binding MakeBinding(int key, int btn, int axis, float sign)
        {
            Binding binding{};
            binding.key = key;
            binding.btn = btn;
            binding.axis = axis;
            binding.sign = sign;
            return binding;
        }
    }

    InputMap::InputMap()
    {
        m_bindings[ActionId::MoveX] = MakeBinding(0, 0, 0, 1.0f);
        m_bindings[ActionId::RotateYaw] = MakeBinding(0, 0, 2, 1.0f);
        m_bindings[ActionId::Drop] = MakeBinding('Q', 0, -1, 1.0f);
        m_bindings[ActionId::Confirm] = MakeBinding(VK_RETURN, 0, -1, 1.0f);
        m_bindings[ActionId::Cancel] = MakeBinding(VK_ESCAPE, 0, -1, 1.0f);
        m_bindings[ActionId::Pause] = MakeBinding(VK_SPACE, 0, -1, 1.0f);
        m_bindings[ActionId::Retry] = MakeBinding('R', 0, -1, 1.0f);
        m_bindings[ActionId::DebugConsole] = MakeBinding(VK_F1, 0, -1, 1.0f);
        m_bindings[ActionId::ToggleFullscreen] = MakeBinding(VK_F11, 0, -1, 1.0f);
        m_bindings[ActionId::CameraToggle] = MakeBinding(VK_RETURN, 0, -1, 1.0f);
        m_bindings[ActionId::CameraHeightToggle] = MakeBinding(VK_SPACE, 0, -1, 1.0f);
        m_bindings[ActionId::CameraOrbitYaw] = MakeBinding(0, 0, 1, 1.0f);
        m_bindings[ActionId::CameraOrbitPitch] = MakeBinding(0, 0, 3, -1.0f);
        m_bindings[ActionId::CameraZoom] = MakeBinding(0, 0, -1, 1.0f);
        m_bindings[ActionId::SpeedBoost] = MakeBinding('1', 0, -1, 1.0f);
        m_bindings[ActionId::SpeedReset] = MakeBinding('2', 0, -1, 1.0f);
        m_bindings[ActionId::ColliderNudgeUp] = MakeBinding(VK_F2, 0, -1, 1.0f);
        m_bindings[ActionId::ColliderNudgeDown] = MakeBinding(VK_F3, 0, -1, 1.0f);

        m_axisKeys[ActionId::MoveX] = { VK_A, VK_D };
        m_axisKeys[ActionId::RotateYaw] = { VK_S, VK_W };
        m_axisKeys[ActionId::CameraOrbitYaw] = { VK_LEFT, VK_RIGHT };
        m_axisKeys[ActionId::CameraOrbitPitch] = { VK_UP, VK_DOWN };
        m_axisKeys[ActionId::CameraZoom] = { VK_PRIOR, VK_NEXT };

        m_actionKeys[ActionId::Drop] = { 'Q' };
        m_actionKeys[ActionId::Confirm] = { static_cast<unsigned char>('E'), VK_RETURN };
        m_actionKeys[ActionId::Cancel] = { VK_ESCAPE };
        m_actionKeys[ActionId::Pause] = { VK_SPACE };
        m_actionKeys[ActionId::Retry] = { static_cast<unsigned char>('R') };
        m_actionKeys[ActionId::DebugConsole] = { VK_F1 };
        m_actionKeys[ActionId::ToggleFullscreen] = { VK_F11 };
        m_actionKeys[ActionId::CameraToggle] = { VK_RETURN };
        m_actionKeys[ActionId::CameraHeightToggle] = { VK_SPACE };
        m_actionKeys[ActionId::CameraOrbitYaw] = { VK_LEFT, VK_RIGHT };
        m_actionKeys[ActionId::CameraOrbitPitch] = { VK_UP, VK_DOWN };
        m_actionKeys[ActionId::CameraZoom] = { VK_PRIOR, VK_NEXT };
        m_actionKeys[ActionId::SpeedBoost] = { static_cast<unsigned char>('1') };
        m_actionKeys[ActionId::SpeedReset] = { static_cast<unsigned char>('2') };
        m_actionKeys[ActionId::ColliderNudgeUp] = { VK_F2 };
        m_actionKeys[ActionId::ColliderNudgeDown] = { VK_F3 };
    }

    float InputMap::GetAxis(ActionId action, const Input& input, const Gamepad& gamepad) const
    {
        float axisValue = 0.0f;

        const auto axisIt = m_axisKeys.find(action);
        if (axisIt != m_axisKeys.end())
        {
            const auto [negative, positive] = axisIt->second;
            if (negative && input.IsKeyDown(negative))
            {
                axisValue -= 1.0f;
            }
            if (positive && input.IsKeyDown(positive))
            {
                axisValue += 1.0f;
            }
        }

        if (const Binding* binding = FindBinding(action))
        {
            if (binding->key != 0)
            {
                axisValue += binding->sign * (input.IsKeyDown(static_cast<BYTE>(binding->key)) ? 1.0f : 0.0f);
            }

            if (binding->btn != 0)
            {
                axisValue += binding->sign * (gamepad.IsButtonDown(static_cast<WORD>(binding->btn)) ? 1.0f : 0.0f);
            }

            if (binding->axis >= 0)
            {
                axisValue += GetAxisValue(binding->axis, gamepad, binding->sign);
            }
        }

        return std::clamp(axisValue, -1.0f, 1.0f);
    }

    InputMap::ActionState InputMap::GetState(ActionId action, const Input& input, const Gamepad& gamepad) const
    {
        bool current = false;
        bool previous = false;

        if (const Binding* binding = FindBinding(action))
        {
            if (binding->key != 0)
            {
                current |= input.IsPress(binding->key);
                previous |= input.WasKeyDown(static_cast<BYTE>(binding->key));
            }

            if (binding->btn != 0)
            {
                current |= gamepad.IsButtonDown(static_cast<WORD>(binding->btn));
                previous |= gamepad.WasButtonDown(static_cast<WORD>(binding->btn));
            }

            if (binding->axis >= 0)
            {
                const float axis = GetAxisValue(binding->axis, gamepad, binding->sign);
                current |= std::fabs(axis) >= kAxisTriggerThreshold;
                previous |= std::fabs(GetAxisValue(binding->axis, gamepad.GetPreviousState(), binding->sign)) >= kAxisTriggerThreshold;
            }
        }

        const auto extra = m_actionKeys.find(action);
        if (extra != m_actionKeys.end())
        {
            for (unsigned char key : extra->second)
            {
                current |= input.IsPress(key);
                previous |= input.WasKeyDown(key);
            }
        }

        return ActionState{ current, previous };
    }

    bool InputMap::IsPressed(ActionId action, const Input& input, const Gamepad& gamepad) const
    {
        return GetState(action, input, gamepad).current;
    }

    bool InputMap::WasPressed(ActionId action, const Input& input, const Gamepad& gamepad) const
    {
        return GetState(action, input, gamepad).previous;
    }

    bool InputMap::Is(ActionId action, const Input& input, const Gamepad& gamepad) const
    {
        const auto state = GetState(action, input, gamepad);
        if (state.current && !state.previous)
        {
        }
        return state.current && !state.previous;
    }

    void InputMap::Rebind(ActionId action, const Binding& binding)
    {
        m_bindings[action] = binding;
    }

    const Binding* InputMap::FindBinding(ActionId action) const
    {
        const auto it = m_bindings.find(action);
        if (it == m_bindings.end())
        {
            return nullptr;
        }
        return &it->second;
    }

    float InputMap::GetAxisValue(int axis, const Gamepad& gamepad, float sign) const
    {
        float value = 0.0f;
        switch (axis)
        {
        case 0: value = gamepad.AxisLX(); break;
        case 1: value = gamepad.AxisRX(); break;
        case 2: value = gamepad.AxisLY(); break;
        case 3: value = gamepad.AxisRY(); break;
        default: value = 0.0f; break;
        }
        return value * sign;
    }

    float InputMap::GetAxisValue(int axis, const XINPUT_STATE& state, float sign) const
    {
        constexpr float kStickNorm = 32767.0f;
        float value = 0.0f;
        switch (axis)
        {
        case 0: value = state.Gamepad.sThumbLX / kStickNorm; break;
        case 1: value = state.Gamepad.sThumbRX / kStickNorm; break;
        case 2: value = state.Gamepad.sThumbLY / kStickNorm; break;
        case 3: value = state.Gamepad.sThumbRY / kStickNorm; break;
        default: value = 0.0f; break;
        }
        return value * sign;
    }

}

