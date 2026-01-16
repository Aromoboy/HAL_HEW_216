#pragma once

#include <map>
#include <vector>

#include "Core/ActionId.h"

namespace Core
{
    struct Binding
    {
        int key = 0;
        float sign = 1.0f;
    };

    class Input;

    class InputMap
    {
    public:
        InputMap();

        float GetAxis(ActionId action, const Input& input) const;
        struct ActionState
        {
            bool current = false;
            bool previous = false;
        };

        ActionState GetState(ActionId action, const Input& input) const;
        bool IsPressed(ActionId action, const Input& input) const;
        bool WasPressed(ActionId action, const Input& input) const;
        bool Is(ActionId action, const Input& input) const;

        void Rebind(ActionId action, const Binding& binding);

    private:
        const Binding* FindBinding(ActionId action) const;

        std::map<ActionId, Binding> m_bindings;
        std::map<ActionId, std::pair<unsigned char, unsigned char>> m_axisKeys;
        std::map<ActionId, std::vector<unsigned char>> m_actionKeys;
    };
}
