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

    // Pivot policy: align origins to the bottom of local bounds so objects rest on y=0 when spawned there.
    constexpr const char* kPivotPolicyName = "OriginBottom";
    constexpr const char* kPivotPolicyDescription = "Origin=bottom; raise objects by -minY of local bounds scaled by applied scale.";

    PivotInfo ComputePivotInfoOriginBottom(const Model& model, const float3& appliedScale);
    PivotInfo ComputePivotInfoOriginBottom(const std::shared_ptr<Model>& model, const float3& appliedScale);
    float ComputePivotOffsetYOriginBottom(const Model& model, const float3& appliedScale);
    float ComputePivotOffsetYOriginBottom(const std::shared_ptr<Model>& model, const float3& appliedScale);
}
