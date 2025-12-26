#include "ModelPivot.h"

#include <algorithm>
#include <cmath>

namespace
{
    Render::PivotInfo ComputePivotInfoOriginBottomInternal(const Render::Model* model, const float3& appliedScale)
    {
        if (!model || !model->HasLocalBounds())
        {
            return {};
        }

        const AABB& bounds = model->GetLocalBounds();
        const float minY = std::min(bounds.min.y, bounds.max.y);
        const float maxY = std::max(bounds.min.y, bounds.max.y);
        const float halfHeight = std::abs(maxY - minY) * 0.5f * std::abs(appliedScale.y);

        Render::PivotInfo info{};
        info.rawOffsetY = -minY * appliedScale.y;
        info.halfHeight = halfHeight;
        if (halfHeight > 0.0f)
        {
            info.offsetY = std::clamp(info.rawOffsetY, -halfHeight, halfHeight);
        }
        else
        {
            info.offsetY = info.rawOffsetY;
        }

        return info;
    }
}

namespace Render
{
    PivotInfo ComputePivotInfoOriginBottom(const Model& model, const float3& appliedScale)
    {
        return ComputePivotInfoOriginBottomInternal(&model, appliedScale);
    }

    PivotInfo ComputePivotInfoOriginBottom(const std::shared_ptr<Model>& model, const float3& appliedScale)
    {
        return ComputePivotInfoOriginBottomInternal(model.get(), appliedScale);
    }

    float ComputePivotOffsetYOriginBottom(const Model& model, const float3& appliedScale)
    {
        return ComputePivotInfoOriginBottom(model, appliedScale).offsetY;
    }

    float ComputePivotOffsetYOriginBottom(const std::shared_ptr<Model>& model, const float3& appliedScale)
    {
        return ComputePivotInfoOriginBottom(model, appliedScale).offsetY;
    }
}

