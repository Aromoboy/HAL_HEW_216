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
        PickupOffsetMode mode{ PickupOffsetMode::FacingDirection };
        float3 localOffset{ 0.0f, 0.0f, 0.0f };
    };
}

