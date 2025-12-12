#pragma once

#include <memory>

#include "ModelDX.h"
#include "Vector.h"

namespace Render
{
    // Pivot policy: align origins to the bottom of local bounds so objects rest on y=0 when spawned there.
    constexpr const char* kPivotPolicyName = "OriginBottom";
    constexpr const char* kPivotPolicyDescription = "Origin=bottom; raise objects by -minY of local bounds scaled by applied scale.";

    float ComputePivotOffsetYOriginBottom(const Model& model, const float3& appliedScale);
    float ComputePivotOffsetYOriginBottom(const std::shared_ptr<Model>& model, const float3& appliedScale);
}

