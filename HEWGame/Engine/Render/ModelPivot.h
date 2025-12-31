#pragma once

#include <memory>

#include "Render/Model.h"
#include "Vector.h"

namespace Render
{
    struct PivotInfo
    {
        float rawOffsetY{ 0.0f };
        float offsetY{ 0.0f };
        float halfHeight{ 0.0f };
    };

    // ピボット方針：ローカル境界の底面に原点を合わせ、生成時に y=0 に接地させる。
    constexpr const char* kPivotPolicyName = "OriginBottom";
    constexpr const char* kPivotPolicyDescription = "Origin=bottom; raise objects by -minY of local bounds scaled by applied scale.";

    PivotInfo ComputePivotInfoOriginBottom(const Model& model, const float3& appliedScale);
    PivotInfo ComputePivotInfoOriginBottom(const std::shared_ptr<Model>& model, const float3& appliedScale);
    float ComputePivotOffsetYOriginBottom(const Model& model, const float3& appliedScale);
    float ComputePivotOffsetYOriginBottom(const std::shared_ptr<Model>& model, const float3& appliedScale);
}
