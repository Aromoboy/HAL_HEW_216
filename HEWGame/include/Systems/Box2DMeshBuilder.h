#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <box2d/box2d.h>

#include "MeshDX.h"

namespace Systems
{
    struct Box2DMeshFixtures
    {
        std::vector<std::vector<b2Vec2>> polygons;
        std::vector<b2Vec2> boundary;
    };

    // Utility for converting render meshes into 2D Box2D fixtures.
    class Box2DMeshBuilder
    {
    public:
        static Box2DMeshBuilder& Instance();

        // Build (or fetch from cache) convex polygons and an outline chain from a model file.
        std::optional<Box2DMeshFixtures> BuildFromModel(const std::string& modelPath);

    private:
        Box2DMeshBuilder() = default;

        Box2DMeshFixtures BuildInternal(const std::string& modelPath) const;

        std::map<std::string, Box2DMeshFixtures> m_cache;
    };

    // Shared polygon extraction helper (also used by tests/harnesses).
    std::vector<std::vector<b2Vec2>> BuildPolygonsFromMesh(const MeshDX::Description& desc,
        std::vector<b2Vec2>* outBoundary = nullptr,
        const std::string& modelPath = {});
}

