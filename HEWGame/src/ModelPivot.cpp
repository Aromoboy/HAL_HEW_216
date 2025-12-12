#include "ModelPivot.h"

#include <algorithm>

namespace
{
    float ComputePivotOffsetYOriginBottomInternal(const Render::Model* model, const float3& appliedScale)
    {
        if (!model || !model->HasLocalBounds())
        {
            return 0.0f;
        }

        const AABB& bounds = model->GetLocalBounds();
        const float minY = std::min(bounds.min.y, bounds.max.y);
        return -minY * appliedScale.y;
    }
}

namespace Render
{
    float ComputePivotOffsetYOriginBottom(const Model& model, const float3& appliedScale)
    {
        return ComputePivotOffsetYOriginBottomInternal(&model, appliedScale);
    }

    float ComputePivotOffsetYOriginBottom(const std::shared_ptr<Model>& model, const float3& appliedScale)
    {
        return ComputePivotOffsetYOriginBottomInternal(model.get(), appliedScale);
    }
}

