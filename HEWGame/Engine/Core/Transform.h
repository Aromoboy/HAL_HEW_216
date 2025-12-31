#ifndef ___TRANSFORM_H___
#define ___TRANSFORM_H___

#include "Vector.h"

namespace Components
{
        struct Transform
        {
                float3 position;
                float3 rotation;
                float3 scale;
        };
}

using Transform = Components::Transform;

#endif
