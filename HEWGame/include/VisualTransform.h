#pragma once

#include "Vector.h"

namespace Components
{
    struct VisualTransform
    {
        float3 position{};
        float3 rotation{};
        float3 scale{};
    };
}

using VisualTransform = Components::VisualTransform;

