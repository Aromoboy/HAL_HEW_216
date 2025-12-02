#include "Collision/ConvexHullBuilder.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

#include "Debug.h"

namespace
{
    constexpr float kEpsilon = 1e-5f;

#if defined(_WIN32) && defined(_DEBUG)
    void DebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    float Cross(const float2& a, const float2& b, const float2& c)
    {
        const float abx = b.x - a.x;
        const float aby = b.y - a.y;
        const float acx = c.x - a.x;
        const float acy = c.y - a.y;
        return (abx * acy) - (aby * acx);
    }

    bool AlmostEqual(float a, float b, float epsilon = kEpsilon)
    {
        return std::fabs(a - b) <= epsilon;
    }

    bool AlmostEqual(const float2& a, const float2& b, float epsilon = kEpsilon)
    {
        return AlmostEqual(a.x, b.x, epsilon) && AlmostEqual(a.y, b.y, epsilon);
    }

    float2 ComputeCentroid(const std::vector<float2>& points)
    {
        float2 centroid{ 0.0f, 0.0f };
        if (points.empty())
        {
            return centroid;
        }

        for (const auto& p : points)
        {
            centroid.x += p.x;
            centroid.y += p.y;
        }

        const float invCount = 1.0f / static_cast<float>(points.size());
        centroid.x *= invCount;
        centroid.y *= invCount;
        return centroid;
    }

    std::vector<float2> ComputeConvexHull(std::vector<float2> points)
    {
        if (points.size() < 3)
        {
            return points;
        }

        std::sort(points.begin(), points.end(), [](const float2& lhs, const float2& rhs)
            {
                if (!AlmostEqual(lhs.x, rhs.x))
                {
                    return lhs.x < rhs.x;
                }
                return lhs.y < rhs.y;
            });

        points.erase(std::unique(points.begin(), points.end(), [](const float2& a, const float2& b)
            {
                return AlmostEqual(a, b);
            }), points.end());

        if (points.size() < 3)
        {
            return points;
        }

        std::vector<float2> lower;
        std::vector<float2> upper;
        lower.reserve(points.size());
        upper.reserve(points.size());

        for (const auto& p : points)
        {
            while (lower.size() >= 2 && Cross(lower[lower.size() - 2], lower.back(), p) <= 0.0f)
            {
                lower.pop_back();
            }
            lower.push_back(p);
        }

        for (auto it = points.rbegin(); it != points.rend(); ++it)
        {
            while (upper.size() >= 2 && Cross(upper[upper.size() - 2], upper.back(), *it) <= 0.0f)
            {
                upper.pop_back();
            }
            upper.push_back(*it);
        }

        lower.pop_back();
        upper.pop_back();

        lower.insert(lower.end(), upper.begin(), upper.end());
        return lower;
    }
}

namespace Collision
{
    std::vector<float2> BuildConvexHullFromMesh(const MeshBuffer::Description& desc)
    {
        std::vector<float2> extracted;
        if (!desc.pVtx || desc.vtxSize < sizeof(float) * 2 || desc.vtxCount == 0)
        {
#if defined(_DEBUG)
            DebugTrace("[ConvexHullBuilder] Invalid mesh description; skipping hull extraction.\n");
#endif
            return extracted;
        }

        const auto* base = static_cast<const std::byte*>(desc.pVtx);
        extracted.reserve(desc.vtxCount);

        for (UINT i = 0; i < desc.vtxCount; ++i)
        {
            const auto* vertexBytes = base + (static_cast<std::size_t>(i) * desc.vtxSize);
            const float* pos = reinterpret_cast<const float*>(vertexBytes);
            if (!std::isfinite(pos[0]) || !std::isfinite(pos[1]))
            {
                continue;
            }
            extracted.push_back(float2{ pos[0], pos[1] });
        }

        if (extracted.empty())
        {
#if defined(_DEBUG)
            DebugTrace("[ConvexHullBuilder] No finite XY vertices found in mesh; hull cannot be built.\n");
#endif
            return extracted;
        }

        auto uniquePoints = extracted;
        std::sort(uniquePoints.begin(), uniquePoints.end(), [](const float2& lhs, const float2& rhs)
            {
                if (!AlmostEqual(lhs.x, rhs.x))
                {
                    return lhs.x < rhs.x;
                }
                return lhs.y < rhs.y;
            });
        uniquePoints.erase(std::unique(uniquePoints.begin(), uniquePoints.end(), [](const float2& a, const float2& b)
            {
                return AlmostEqual(a, b);
            }), uniquePoints.end());

        auto hull = ComputeConvexHull(uniquePoints);

        const float2 centroid = ComputeCentroid(hull);
        for (auto& p : hull)
        {
            p.x -= centroid.x;
            p.y -= centroid.y;
        }

#if defined(_DEBUG)
        std::ostringstream oss;
        oss << "[ConvexHullBuilder] Hull generated with " << hull.size() << " points from "
            << uniquePoints.size() << " unique XY vertices (" << extracted.size()
            << " raw). Centroid shifted to (" << centroid.x << ", " << centroid.y << ")\n";
        DebugTrace(oss.str());
#endif

        return hull;
    }
}

