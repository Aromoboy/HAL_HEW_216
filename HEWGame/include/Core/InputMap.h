#pragma once

#include <Windows.h>
#include <map>
#include <vector>
#include <Xinput.h>

namespace Core
{
    enum class ActionId
    {
        MoveX,
        RotateYaw,
        Drop,
        Confirm,
        Cancel,
        Pause,
        Retry,
        DebugConsole,
        CameraToggle,
        CameraHeightToggle,
        SpeedBoost,
        SpeedReset,
        ColliderNudgeUp,
        ColliderNudgeDown,
    };

    struct Binding
    {
        int key = 0;
        int btn = 0;
        int axis = -1;
        float sign = 1.0f;
    };

    class Input;
    class Gamepad;

    class InputMap
    {
    public:
        InputMap();

        float GetAxis(ActionId action, const Input& input, const Gamepad& gamepad) const;
        struct ActionState
        {
            bool current = false;
            bool previous = false;
        };

        ActionState GetState(ActionId action, const Input& input, const Gamepad& gamepad) const;
        bool IsPressed(ActionId action, const Input& input, const Gamepad& gamepad) const;
        bool WasPressed(ActionId action, const Input& input, const Gamepad& gamepad) const;
        bool Is(ActionId action, const Input& input, const Gamepad& gamepad) const;

        void Rebind(ActionId action, const Binding& binding);

    private:
        const Binding* FindBinding(ActionId action) const;
        float GetAxisValue(int axis, const Gamepad& gamepad, float sign) const;
        float GetAxisValue(int axis, const XINPUT_STATE& state, float sign) const;

        static const char* ToString(ActionId action);

        std::map<ActionId, Binding> m_bindings;
        std::map<ActionId, std::pair<unsigned char, unsigned char>> m_axisKeys;
        std::map<ActionId, std::vector<unsigned char>> m_actionKeys;
    };
}

