#pragma once

#include <Windows.h>
#include <map>
#include <vector>
#include <Xinput.h>

#include "Core/ActionId.h"
#include "Core/Gamepad.h"

namespace Core
{
    struct Binding
    {
        int key = 0;
        int btn = 0;
        int axis = -1;
        float sign = 1.0f;
    };

    class Input;

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

        std::map<ActionId, Binding> m_bindings;
        std::map<ActionId, std::pair<unsigned char, unsigned char>> m_axisKeys;
        std::map<ActionId, std::vector<unsigned char>> m_actionKeys;
    };
}

