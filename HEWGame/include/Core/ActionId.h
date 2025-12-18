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
        ToggleFullscreen,
        CameraToggle,
        CameraHeightToggle,
        CameraOrbitYaw,
        CameraOrbitPitch,
        CameraZoom,
        SpeedBoost,
        SpeedReset,
        ColliderNudgeUp,
        ColliderNudgeDown,
    };
}

