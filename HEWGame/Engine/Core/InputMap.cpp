#include "Core/InputMap.h"
#include "Core/Input.h"

#include <Windows.h>

#include <algorithm>

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


namespace Core
{
    namespace
    {
        Binding MakeBinding(int key, float sign)
        {
            Binding binding{};
            binding.key = key;
            binding.sign = sign;
            return binding;
        }
    }

    InputMap::InputMap()
    {
        m_bindings[ActionId::MoveX] = MakeBinding(0, 1.0f);
        m_bindings[ActionId::RotateYaw] = MakeBinding(0, 1.0f);
        m_bindings[ActionId::Drop] = MakeBinding('Q', 1.0f);
        m_bindings[ActionId::FireMissile] = MakeBinding('E', 1.0f);
        m_bindings[ActionId::Confirm] = MakeBinding(VK_RETURN, 1.0f);
        m_bindings[ActionId::Cancel] = MakeBinding(VK_ESCAPE, 1.0f);
        m_bindings[ActionId::Pause] = MakeBinding(VK_SPACE, 1.0f);
        m_bindings[ActionId::Retry] = MakeBinding('R', 1.0f);
#if defined(_DEBUG)
        m_bindings[ActionId::DebugConsole] = MakeBinding(VK_F1, 1.0f);
#endif
        m_bindings[ActionId::DebugToggle] = MakeBinding(VK_F5, 1.0f);
        m_bindings[ActionId::ToggleFullscreen] = MakeBinding(VK_F11, 1.0f);
        m_bindings[ActionId::CameraHeightToggle] = MakeBinding(VK_SPACE, 1.0f);
        m_bindings[ActionId::SpeedBoost] = MakeBinding('1', 1.0f);
        m_bindings[ActionId::SpeedReset] = MakeBinding('2', 1.0f);
        m_bindings[ActionId::ColliderNudgeUp] = MakeBinding(VK_F2, 1.0f);
        m_bindings[ActionId::ColliderNudgeDown] = MakeBinding(VK_F3, 1.0f);
        m_bindings[ActionId::MoveMenu] = MakeBinding(0, 1.0f);

        m_axisKeys[ActionId::MoveX] = { VK_A, VK_D };
        m_axisKeys[ActionId::RotateYaw] = { VK_S, VK_W };
        m_axisKeys[ActionId::MoveMenu] = { VK_UP, VK_DOWN }; 

        m_actionKeys[ActionId::Drop] = { 'Q' };
        m_actionKeys[ActionId::FireMissile] = { static_cast<unsigned char>('E') };
        m_actionKeys[ActionId::Confirm] = { VK_RETURN };
        m_actionKeys[ActionId::Cancel] = { VK_ESCAPE };
        m_actionKeys[ActionId::Pause] = { VK_SPACE };
        m_actionKeys[ActionId::Retry] = { static_cast<unsigned char>('R') };
#if defined(_DEBUG)
        m_actionKeys[ActionId::DebugConsole] = { VK_F1 };
#endif
        m_actionKeys[ActionId::DebugToggle] = { VK_F5 };
        m_actionKeys[ActionId::ToggleFullscreen] = { VK_F11 };
        m_actionKeys[ActionId::CameraToggle] = { VK_RETURN };
        m_actionKeys[ActionId::CameraHeightToggle] = { VK_SPACE };
        m_actionKeys[ActionId::SpeedBoost] = { static_cast<unsigned char>('1') };
        m_actionKeys[ActionId::SpeedReset] = { static_cast<unsigned char>('2') };
        m_actionKeys[ActionId::ColliderNudgeUp] = { VK_F2 };
        m_actionKeys[ActionId::ColliderNudgeDown] = { VK_F3 };
    }

    float InputMap::GetAxis(ActionId action, const Input& input) const
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
        }

        return std::clamp(axisValue, -1.0f, 1.0f);
    }

    InputMap::ActionState InputMap::GetState(ActionId action, const Input& input) const
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

    bool InputMap::IsPressed(ActionId action, const Input& input) const
    {
        return GetState(action, input).current;
    }

    bool InputMap::WasPressed(ActionId action, const Input& input) const
    {
        return GetState(action, input).previous;
    }

    bool InputMap::Is(ActionId action, const Input& input) const
    {
        const auto state = GetState(action, input);
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

}
