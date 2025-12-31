#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <box2d/box2d.h>

#include "MeshBuffer.h"

namespace Systems
{
    struct Box2DMeshFixtures
    {
        std::vector<std::vector<b2Vec2>> polygons;
        std::vector<b2Vec2> boundary;
    };

    // レンダーメッシュを 2D Box2D フィクスチャに変換する補助。
    class Box2DMeshBuilder
    {
    public:
        static Box2DMeshBuilder& Instance();

        // モデルから凸ポリゴンと輪郭チェインを生成（またはキャッシュ取得）。
        std::optional<Box2DMeshFixtures> BuildFromModel(const std::string& modelPath);

    private:
        Box2DMeshBuilder() = default;

        Box2DMeshFixtures BuildInternal(const std::string& modelPath) const;

        std::map<std::string, Box2DMeshFixtures> m_cache;
    };

    // ポリゴン抽出の共通ヘルパー（テスト/ハーネスでも使用）。
    std::vector<std::vector<b2Vec2>> BuildPolygonsFromMesh(const MeshBuffer::Description& desc,
        std::vector<b2Vec2>* outBoundary = nullptr,
        const std::string& modelPath = {});
}
