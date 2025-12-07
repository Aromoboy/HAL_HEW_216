#include "Collision/ConvexHullBuilder.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <unordered_map>

namespace
{
    constexpr float kEpsilon = 1e-5f;

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

    float Dot(const float3& a, const float3& b)
    {
        return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
    }

    float3 Cross(const float3& a, const float3& b)
    {
        return float3{
            (a.y * b.z) - (a.z * b.y),
            (a.z * b.x) - (a.x * b.z),
            (a.x * b.y) - (a.y * b.x)
        };
    }

    float3 Normalize(const float3& v)
    {
        const float lenSq = Dot(v, v);
        if (lenSq <= kEpsilon)
        {
            return float3{ 0.0f, 0.0f, 1.0f };
        }
        const float invLen = 1.0f / std::sqrt(lenSq);
        return float3{ v.x * invLen, v.y * invLen, v.z * invLen };
    }

    float2 Project(const float3& p, const float3& tangent, const float3& bitangent)
    {
        return float2{ Dot(p, tangent), Dot(p, bitangent) };
    }

    float ComputeSignedArea(const std::vector<float2>& polygon)
    {
        if (polygon.size() < 3)
        {
            return 0.0f;
        }

        float area = 0.0f;
        for (size_t i = 0; i < polygon.size(); ++i)
        {
            const float2& p1 = polygon[i];
            const float2& p2 = polygon[(i + 1) % polygon.size()];
            area += (p1.x * p2.y) - (p2.x * p1.y);
        }
        return 0.5f * area;
    }

    bool IsPointInsideTriangle(const float2& p, const float2& a, const float2& b, const float2& c)
    {
        const float area = Cross(a, b, c);
        const float area1 = Cross(p, a, b);
        const float area2 = Cross(p, b, c);
        const float area3 = Cross(p, c, a);

        const bool hasNeg = (area1 < -kEpsilon) || (area2 < -kEpsilon) || (area3 < -kEpsilon);
        const bool hasPos = (area1 > kEpsilon) || (area2 > kEpsilon) || (area3 > kEpsilon);

        if (AlmostEqual(area, 0.0f))
        {
            return false;
        }

        return !(hasNeg && hasPos);
    }

    std::vector<std::vector<float2>> EarClipTriangulate(std::vector<float2> polygon)
    {
        std::vector<std::vector<float2>> triangles;
        if (polygon.size() < 3)
        {
            return triangles;
        }

        if (ComputeSignedArea(polygon) < 0.0f)
        {
            std::reverse(polygon.begin(), polygon.end());
        }

        std::vector<size_t> indices(polygon.size());
        for (size_t i = 0; i < polygon.size(); ++i)
        {
            indices[i] = i;
        }

        auto isConvex = [&](size_t i0, size_t i1, size_t i2)
        {
            const float2& a = polygon[i0];
            const float2& b = polygon[i1];
            const float2& c = polygon[i2];
            return Cross(a, b, c) >= -kEpsilon;
        };

        while (indices.size() >= 3)
        {
            bool earFound = false;
            for (size_t i = 0; i < indices.size(); ++i)
            {
                const size_t iPrev = (i == 0) ? indices.size() - 1 : i - 1;
                const size_t iNext = (i + 1) % indices.size();

                const size_t i0 = indices[iPrev];
                const size_t i1 = indices[i];
                const size_t i2 = indices[iNext];

                if (!isConvex(i0, i1, i2))
                {
                    continue;
                }

                const float2& a = polygon[i0];
                const float2& b = polygon[i1];
                const float2& c = polygon[i2];

                bool containsPoint = false;
                for (size_t j = 0; j < indices.size(); ++j)
                {
                    if (j == iPrev || j == i || j == iNext)
                    {
                        continue;
                    }
                    const float2& p = polygon[indices[j]];
                    if (IsPointInsideTriangle(p, a, b, c))
                    {
                        containsPoint = true;
                        break;
                    }
                }

                if (containsPoint)
                {
                    continue;
                }

                triangles.push_back({ a, b, c });
                indices.erase(indices.begin() + i);
                earFound = true;
                break;
            }

            if (!earFound)
            {
                break;
            }
        }

        return triangles;
    }

