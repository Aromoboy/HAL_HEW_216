#pragma once

#include <Windows.h>
#include <Xinput.h>

#include <array>
#include <unordered_map>
#include <vector>

#include "Core/UpdateNode.h"

#pragma comment(lib, "xinput.lib")

namespace Core
{
        enum class ActionId
        {
                MoveX,
                MoveY,
                RotateYaw,
                Drop,
                Pickup,
                Confirm,
                Cancel,
                Pause,
                Retry,
                SpeedBoost,
                SpeedReset,
                ToggleDebugConsole,
                ToggleCameraAuto,
                ToggleCameraElevation,
                ToggleCameraMove,
                DebugNudgeUp,
                DebugNudgeDown,
                Count,
        };

        class Gamepad
        {
        public:
                enum class Axis
                {
                        None = -1,
                        LeftX,
                        LeftY,
                        RightX,
                        RightY,
                        LeftTrigger,
                        RightTrigger,
                };

                Gamepad();

                void Update(int index = 0);

                bool IsConnected() const;
                bool WasConnected() const;
                bool IsButtonDown(WORD button) const;
                bool WasButtonDown(WORD button) const;
                float GetAxis(Axis axis) const;

        private:
                float NormalizeStick(SHORT value) const;
                float NormalizeTrigger(BYTE value) const;

                bool m_connected;
                bool m_wasConnected;
                XINPUT_STATE m_state{};
                XINPUT_STATE m_prevState{};
        };

        struct Binding
        {
                int key = -1;
                int btn = -1;
                int axis = static_cast<int>(Gamepad::Axis::None);
                float sign = 1.0f;
        };

        class InputMap
        {
        public:
                using BindingList = std::vector<Binding>;

                InputMap();

                void Clear();
                void Rebind(ActionId action, const BindingList& bindings);
                void AddBinding(ActionId action, const Binding& binding);

                float GetAxis(ActionId action, const std::array<BYTE, 256>& keys, const Gamepad& pad) const;
                bool Is(ActionId action, const std::array<BYTE, 256>& keys, const Gamepad& pad) const;

        private:
                using MapType = std::unordered_map<ActionId, BindingList>;
                MapType m_bindings;
                float m_axisDeadzone;
        };

        class Input : public UpdateNode
        {
        public:
                Input();

                HRESULT Initialize();
                void Shutdown();

                void OnUpdate(UpdateContext& ctx) override;

                float GetAxis(ActionId action) const;
                bool IsAction(ActionId action) const;
                bool IsTrigger(ActionId action) const;

                bool IsPress(int code) const;
                bool IsTrigger(int code) const;
                bool IsRelease(int code) const;

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
                using KeyState = std::array<BYTE, 256>;
                static constexpr size_t kActionCount = static_cast<size_t>(ActionId::Count);

                void PollKeyboardState();
                bool IsKeyDown(BYTE key, const KeyState& state) const;
                void ConfigureDefaultBindings();

                KeyState m_currentKeys;
                KeyState m_previousKeys;
                std::array<bool, kActionCount> m_currentActions;
                std::array<bool, kActionCount> m_previousActions;
                bool m_initialized;

                Gamepad m_gamepad;
                InputMap m_map;
        };
}
