// 描画用の位置や回転の情報をまとめている
// モデル描画やカメラ計算から参照される

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