    float3 ComputeTangent(const float3& projectionNormal)
    {
        const float3 up{ 0.0f, 1.0f, 0.0f };
        const float3 right{ 1.0f, 0.0f, 0.0f };
        float3 tangent = Cross(projectionNormal, up);
        if (Dot(tangent, tangent) < kEpsilon)
        {
            tangent = Cross(projectionNormal, right);
        }
        return Normalize(tangent);
    }

    float3 ComputeBitangent(const float3& projectionNormal, const float3& tangent)
    {
        return Normalize(Cross(projectionNormal, tangent));
    }

    struct EdgeKey
    {
        UINT a;
        UINT b;

        bool operator==(const EdgeKey& other) const noexcept
        {
            return a == other.a && b == other.b;
        }
    };

    struct EdgeKeyHash
    {
        size_t operator()(const EdgeKey& key) const noexcept
        {
            return (static_cast<size_t>(key.a) << 32) ^ static_cast<size_t>(key.b);
        }
    };

    std::optional<UINT> ReadIndex(const MeshBuffer::Description& desc, UINT index)
    {
        if (!desc.pIdx || desc.idxSize == 0 || desc.idxCount <= index)
        {
            return std::nullopt;
        }

        const auto* base = static_cast<const std::byte*>(desc.pIdx);
        const std::byte* addr = base + static_cast<std::size_t>(index) * desc.idxSize;
        if (desc.idxSize == sizeof(std::uint16_t))
        {
            return static_cast<UINT>(*reinterpret_cast<const std::uint16_t*>(addr));
        }
        if (desc.idxSize == sizeof(std::uint32_t))
        {
            return static_cast<UINT>(*reinterpret_cast<const std::uint32_t*>(addr));
        }
        return std::nullopt;
    }

