// Physical.h
#ifndef __PHYSICAL_H__
#define __PHYSICAL_H__

#include <DirectXMath.h>

namespace Physics
{
    struct Physical
    {
        DirectX::XMFLOAT3 velocity;  // ë¨ìx
        DirectX::XMFLOAT3 force;     // óÕ
        float weight;                 // èdÇ≥
        float damping;                // å∏êä

        Physical()
            : velocity(0.0f, 0.0f, 0.0f)
            , force(0.0f, 0.0f, 0.0f)
            , weight(1.0f)
            , damping(0.98f)
        {
        }
    };
}
#endif