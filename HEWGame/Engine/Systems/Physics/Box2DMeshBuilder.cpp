#include "Systems/Physics/Box2DMeshBuilder.h"

#include <algorithm>
#include <cstdint>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <sstream>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

#include "Systems/Physics/Collision/ConvexHullBuilder.h"

namespace
{
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

    constexpr const char* kLogPrefix = "[Box2DMeshBuilder] ";
}

namespace Systems
{
    std::vector<std::vector<b2Vec2>> BuildPolygonsFromMesh(const MeshBuffer::Description& desc, std::vector<b2Vec2>* outBoundary, const std::string& modelPath)
    {
        std::vector<std::vector<b2Vec2>> polygons;
        Collision::Mesh2DExtraction extraction = Collision::ExtractProjectedPolygons(desc);
        if (!extraction.polygons.empty())
        {
            polygons.reserve(extraction.polygons.size());
            for (const auto& polygon : extraction.polygons)
            {
                if (polygon.size() < 3)
                {
                    continue;
                }

                std::vector<b2Vec2> converted;
                converted.reserve(polygon.size());
                for (const float2& point : polygon)
                {
                    converted.push_back(b2Vec2{ point.x, point.y });
                }

                polygons.push_back(std::move(converted));
            }

            if (outBoundary)
            {
                outBoundary->clear();
                outBoundary->reserve(extraction.boundary.size());
                for (const float2& pt : extraction.boundary)
                {
                    outBoundary->push_back(b2Vec2{ pt.x, pt.y });
                }
            }

            return polygons;
        }

        const Collision::HullBuildResult hullResult = Collision::BuildConvexHullFromMesh(desc, {}, modelPath);
        if (!hullResult.hullPointsCentered.empty())
        {
            std::vector<b2Vec2> hull;
            hull.reserve(hullResult.hullPointsCentered.size());
            for (const float2& point : hullResult.hullPointsCentered)
            {
                hull.push_back(b2Vec2{ point.x + hullResult.centroidOffset.x, point.y + hullResult.centroidOffset.y });
            }
            polygons.push_back(std::move(hull));
        }

        return polygons;
    }

    Box2DMeshBuilder& Box2DMeshBuilder::Instance()
    {
        static Box2DMeshBuilder builder;
        return builder;
    }

    std::optional<Box2DMeshFixtures> Box2DMeshBuilder::BuildFromModel(const std::string& modelPath)
    {
        auto it = m_cache.find(modelPath);
        if (it != m_cache.end())
        {
            return it->second;
        }

        Box2DMeshFixtures fixtures = BuildInternal(modelPath);
        if (fixtures.polygons.empty())
        {
            return std::nullopt;
        }

        m_cache.emplace(modelPath, fixtures);
        return fixtures;
    }

    Box2DMeshFixtures Box2DMeshBuilder::BuildInternal(const std::string& modelPath) const
    {
        struct PositionOnly
        {
            float x;
            float y;
            float z;
        };

        Box2DMeshFixtures fixtures{};

        Assimp::Importer importer;
        const unsigned int flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices;
        const aiScene* scene = importer.ReadFile(modelPath.c_str(), flags);
        if (!scene || !scene->HasMeshes())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Failed to load model for Box2D fixture build path='" << modelPath
                << "' reason='" << (scene ? "No meshes" : importer.GetErrorString()) << "'\n";
            DebugTrace(oss.str());
            return fixtures;
        }

        std::vector<PositionOnly> positions;
        std::vector<uint16_t> indices;

        for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
        {
            const aiMesh* mesh = scene->mMeshes[meshIndex];
            if (!mesh || !mesh->HasPositions())
            {
                continue;
            }

            const unsigned int baseIndex = static_cast<unsigned int>(positions.size());
            positions.reserve(positions.size() + mesh->mNumVertices);
            for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
            {
                const aiVector3D pos = mesh->mVertices[v];
                positions.push_back(PositionOnly{ pos.x, pos.y, pos.z });
            }

            const unsigned int numFaces = mesh->mNumFaces;
            for (unsigned int faceIndex = 0; faceIndex < numFaces; ++faceIndex)
            {
                const aiFace& face = mesh->mFaces[faceIndex];
                if (face.mNumIndices < 3)
                {
                    continue;
                }

                // aiProcess_Triangulate guarantees triangles
                indices.push_back(static_cast<uint16_t>(baseIndex + face.mIndices[0]));
                indices.push_back(static_cast<uint16_t>(baseIndex + face.mIndices[1]));
                indices.push_back(static_cast<uint16_t>(baseIndex + face.mIndices[2]));
            }
        }

        if (positions.empty())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "No positions found while building fixtures for '" << modelPath << "'.\n";
            DebugTrace(oss.str());
            return fixtures;
        }

        MeshBuffer::Description desc{};
        desc.pVtx = positions.data();
        desc.vtxSize = sizeof(PositionOnly);
        desc.vtxCount = static_cast<UINT>(positions.size());
        desc.pIdx = indices.empty() ? nullptr : indices.data();
        desc.idxSize = indices.empty() ? 0u : sizeof(uint16_t);
        desc.idxCount = static_cast<UINT>(indices.size());
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        fixtures.polygons = BuildPolygonsFromMesh(desc, &fixtures.boundary, modelPath);

        std::ostringstream oss;
        oss << kLogPrefix << "Built Box2D fixtures from '" << modelPath << "'"
            << " vertices=" << positions.size()
            << " indices=" << indices.size()
            << " polygons=" << fixtures.polygons.size()
            << " boundaryPoints=" << fixtures.boundary.size() << "\n";
        DebugTrace(oss.str());

        return fixtures;
    }
}

