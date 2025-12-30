#pragma once

#include <limits>

#include "Vector.h"

struct ModelBounds
{
    float3 min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
    float3 max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
    bool valid{ false };
};
