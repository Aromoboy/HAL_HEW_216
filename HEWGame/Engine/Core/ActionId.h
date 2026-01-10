#pragma once

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
        DebugToggle,
        ToggleFullscreen,
        CameraToggle,
        CameraHeightToggle,
        SpeedBoost,
        SpeedReset,
        ColliderNudgeUp,
        ColliderNudgeDown,
        MoveMenu
    };
}