    std::vector<std::vector<UINT>> BuildBoundaryContours(const MeshBuffer::Description& desc)
    {
        std::vector<std::vector<UINT>> contours;
        if (!desc.pIdx || desc.idxCount < 3)
        {
            return contours;
        }

        std::unordered_map<EdgeKey, int, EdgeKeyHash> edgeUseCounts;
        std::unordered_multimap<UINT, UINT> orientedEdges;

        const UINT triCount = desc.idxCount / 3;
        for (UINT tri = 0; tri < triCount; ++tri)
        {
            const auto i0 = ReadIndex(desc, tri * 3 + 0);
            const auto i1 = ReadIndex(desc, tri * 3 + 1);
            const auto i2 = ReadIndex(desc, tri * 3 + 2);
            if (!i0 || !i1 || !i2)
            {
                continue;
            }

            const EdgeKey edges[3] = {
                EdgeKey{ std::min(*i0, *i1), std::max(*i0, *i1) },
                EdgeKey{ std::min(*i1, *i2), std::max(*i1, *i2) },
                EdgeKey{ std::min(*i2, *i0), std::max(*i2, *i0) }
            };

            for (const auto& edge : edges)
            {
                ++edgeUseCounts[edge];
            }

            orientedEdges.emplace(*i0, *i1);
            orientedEdges.emplace(*i1, *i2);
            orientedEdges.emplace(*i2, *i0);
        }

        std::unordered_multimap<UINT, UINT> boundaryGraph;
        for (const auto& entry : orientedEdges)
        {
            const EdgeKey key{ std::min(entry.first, entry.second), std::max(entry.first, entry.second) };
            const auto it = edgeUseCounts.find(key);
            if (it != edgeUseCounts.end() && it->second == 1)
            {
                boundaryGraph.emplace(entry.first, entry.second);
            }
        }

        std::unordered_multimap<UINT, UINT> remaining = boundaryGraph;
        while (!remaining.empty())
        {
            std::vector<UINT> contour;
            const UINT start = remaining.begin()->first;
            UINT current = start;
            std::unordered_map<UINT, int> visitedCount;

            while (true)
            {
                contour.push_back(current);
                auto range = remaining.equal_range(current);
                if (range.first == range.second)
                {
                    break;
                }

                UINT next = range.first->second;
                remaining.erase(range.first);
                if (++visitedCount[next] > 2)
                {
                    break;
                }

                current = next;
                if (current == start)
                {
                    break;
                }
            }

            if (contour.size() >= 3 && contour.front() == contour.back())
            {
                contour.pop_back();
            }

            if (contour.size() >= 3)
            {
                contours.push_back(contour);
            }
        }

        return contours;
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
    Mesh2DExtraction ExtractProjectedPolygons(const MeshBuffer::Description& desc, const float3& projectionNormal)
    {
        Mesh2DExtraction extraction{};
        if (!desc.pVtx || desc.vtxSize < sizeof(float) * 3 || desc.vtxCount == 0)
        {
            return extraction;
        }

        const float3 basisNormal = Normalize(projectionNormal);
        const float3 tangent = ComputeTangent(basisNormal);
        const float3 bitangent = ComputeBitangent(basisNormal, tangent);

        const auto* base = static_cast<const std::byte*>(desc.pVtx);
        std::vector<float2> projected;
        projected.reserve(desc.vtxCount);

        for (UINT i = 0; i < desc.vtxCount; ++i)
        {
            const auto* vertexBytes = base + (static_cast<std::size_t>(i) * desc.vtxSize);
            const float* pos = reinterpret_cast<const float*>(vertexBytes);
            const float3 position{ pos[0], pos[1], pos[2] };
            projected.push_back(Project(position, tangent, bitangent));
        }

        const auto contours = BuildBoundaryContours(desc);
        if (contours.empty())
        {
            extraction.boundary = projected;
            return extraction;
        }

        float largestArea = std::numeric_limits<float>::lowest();
        for (const auto& contour : contours)
        {
            std::vector<float2> polygon;
            polygon.reserve(contour.size());
            for (UINT index : contour)
            {
                if (index < projected.size())
                {
                    polygon.push_back(projected[index]);
                }
            }

            if (polygon.size() < 3)
            {
                continue;
            }

            const float area = ComputeSignedArea(polygon);
            if (area < 0.0f)
            {
                std::reverse(polygon.begin(), polygon.end());
            }

            const auto triangles = EarClipTriangulate(polygon);
            extraction.polygons.insert(extraction.polygons.end(), triangles.begin(), triangles.end());

            if (std::abs(area) > largestArea)
            {
                largestArea = std::abs(area);
                extraction.boundary = polygon;
            }
        }

        return extraction;
    }

    HullBuildResult BuildConvexHullFromMesh(const MeshBuffer::Description& desc)
    {
        HullBuildResult result{};
        std::vector<float2> extracted;
        Mesh2DExtraction meshExtraction = ExtractProjectedPolygons(desc);
        if (!meshExtraction.boundary.empty())
        {
            extracted = meshExtraction.boundary;
        }
        else if (desc.vtxSize >= sizeof(float) * 2)
        {
            if (!desc.pVtx || desc.vtxCount == 0)
            {
                return result;
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
        }

        if (extracted.empty())
        {
            return result;
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

        const auto hull = ComputeConvexHull(uniquePoints);

        result.centroidOffset = ComputeCentroid(hull);
        result.hullPointsCentered.reserve(hull.size());
        for (const auto& p : hull)
        {
            result.hullPointsCentered.push_back(float2{ p.x - result.centroidOffset.x, p.y - result.centroidOffset.y });
        }

        return result;
    }
}

