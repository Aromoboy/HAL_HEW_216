#pragma once

#include <algorithm>

namespace BackgroundUV
{
    struct UVWindow
    {
        float u0 = 0.0f;
        float v0 = 0.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
    };

    inline UVWindow ComputeVerticalUVWindow(
        float textureWidth,
        float textureHeight,
        float screenWidth,
        float screenHeight,
        float scrollOffset,
        float parallaxFactor,
        float baseOffset01,
        float pixelsPerUnit)
    {
        UVWindow out{};
        if (textureWidth <= 0.0f || textureHeight <= 0.0f || screenWidth <= 0.0f || screenHeight <= 0.0f)
        {
            return out;
        }

        const float screenAspect = screenWidth / screenHeight;
        const float textureAspect = textureWidth / textureHeight;
        const float scrollWorld = scrollOffset * parallaxFactor;
        const float scrollPx = scrollWorld * pixelsPerUnit;
        const float clampedBaseOffset01 = std::clamp(baseOffset01, 0.0f, 1.0f);

        if (textureAspect > screenAspect)
        {
            const float scaleX = screenAspect / textureAspect;
            if (scaleX <= 0.0f)
            {
                return out;
            }

            const float visibleWidthPx = textureWidth * scaleX;
            const float offsetRangePx = textureWidth - visibleWidthPx;
            const float maxOffsetPx = std::max(0.0f, offsetRangePx);
            const float baseOffsetPx = clampedBaseOffset01 * (textureWidth - visibleWidthPx);
            const float offsetPx = std::clamp(scrollPx + baseOffsetPx, 0.0f, maxOffsetPx);

            float u0 = offsetPx / textureWidth;
            u0 = std::clamp(u0, 0.0f, 1.0f - scaleX);

            out.u0 = u0;
            out.scaleX = scaleX;
            out.scaleY = 1.0f;
        }
        else if (textureAspect < screenAspect)
        {
            const float scaleY = textureAspect / screenAspect;
            if (scaleY <= 0.0f)
            {
                return out;
            }

            const float visibleHeightPx = textureHeight * scaleY;
            const float offsetRangePx = textureHeight - visibleHeightPx;
            const float maxOffsetPx = std::max(0.0f, offsetRangePx);
            const float baseOffsetPx = clampedBaseOffset01 * (textureHeight - visibleHeightPx);
            const float offsetPx = std::clamp(scrollPx + baseOffsetPx, 0.0f, maxOffsetPx);

            float v0 = offsetPx / textureHeight;
            v0 = std::clamp(v0, 0.0f, 1.0f - scaleY);

            out.v0 = v0;
            out.scaleY = scaleY;
        }
        return out;
    }
}
