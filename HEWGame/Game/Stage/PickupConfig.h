// 拾い物の設定内容をまとめている
// ステージ設定の読み込みから参照される

#pragma once

#include "Vector.h"

namespace Stage
{
    enum class PickupOffsetMode
    {
        None,
        FacingDirection,
        WorldX,
    };

    struct PickupOffset
    {
        PickupOffsetMode mode{ PickupOffsetMode::WorldX };
        float3 localOffset{ 0.0f, 0.0f, 0.0f };
    };
}

