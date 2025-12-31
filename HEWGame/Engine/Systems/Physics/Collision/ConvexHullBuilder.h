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

    // 頂点を法線方向（既定は +Z）へ投影し、メッシュエッジから境界を再構築して
    // 2D 輪郭と凸ポリゴン群を抽出する。
    // 返却ポリゴンは凸（耳切り三角形化）で、凹シルエットを近似する。
    Mesh2DExtraction ExtractProjectedPolygons(const MeshBuffer::Description& desc, const float3& projectionNormal = float3{ 0.0f, 0.0f, 1.0f });

    // メッシュ記述から XY 座標を抽出し、重心基準で正規化した凸包を算出して
    // コライダー用途に返す。
    HullBuildResult BuildConvexHullFromMesh(const MeshBuffer::Description& desc,
        std::string_view tag = {}, std::string_view modelPath = {});
}
