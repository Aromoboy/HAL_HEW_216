#pragma once

#include <vector>

#include "AABB.h"
#include "Vector.h"

struct Color
{
        float r;
        float g;
        float b;
        float a;
};

namespace DebugColliderRenderer
{
        void DrawAABB(const AABB& bounds, Color c);
        void DrawHull(const std::vector<float2>& pts, float z, Color c);
        void DrawCircle(float2 center, float radius, float z, Color c);
        void DrawCapsule(float2 p0, float2 p1, float radius, float z, Color c);
}

