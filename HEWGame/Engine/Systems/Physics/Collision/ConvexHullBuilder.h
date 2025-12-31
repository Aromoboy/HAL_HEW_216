// 点群から凸形状を作る計算をまとめている
// コライダー生成の下準備で使われる

#pragma once

#include <string_view>
#include <vector>

#include "MeshBuffer.h"
#include "Vector.h"

namespace Collision
{
    struct HullBuildResult
    {
        std::vector<float2> hullPointsCentered;
        float2 centroidOffset{ 0.0f, 0.0f };
        bool valid{ false };
    };

    struct Mesh2DExtraction
    {
        std::vector<float2> boundary;
        std::vector<std::vector<float2>> polygons;
    };

    // Extracts a 2D outline and convex polygon set by projecting vertices along the
    // provided normal (defaults to +Z) and rebuilding the boundary from mesh edges.
    // The returned polygons are convex (ear-clipped triangles) that approximate any
    // concave silhouette.
    Mesh2DExtraction ExtractProjectedPolygons(const MeshBuffer::Description& desc, const float3& projectionNormal = float3{ 0.0f, 0.0f, 1.0f });

    // Extracts XY positions from a mesh description and computes a
    // normalized convex hull centered around its centroid for collider use.
    HullBuildResult BuildConvexHullFromMesh(const MeshBuffer::Description& desc,
        std::string_view tag = {}, std::string_view modelPath = {});
}

