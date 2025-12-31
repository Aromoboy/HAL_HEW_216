// エンジンの入力アクションを識別する番号をまとめている
// 入力判定や割り当ての処理から参照される

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
    };
}
