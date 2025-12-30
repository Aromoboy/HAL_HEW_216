#include "Transform.h"
#include "SceneGame.h"
#include "Geometory.h"
#include "DrawMatrix.h"
#include "Vector.h"
#include "ShaderList.h"
#include "Defines.h"
#include "AssimpModelLorder.h"
#include "MeshBuffer.h"
#include "Core/Input.h"
#include "Systems/PhysicsBinder.h"
#include "Systems/DrawSystem.h"
#include "BoxColliderComponent.h"
#include "ModelCache.h"
#include "TextureCache.h"
#include "AnimationCache.h"
#include "DirectX.h"
#include "BaseCamera.h"
#include "PerspectiveCamera.h"
#include "SweetsManager.h"
#include "SweetsPrototypeManifest.h"
#include "Sweets/BaseSweets.h"
#include "Entities/Player.h"
#include "Collision/ConvexHullBuilder.h"
#include "ColliderConfigLoader.h"
#include "DebugColliderRenderer.h"
#include "SceneComposer.h"
#include "SceneGameConfigLoader.h"
#include "SceneGameUIOverlay.h"
#include "GameplayConfig.h"
#include "UIManager.h"
#include "GameplayRules.h"
#include <cassert>
#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <unordered_map>
#include <optional>
#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>
#include <iterator>
#include <DirectXMath.h>
#include "Physical.h"
#include "ColliderRegistry.h"
#include "TitleScene.h"
#include "SceneGameUIOverlay.h"

#if defined(_WIN32)
#       ifndef NOMINMAX
#               define NOMINMAX
#       endif
#       include <windows.h>
#endif

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

        constexpr const char* kLogPrefix = "[SceneComposer] ";
        constexpr bool kEnableDrawSystemVerboseLogging = false;
        constexpr bool kEnablePlayerBoundsLogging = false;
        constexpr bool kEnableStageModelVerboseLogging = false;
        constexpr float kDefaultPlaySeconds = 30.0f;
        constexpr float kCountdownLogIntervalSeconds = 5.0f;
        constexpr float kBaseScorePerMeter = 100.0f;
        constexpr float kPlayerSpeedBoostStep = 1.2f;
        constexpr float kMaxPlayerSpeedMultiplier = 2.0f;
        constexpr float kMinGeneratedThickness = 0.05f;
        constexpr float kPlayerMinimumYMargin = 1.0f;
        constexpr float kPlayerBoundsLogIntervalSeconds = 2.0f;
        constexpr float kGuidelineTimeBonusSeconds = 5.0f;
        constexpr float kGuidelineGlobalCooldownSeconds = 0.15f;
        bool ReadOsaraColliderDebugFlag()
        {
                const char* value = std::getenv("r.DebugOsaraColliders");
                if (!value)
                {
                        return false;
                }

                return std::atoi(value) != 0;
        }

        bool g_RegisterOsaraColliders = ReadOsaraColliderDebugFlag();
        struct ModelBounds
        {
            float3 min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
            float3 max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
            bool valid{ false };
        };

        struct PlayerColliderGeometry
        {
            ModelBounds bounds{};
            std::vector<float2> hull;
            float2 centroidOffset{ 0.0f, 0.0f };
            float rawThickness{ 0.0f };
            float paddedThickness{ 0.0f };
        };

        std::string NormalizeColliderKey(const std::string& key)
        {
            std::string normalized;
            normalized.reserve(key.size());
            for (unsigned char ch : key)
            {
                normalized.push_back(static_cast<char>(std::tolower(ch)));
            }
            return normalized;
        }

        float3 ScaleVector(const float3& value, const float3& scale)
        {
            return float3{ value.x * scale.x, value.y * scale.y, value.z * scale.z };
        }

        Physics::Box2DShapeConfig::Type InferShapeType(const std::vector<Physics::ColliderShapeDescriptor>& shapes)
        {
            if (shapes.empty())
            {
                return Physics::Box2DShapeConfig::Type::None;
            }

            switch (shapes.front().type)
            {
            case Physics::ColliderShapeType::Box:
                return Physics::Box2DShapeConfig::Type::Box;
            case Physics::ColliderShapeType::Polygon:
                return Physics::Box2DShapeConfig::Type::Polygon;
            case Physics::ColliderShapeType::Circle:
                return Physics::Box2DShapeConfig::Type::Circle;
            default:
                return Physics::Box2DShapeConfig::Type::None;
            }
        }

#if defined(_DEBUG)
        Color MakeColor(float r, float g, float b, float a)
        {
            return Color{ r, g, b, a };
        }

        float2 ApplyTransform2D(const float2& local, const Transform& transform)
        {
            const float scaledX = local.x * transform.scale.x;
            const float scaledY = local.y * transform.scale.y;
            const float cosR = std::cos(transform.rotation.z);
            const float sinR = std::sin(transform.rotation.z);

            return float2{
                transform.position.x + (cosR * scaledX - sinR * scaledY),
                transform.position.y + (sinR * scaledX + cosR * scaledY),
            };
        }

        void DrawColliderOverlay(const Entities::PhysicalEntity& entity, const Color& color)
        {
            b2Transform bodyTransform{};
            std::vector<b2ShapeId> shapes;
            if (!entity.CollectBox2DShapes(shapes, bodyTransform))
            {
                return;
            }

            const float z = entity.GetTransform().position.z;

            const auto toWorldFloat2 = [](const b2Vec2& v)
            {
                const float3 world = Entities::PhysicalEntity::ToWorldVector(v);
                return float2{ world.x, world.y };
            };

            for (const b2ShapeId shapeId : shapes)
            {
                switch (b2Shape_GetType(shapeId))
                {
                case b2_circleShape:
                {
                    const b2Circle circle = b2Shape_GetCircle(shapeId);
                    const b2Vec2 worldCenter = b2TransformPoint(bodyTransform, circle.center);
                    const float2 center = toWorldFloat2(worldCenter);
                    const float radius = Entities::PhysicalEntity::ToWorldLength(circle.radius);
                    DebugColliderRenderer::DrawCircle(center, radius, z, color);
                    break;
                }
                case b2_polygonShape:
                {
                    const b2Polygon polygon = b2Shape_GetPolygon(shapeId);
                    if (polygon.count < 2)
                    {
                        break;
                    }

                    std::vector<float2> hull;
                    hull.reserve(static_cast<size_t>(polygon.count));

                    for (int i = 0; i < polygon.count; ++i)
                    {
                        const b2Vec2 worldVertex = b2TransformPoint(bodyTransform, polygon.vertices[i]);
                        hull.push_back(toWorldFloat2(worldVertex));
                    }

                    DebugColliderRenderer::DrawHull(hull, z, color);
                    break;
                }
                case b2_capsuleShape:
                {
                    const b2Capsule capsule = b2Shape_GetCapsule(shapeId);
                    const b2Vec2 worldP0 = b2TransformPoint(bodyTransform, capsule.center1);
                    const b2Vec2 worldP1 = b2TransformPoint(bodyTransform, capsule.center2);
                    const float radius = Entities::PhysicalEntity::ToWorldLength(capsule.radius);
                    DebugColliderRenderer::DrawCapsule(toWorldFloat2(worldP0), toWorldFloat2(worldP1), radius, z, color);
                    break;
                }
                default:
                    break;
                }
            }
        }

        void DrawModelBoundsOverlay(const Entities::PhysicalEntity& entity, const Color& color)
        {
            b2AABB bodyAabb{};
            if (entity.GetBox2DBodyAABB(bodyAabb))
            {
                const float3 lower = Entities::PhysicalEntity::ToWorldVector(bodyAabb.lowerBound);
                const float3 upper = Entities::PhysicalEntity::ToWorldVector(bodyAabb.upperBound);
                const float2 corners[4] = {
                    float2{ lower.x, lower.y },
                    float2{ upper.x, lower.y },
                    float2{ upper.x, upper.y },
                    float2{ lower.x, upper.y },
                };

                std::vector<float2> worldCorners;
                worldCorners.reserve(4u);
                for (const auto& corner : corners)
                {
                    worldCorners.push_back(corner);
                }

                DebugColliderRenderer::DrawHull(worldCorners, entity.GetTransform().position.z, color);
                return;
            }

            const Render::Model* model = entity.GetRenderableModel();
            if (!model || !model->HasLocalBounds())
            {
                return;
            }

            const Transform& transform = entity.GetTransform();
            const AABB bounds = model->GetLocalBounds();
            const float2 corners[4] = {
                float2{ bounds.min.x, bounds.min.y },
                float2{ bounds.max.x, bounds.min.y },
                float2{ bounds.max.x, bounds.max.y },
                float2{ bounds.min.x, bounds.max.y },
            };

            std::vector<float2> worldCorners;
            worldCorners.reserve(4u);
            for (const auto& corner : corners)
            {
                worldCorners.push_back(ApplyTransform2D(corner, transform));
            }

            DebugColliderRenderer::DrawHull(worldCorners, transform.position.z, color);
        }
#endif

        PlayerColliderGeometry AnalyzePlayerModelGeometry(const std::string& path, float thicknessPadding)
        {
            PlayerColliderGeometry summary{};

            struct PositionOnly
            {
                float x;
                float y;
                float z;
            };

            const float sanitizedPadding = std::max(thicknessPadding, 1.0f);
            std::vector<PositionOnly> positions;
            std::vector<UINT> indices;

            Assimp::Importer importer;
            const unsigned int flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices;
            const aiScene* scene = importer.ReadFile(path.c_str(), flags);
            if (!scene || !scene->HasMeshes())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Failed to load model for geometry analysis path='" << path
                    << "' reason='" << (scene ? "No meshes" : importer.GetErrorString()) << "'\n";
                DebugTrace(oss.str());
                return summary;
            }

            const std::string::size_type dot = path.find_last_of('.');
            const std::string format = dot != std::string::npos ? path.substr(dot + 1) : std::string("unknown");

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

                    summary.bounds.min.x = std::min(summary.bounds.min.x, pos.x);
                    summary.bounds.min.y = std::min(summary.bounds.min.y, pos.y);
                    summary.bounds.min.z = std::min(summary.bounds.min.z, pos.z);

                    summary.bounds.max.x = std::max(summary.bounds.max.x, pos.x);
                    summary.bounds.max.y = std::max(summary.bounds.max.y, pos.y);
                    summary.bounds.max.z = std::max(summary.bounds.max.z, pos.z);
                    summary.bounds.valid = true;
                }

                const unsigned int numFaces = mesh->mNumFaces;
                for (unsigned int faceIndex = 0; faceIndex < numFaces; ++faceIndex)
                {
                    const aiFace& face = mesh->mFaces[faceIndex];
                    if (face.mNumIndices < 3)
                    {
                        continue;
                    }

                    // aiProcess_Triangulateは三角形を保証
                    indices.push_back(baseIndex + face.mIndices[0]);
                    indices.push_back(baseIndex + face.mIndices[1]);
                    indices.push_back(baseIndex + face.mIndices[2]);
                }
            }

            if (summary.bounds.valid)
            {
                summary.rawThickness = std::max(summary.bounds.max.z - summary.bounds.min.z, 0.0f);
                summary.paddedThickness = summary.rawThickness * sanitizedPadding;
            }
            else
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] No vertex positions parsed from model path='" << path << "'; bounds invalid.\n";
                DebugTrace(oss.str());
            }

            if (!positions.empty())
            {
                MeshBuffer::Description desc{};
                desc.pVtx = positions.data();
                desc.vtxSize = sizeof(PositionOnly);
                desc.vtxCount = static_cast<UINT>(positions.size());
                desc.pIdx = indices.empty() ? nullptr : indices.data();
                desc.idxSize = indices.empty() ? 0u : sizeof(UINT);
                desc.idxCount = static_cast<UINT>(indices.size());
                desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

                const auto hullResult = Collision::BuildConvexHullFromMesh(desc, "Player", path);
                summary.centroidOffset = hullResult.centroidOffset;
                summary.hull = hullResult.hullPointsCentered;
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Geometry analyzed path='" << path
                    << "' format=" << format
                    << " vertices=" << positions.size()
                    << " indices=" << indices.size()
                    << " hullPoints=" << summary.hull.size()
                    << " rawThickness=" << summary.rawThickness
                    << " paddedThickness=" << summary.paddedThickness
                    << " paddingFactor=" << sanitizedPadding << "\n";
                DebugTrace(oss.str());
            }
            else
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Geometry analysis found no positions for '" << path << "'.\n";
                DebugTrace(oss.str());
            }

            return summary;
        }

        std::vector<float2> BuildPaddedHull(const PlayerColliderGeometry& geometry, const SceneGame::PlayerColliderTuning& tuning, const float3& scale)
        {
            std::vector<float2> hull;
            hull.reserve(geometry.hull.empty() ? 4u : geometry.hull.size());

            if (geometry.hull.empty())
            {
                const float3 scaledMin = ScaleVector(geometry.bounds.min, scale);
                const float3 scaledMax = ScaleVector(geometry.bounds.max, scale);
                const float halfWidth = ((scaledMax.x - scaledMin.x) * 0.5f) + tuning.padding.x;
                const float halfHeight = ((scaledMax.y - scaledMin.y) * 0.5f) + tuning.padding.y;
                hull.push_back(float2{ -halfWidth, -halfHeight });
                hull.push_back(float2{ halfWidth, -halfHeight });
                hull.push_back(float2{ halfWidth, halfHeight });
                hull.push_back(float2{ -halfWidth, halfHeight });
            }
            else
            {
                for (auto p : geometry.hull)
                {
                    float2 scaled{ p.x * scale.x, p.y * scale.y };
                    if (scaled.x >= 0.0f)
                    {
                        scaled.x += tuning.padding.x;
                    }
                    else
                    {
                        scaled.x -= tuning.padding.x;
                    }
                    if (scaled.y >= 0.0f)
                    {
                        scaled.y += tuning.padding.y;
                    }
                    else
                    {
                        scaled.y -= tuning.padding.y;
                    }
                    hull.push_back(scaled);
                }
            }

            return hull;
        }

        struct PlayerColliderBuildResult
        {
            std::optional<Physics::BoxColliderComponent> collider;
            std::optional<Physics::Box2DShapeConfig> shapeConfig;
            float3 halfExtents{ 0.0f, 0.0f, 0.0f };
            float3 centerOffset{ 0.0f, 0.0f, 0.0f };
            std::string source{ "Unknown" };
            std::string shapeSource{ "None" };
            bool registryHit{ false };
            bool registryHasShapes{ false };
        };

        struct FootprintAccumulator
        {
            float minX{ std::numeric_limits<float>::max() };
            float minY{ std::numeric_limits<float>::max() };
            float maxX{ std::numeric_limits<float>::lowest() };
            float maxY{ std::numeric_limits<float>::lowest() };
            float halfZ{ 0.0f };
            bool valid{ false };

            void IncludeVertex(const b2Vec2& v)
            {
                minX = std::min(minX, v.x);
                minY = std::min(minY, v.y);
                maxX = std::max(maxX, v.x);
                maxY = std::max(maxY, v.y);
                valid = true;
            }

            void IncludeVertices(const std::vector<b2Vec2>& vertices)
            {
                for (const auto& v : vertices)
                {
                    IncludeVertex(v);
                }
            }

            void IncludeCircle(const b2Vec2& center, float radius)
            {
                if (!(radius > 0.0f))
                {
                    return;
                }

                IncludeVertex(b2Vec2{ center.x - radius, center.y - radius });
                IncludeVertex(b2Vec2{ center.x + radius, center.y + radius });
            }

            void ExpandHalfZ(float thickness)
            {
                if (std::isfinite(thickness) && thickness > 0.0f)
                {
                    halfZ = std::max(halfZ, thickness * 0.5f);
                }
            }

            float3 Center(float zOffset) const
            {
                if (!valid)
                {
                    return float3{ 0.0f, 0.0f, zOffset };
                }

                return float3{ (minX + maxX) * 0.5f, (minY + maxY) * 0.5f, zOffset };
            }

            float3 HalfExtents(float minHalfZ) const
            {
                float3 hext{ 0.0f, 0.0f, std::max(halfZ, minHalfZ) };
                if (valid)
                {
                    hext.x = (maxX - minX) * 0.5f;
                    hext.y = (maxY - minY) * 0.5f;
                }
                return hext;
            }
        };

        PlayerColliderBuildResult BuildPlayerColliderFromRegistry(
            const std::string& colliderId,
            const Transform& transform,
            const SceneGame::PlayerColliderTuning& tuning,
            b2BodyType bodyType)
        {
            PlayerColliderBuildResult result{};
            result.source = "RegistryMissing";

            const auto registryDefinition = Physics::ColliderRegistry::Instance().Find(colliderId);
            result.registryHit = registryDefinition.has_value();
            result.registryHasShapes = registryDefinition && !registryDefinition->shapes.empty();

            if (!registryDefinition)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Registry definition missing for id='" << colliderId
                    << "'; unable to build player collider from registry.\n";
                DebugTrace(oss.str());
                return result;
            }

            if (!result.registryHasShapes)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Registry collider '" << colliderId
                    << "' has no shapes; collider will not be created.\n";
                DebugTrace(oss.str());
                result.source = "RegistryEmpty";
                return result;
            }

            Physics::BoxColliderComponent collider{};
            collider.colliderId = colliderId;
            collider.shapeDef = registryDefinition->shapeDef;
            collider.shapes = registryDefinition->shapes;
            const float3 scaledOffset = ScaleVector(tuning.offset, transform.scale);
            collider.colliderOffset = b2Vec2{ scaledOffset.x, scaledOffset.y };

            Physics::Box2DShapeConfig shapeConfig{};
            shapeConfig.bodyType = bodyType;
            shapeConfig.fixedRotation = true;
            shapeConfig.allowRotation = false;
            shapeConfig.angularDamping = 5.0f;

            FootprintAccumulator footprint{};
            const float paddedThickness = std::max(
                kMinGeneratedThickness,
                (tuning.fallbackSize.z + tuning.fallbackPadding.z * 2.0f) * transform.scale.z);
            footprint.ExpandHalfZ(paddedThickness);

            const float uniformScale = std::max(transform.scale.x, transform.scale.y);
            for (const auto& shape : collider.shapes)
            {
                switch (shape.type)
                {
                case Physics::ColliderShapeType::Circle:
                {
                    const float radius = shape.radius * uniformScale;
                    const b2Vec2 center = shape.vertices.empty()
                        ? b2Vec2{ 0.0f, 0.0f }
                        : b2Vec2{ shape.vertices.front().x * transform.scale.x, shape.vertices.front().y * transform.scale.y };
                    footprint.IncludeCircle(center, radius);
                    break;
                }
                case Physics::ColliderShapeType::Box:
                case Physics::ColliderShapeType::Polygon:
                default:
                {
                    std::vector<b2Vec2> vertices;
                    vertices.reserve(shape.vertices.size());
                    for (const auto& vertex : shape.vertices)
                    {
                        vertices.push_back(b2Vec2{ vertex.x * transform.scale.x, vertex.y * transform.scale.y });
                    }
                    footprint.IncludeVertices(vertices);
                    break;
                }
                }
            }

            shapeConfig.type = InferShapeType(collider.shapes);
            if (shapeConfig.type == Physics::Box2DShapeConfig::Type::None)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Registry collider '" << colliderId
                    << "' does not declare a supported shape type.\n";
                DebugTrace(oss.str());
                return result;
            }

            result.centerOffset = footprint.Center(scaledOffset.z);
            result.centerOffset.x += scaledOffset.x;
            result.centerOffset.y += scaledOffset.y;
            result.halfExtents = footprint.HalfExtents(kMinGeneratedThickness * 0.5f);

            shapeConfig.density = std::max(0.0f, registryDefinition->shapeDef.density);
            shapeConfig.friction = std::max(0.0f, registryDefinition->shapeDef.material.friction);
            shapeConfig.restitution = std::clamp(registryDefinition->shapeDef.material.restitution, 0.0f, 1.0f);
            shapeConfig.offset = collider.colliderOffset;

            result.collider = collider;
            result.shapeConfig = shapeConfig;
            result.source = "Registry";
            result.shapeSource = "Registry";
            return result;
        }

        PlayerColliderBuildResult BuildPlayerColliderFromConfig(
            const std::string& resolvedKey,
            const Transform& transform,
            const SceneGame::PlayerColliderTuning& tuning,
            const std::map<std::string, Collision::ColliderDefinition>& definitions,
            b2BodyType bodyType)
        {
            PlayerColliderBuildResult result{};

            Physics::Box2DShapeConfig shapeConfig{};
            shapeConfig.type = Physics::Box2DShapeConfig::Type::None;
            shapeConfig.bodyType = bodyType;
            shapeConfig.fixedRotation = true;
            shapeConfig.allowRotation = false;
            shapeConfig.angularDamping = 5.0f;

            const auto registryDefinition = Physics::ColliderRegistry::Instance().Find(resolvedKey);
            result.registryHit = registryDefinition.has_value();
            result.registryHasShapes = registryDefinition && !registryDefinition->shapes.empty();

            const auto configIt = definitions.find(resolvedKey);
            if (configIt == definitions.end())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Definition missing for key='" << resolvedKey
                    << "'; model-based generation is disabled; player collider cannot be created from config.\n";
                DebugTrace(oss.str());
                result.source = "ConfigMissing";
                return result;
            }

            const Collision::ColliderDefinition& configDef = configIt->second;

            Physics::BoxColliderComponent collider{};
            collider.colliderId = resolvedKey;
            if (registryDefinition)
            {
                collider.shapeDef = registryDefinition->shapeDef;
            }
            const float3 scaledOffset = ScaleVector(tuning.offset, transform.scale);
            collider.colliderOffset = b2Vec2{ scaledOffset.x, scaledOffset.y };

            FootprintAccumulator footprint{};
            const auto appendDefinition = [&](const Collision::ColliderDefinition& def, const b2Vec2& parentOffset, float parentScale, const auto& self) -> void
            {
                const float combinedScale = std::max(0.0f, def.scale) * parentScale;
                const b2Vec2 scaledOffset{
                    parentOffset.x + def.offset.x * transform.scale.x,
                    parentOffset.y + def.offset.y * transform.scale.y
                };

                const float marginX = def.margin2 ? def.margin2->x : (def.margin3 ? def.margin3->x : def.margin);
                const float marginY = def.margin2 ? def.margin2->y : (def.margin3 ? def.margin3->y : def.margin);

                switch (def.type)
                {
                case Collision::ColliderKind::Circle:
                {
                    float radius = def.radius * combinedScale * std::max(transform.scale.x, transform.scale.y);
                    float marginRadius = def.margin;
                    if (def.margin2)
                    {
                        marginRadius = std::max(def.margin2->x, def.margin2->y);
                    }
                    if (def.margin3)
                    {
                        marginRadius = std::max(marginRadius, std::max(def.margin3->x, def.margin3->y));
                    }
                    radius = std::max(kMinGeneratedThickness * 0.5f, radius + marginRadius);

                    b2Vec2 center = scaledOffset;
                    Physics::ColliderShapeDescriptor shape{};
                    shape.type = Physics::ColliderShapeType::Circle;
                    shape.radius = radius;
                    shape.vertices.push_back(center);
                    collider.shapes.push_back(shape);
                    footprint.IncludeCircle(center, radius);

                    float thickness = (def.thickness + (def.margin3 ? def.margin3->z * 2.0f : 0.0f)) * transform.scale.z;
                    thickness = std::max(thickness, kMinGeneratedThickness);
                    footprint.ExpandHalfZ(thickness + tuning.padding.z * 2.0f);
                    break;
                }
                case Collision::ColliderKind::Hull:
                {
                    std::vector<b2Vec2> vertices;
                    vertices.reserve(def.points.size());
                    for (auto p : def.points)
                    {
                        float vx = p.x * combinedScale * transform.scale.x;
                        float vy = p.y * combinedScale * transform.scale.y;
                        if (marginX != 0.0f)
                        {
                            vx += (vx >= 0.0f) ? marginX : -marginX;
                        }
                        if (marginY != 0.0f)
                        {
                            vy += (vy >= 0.0f) ? marginY : -marginY;
                        }

                        vertices.push_back(b2Vec2{ scaledOffset.x + vx, scaledOffset.y + vy });
                    }

                    collider.AddPolygon(vertices);
                    footprint.IncludeVertices(vertices);

                    float thickness = (def.thickness + (def.margin3 ? def.margin3->z * 2.0f : 0.0f)) * transform.scale.z;
                    thickness = std::max(thickness, kMinGeneratedThickness);
                    footprint.ExpandHalfZ(thickness + tuning.padding.z * 2.0f);
                    break;
                }
                case Collision::ColliderKind::Compound:
                default:
                {
                    for (const auto& child : def.children)
                    {
                        self(child, scaledOffset, combinedScale, self);
                    }
                    break;
                }
                }
            };

            appendDefinition(configDef, b2Vec2{ 0.0f, 0.0f }, 1.0f, appendDefinition);

            if (collider.shapes.empty())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Config definition for '" << resolvedKey
                    << "' produced no shapes; player collider cannot be created.\n";
                DebugTrace(oss.str());
                result.source = "ConfigEmpty";
                return result;
            }

            shapeConfig.type = InferShapeType(collider.shapes);
            if (shapeConfig.type == Physics::Box2DShapeConfig::Type::None)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Config definition for '" << resolvedKey
                    << "' lacks a supported shape type.\n";
                DebugTrace(oss.str());
                result.source = "ConfigInvalid";
                return result;
            }

            if (registryDefinition)
            {
                shapeConfig.density = std::max(0.0f, registryDefinition->shapeDef.density);
                shapeConfig.friction = std::max(0.0f, registryDefinition->shapeDef.material.friction);
                shapeConfig.restitution = std::clamp(registryDefinition->shapeDef.material.restitution, 0.0f, 1.0f);
            }

            result.centerOffset = footprint.Center(scaledOffset.z);
            result.centerOffset.x += scaledOffset.x;
            result.centerOffset.y += scaledOffset.y;
            result.halfExtents = footprint.HalfExtents(kMinGeneratedThickness * 0.5f);
            shapeConfig.offset = collider.colliderOffset;

            result.collider = collider;
            result.shapeConfig = shapeConfig;
            result.source = registryDefinition ? "Config" : "ConfigNoRegistry";
            result.shapeSource = "Config";
            return result;
        }

        PlayerColliderBuildResult BuildFallbackPlayerCollider(
            const std::string& resolvedKey,
            const Transform& transform,
            const SceneGame::PlayerColliderTuning& tuning,
            b2BodyType bodyType)
        {
            PlayerColliderBuildResult result{};
            result.source = "Fallback";

            Physics::BoxColliderComponent collider{};
            collider.colliderId = resolvedKey;

            const float3 scaledSize = ScaleVector(tuning.fallbackSize, transform.scale);
            const float3 scaledPadding = ScaleVector(tuning.fallbackPadding, transform.scale);
            const float3 scaledOffset = ScaleVector(tuning.fallbackOffset, transform.scale);

            const float halfWidth = std::max(kMinGeneratedThickness * 0.5f, scaledSize.x * 0.5f + scaledPadding.x);
            const float halfHeight = std::max(kMinGeneratedThickness * 0.5f, scaledSize.y * 0.5f + scaledPadding.y);

            collider.AddBox(halfWidth, halfHeight);
            collider.colliderOffset = b2Vec2{ scaledOffset.x, scaledOffset.y };

            Physics::Box2DShapeConfig shapeConfig{};
            shapeConfig.bodyType = bodyType;
            shapeConfig.fixedRotation = true;
            shapeConfig.allowRotation = false;
            shapeConfig.angularDamping = 5.0f;
            shapeConfig.type = InferShapeType(collider.shapes);
            shapeConfig.offset = collider.colliderOffset;

            result.centerOffset = float3{ scaledOffset.x, scaledOffset.y, scaledOffset.z };
            result.halfExtents = float3{
                halfWidth,
                halfHeight,
                std::max(kMinGeneratedThickness * 0.5f, scaledSize.z * 0.5f + scaledPadding.z)
            };
            result.collider = collider;
            result.shapeConfig = shapeConfig;
            result.shapeSource = "Fallback";
            return result;
        }

        DirectX::XMVECTOR LoadFloat3(const float3& value)
        {
            return DirectX::XMVectorSet(value.x, value.y, value.z, 0.0f);
        }

        float3 StoreFloat3(DirectX::FXMVECTOR value)
        {
            DirectX::XMFLOAT3 temp;
            DirectX::XMStoreFloat3(&temp, value);
            return float3{ temp.x, temp.y, temp.z };
        }

        DirectX::XMVECTOR RotateOffset(const float3& offset, float yaw, float pitch)
        {
            DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, 0.0f);
            return DirectX::XMVector3Transform(LoadFloat3(offset), rotation);
        }

        constexpr CameraPrototypeSettings kFixedSideViewCameraPreset
        {
            true,
            0.0f,
            //カメラ設定
            float3{ 0.0f, 5.5f, -25.0f },
            45.0f,
            DirectX::XMConvertToRadians(0.0f),
            DirectX::XMConvertToRadians(-10.0f)
        };

        constexpr float kStageCenterX = 0.0f;
        constexpr float kCameraStateEpsilon = 1e-4f;

        bool HasMeaningfulDifference(float previous, float current, float epsilon = kCameraStateEpsilon)
        {
            if (!std::isfinite(previous) || !std::isfinite(current))
            {
                return true;
            }
            return std::fabs(previous - current) > epsilon;
        }

        bool HasMeaningfulDifference(const float3& a, const float3& b, float epsilon = kCameraStateEpsilon)
        {
            return HasMeaningfulDifference(a.x, b.x, epsilon)
                || HasMeaningfulDifference(a.y, b.y, epsilon)
                || HasMeaningfulDifference(a.z, b.z, epsilon);
        }

        Transform BuildPlatformTransformFromDescriptor(
            const Stage::PlatformDescriptor& descriptor,
            float stageCenterX)
        {
            Transform result{};
            result.position = float3{
                stageCenterX + descriptor.center.x,
                descriptor.center.y,
                descriptor.center.z };
            result.rotation = float3{ 0.0f, 0.0f, 0.0f };
            result.scale = float3{
                descriptor.widthWorld,
                descriptor.heightWorld,
                std::max(0.01f, descriptor.depthWorld)
            };
            return result;
        }
}

float3 fRads = { 0.0f, 0.0f, 0.0f };

void SceneGame::Load(Core::Input& input)
{
    (void)input;
    std::ostringstream oss;
    oss << kLogPrefix << "Load scenePtr=" << this << "\n";
    ::DebugTrace(oss.str());
}

void SceneGame::Update(Core::StepTimer& timer, Core::Input& input)
{
    RootUpdate(timer, input);
}

void SceneGame::Draw()
{
    RootDraw();
}

void SceneGame::OnEnter()
{
    std::ostringstream oss;
    oss << kLogPrefix << "Enter scenePtr=" << this << "\n";
    ::DebugTrace(oss.str());

    // BGMを再生（ループあり、音量40%）
    if (m_pManager)
    {
        m_pManager->PlaySoundEX("Assets/Audio/BGM_MainGame.wav", true, 0.4f);  // true = ループ
    }
}

void SceneGame::OnExit()
{
    std::ostringstream oss;
    oss << kLogPrefix << "Exit scenePtr=" << this << "\n";
    ::DebugTrace(oss.str());
}

std::string SceneGame::ResolvePlayerColliderKey(const std::string& key) const
{
    const auto& manifest = SweetsSpace::SweetsPrototypeManifest::Instance();
    const auto resolvedType = manifest.Resolve(key);
    if (resolvedType)
    {
        const auto* descriptor = manifest.Find(*resolvedType);
        if (descriptor)
        {
            return NormalizeColliderKey(descriptor->key);
        }
    }

    return NormalizeColliderKey(key);
}

void SceneGame::NormalizePlayerColliderDefinitions()
{
    std::map<std::string, Collision::ColliderDefinition> normalizedDefinitions;

    for (const auto& entry : m_playerColliderDefinitions)
    {
        const std::string canonicalKey = ResolvePlayerColliderKey(entry.first);
        normalizedDefinitions[canonicalKey] = entry.second;
    }

    m_playerColliderDefinitions.swap(normalizedDefinitions);
}

SceneGame::SceneGame(SceneSpace::SceneManager& a_manager)
    : m_manager(&a_manager)
{
    Render::ModelCache& cache = Render::ModelCache::Instance();
    Render::AnimationCache& animeCache = Render::AnimationCache::Instance();

    m_pManager = std::make_unique<SoundManagerEX>();
    m_loader = std::make_shared<AssimpModelLorder>();

    ID3D11Device* device = GetDevice();
    if (!device)
    {
        DebugTrace(std::string(kLogPrefix) + "Failed to configure ModelCache: device was null.\n");
    }
    else
    {
        cache.SetLoader(m_loader, device, TextureCache::Instance());
        animeCache.SetLoader(m_loader, device);
    }

    SceneComposer composer(GetUpdateScheduler());
    SceneComposer::Composition composition = composer.Compose();
    m_playerColliderDefinitions = std::move(composition.playerColliderDefinitions);
    m_playerColliderConfigLoaded = composition.playerColliderConfigLoaded;
    m_forceConfigPlayerCollider = composition.forceConfigPlayerCollider;
    m_playerColliderTuning = composition.playerColliderTuning;
    m_playerSpawnTransform = composition.playerSpawnTransform;
    m_stageData = std::move(composition.stageData);
    m_phaseManager = std::move(composition.phaseManager);
    m_playerBaseHorizontalSpeed = composition.playerBaseHorizontalSpeed;
    m_playerBaseVerticalSpeed = composition.playerBaseVerticalSpeed;
    m_playerPickupBoxWidth = composition.playerPickupBoxWidth;
    m_playerPickupBoxHeight = composition.playerPickupBoxHeight;
    m_playerPickupOffset = composition.playerPickupOffset;
    m_playerPickupMaxDistance = composition.playerPickupMaxDistance;
    m_playerPickupDepthThreshold = composition.playerPickupDepthThreshold;
    m_playerSpeedMultiplier = composition.playerSpeedMultiplier;
    m_guidelineHeightMeters = composition.guidelineHeightMeters;
    m_guidelineStepMeters = composition.guidelineStepMeters;
    m_guidelineThicknessPixels = composition.guidelineThicknessPixels;
    m_guidelineWidthPaddingWorld = composition.guidelineWidthPaddingWorld;
    m_guidelineZOffset = composition.guidelineZOffset;
    m_guidelineCooldownDurationSeconds = composition.guidelineCooldownDurationSeconds;
    m_guidelineCooldownRemainingSeconds = composition.guidelineCooldownRemainingSeconds;
    m_guidelineCenterX = composition.guidelineCenterX;
    m_guidelineWidthWorld = composition.guidelineWidthWorld;
    m_guidelineThicknessWorld = composition.guidelineThicknessWorld;
    m_heightBonusThresholdMeters = composition.heightBonusThresholdMeters;
    m_heightBonusSeconds = composition.heightBonusSeconds;
    m_physicsBinder = std::move(composition.physicsBinder);
    m_drawSystem = std::move(composition.drawSystem);
    m_guidelineActor = std::move(composition.guidelineActor);
    m_sweetsManager = std::move(composition.sweetsManager);
    m_uiManager = std::move(composition.uiManager);
    m_backgroundRenderer = std::move(composition.backgroundRenderer);
    m_stageObjects = std::move(composition.stageObjects);

    if (m_physicsBinder)
    {
        m_physicsBinder->AddContactHandler(
            [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b)
            {
                if (m_sweetsManager)
                {
                    m_sweetsManager->OnContactBegin(&a, &b);
                }
            },
            [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b)
            {
                if (m_sweetsManager)
                {
                    m_sweetsManager->OnContactEnd(&a, &b);
                }
            });
    }

    if (m_sweetsManager)
    {
        m_sweetsManager->SetPhaseChangeCallback([this](int newPhase)
        {
            ForwardSweetsPhaseChange(newPhase);
        });
    }

    NormalizePlayerColliderDefinitions();

    std::string playerColliderKey = "Player";
    m_playerColliderMismatchLogged = false;
    Systems::PhysicsMachine* physicsMachine = m_physicsBinder ? m_physicsBinder->GetPhysicsMachine() : nullptr;
    m_uiOverlay = std::make_unique<SceneGameUIOverlay>();
    if (m_uiOverlay && !m_uiOverlay->Initialize())
    {
        m_uiOverlay.reset();
    }

    RenderTarget* pRTV = GetDefaultRTV();
    DepthStencil* pDSV = GetDefaultDSV();
    SetRenderTargets(1, &pRTV, pDSV);
    SetDepthTest(true);
    m_cameraAnchor = std::make_shared<Entities::Entity>("PlayerAnchor");

    const auto& cameraSettings = kFixedSideViewCameraPreset;
    m_gameCamera = std::make_shared<CameraSpace::PerspectiveCamera>(cameraSettings.fovDegrees);
    auto registeredCamera = AddCamera(m_gameCamera);
    if (!registeredCamera)
    {
        DebugTrace(std::string(kLogPrefix) + "Failed to register gameplay camera.\n");
    }
    else
    {
        SetActiveCamera(registeredCamera);
    }

    if (m_gameCamera)
    {
        auto baseCamera = std::static_pointer_cast<CameraSpace::BaseCamera>(m_gameCamera);
        m_cameraRig = std::make_unique<CameraRig>(baseCamera);
        ConfigureCamera(cameraSettings);
        if (m_cameraAnchor)
        {
            m_cameraRig->Attach(m_cameraAnchor);
        }
    }

    auto& manifest = SweetsSpace::SweetsPrototypeManifest::Instance();
    const auto* fairyDescriptor = manifest.Find("Fairy");
    if (!fairyDescriptor && !m_playerModel)
    {
        DebugTrace(std::string(kLogPrefix) + "[ModelLoad] Fairy descriptor missing; using procedural fallback.\n");
    }

    const std::string primaryModelPath = fairyDescriptor ? fairyDescriptor->modelPath : std::string{};
    playerColliderKey = fairyDescriptor ? fairyDescriptor->key : std::string("Fairy");
    std::string resolvedModelPath = primaryModelPath;
    if (!primaryModelPath.empty())
    {
        m_playerModel = cache.Get(primaryModelPath);
        if (!m_playerModel)
        {
            DebugTrace(std::string(kLogPrefix) + "[ModelLoad] Primary fairy model missing; player will use procedural fallback.\n");
        }
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "[ModelLoad] Fairy model path missing; player will use procedural fallback.\n");
    }


    if (m_playerModel)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[ModelLoad] Player model ready path='" << resolvedModelPath
            << "' refCount=" << m_playerModel.use_count() << "\n";
        ::DebugTrace(oss.str());
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "[ModelLoad] Failed to resolve any fairy model; player will use procedural fallback.\n");
    }

    
    

    m_render = std::make_unique<ModelRender>();
    if (m_drawSystem && m_render)
    {
        m_drawSystem->SetRenderer(m_render.get());
    }
    std::optional<Physics::BoxColliderComponent> boxColliderComponent;
    std::optional<Physics::Box2DShapeConfig> playerShapeConfig;
    float3 playerColliderHalfExtents{ 0.0f, 0.0f, 0.0f };
    float3 playerColliderOffset{ 0.0f, 0.0f, 0.0f };
    Transform trans = m_playerSpawnTransform.transform;

    const bool playerIsKinematic = false;
    const b2BodyType playerBodyType = playerIsKinematic ? b2_kinematicBody : b2_dynamicBody;

    const std::string resolvedColliderKey = NormalizeColliderKey(ResolvePlayerColliderKey(playerColliderKey));
    const bool hasConfigDefinition = m_playerColliderDefinitions.find(resolvedColliderKey) != m_playerColliderDefinitions.end();
    const bool playerConfigAvailable = m_playerColliderConfigLoaded && hasConfigDefinition;

    const bool forceConfigCollider = m_forceConfigPlayerCollider && playerConfigAvailable;
    PlayerColliderBuildResult colliderBuild = forceConfigCollider
        ? BuildPlayerColliderFromConfig(
            resolvedColliderKey,
            trans,
            m_playerColliderTuning,
            m_playerColliderDefinitions,
            playerBodyType)
        : BuildPlayerColliderFromRegistry(
            resolvedColliderKey,
            trans,
            m_playerColliderTuning,
            playerBodyType);
    const auto colliderHasShapes = [](const PlayerColliderBuildResult& build)
    {
        return build.collider && !build.collider->Empty();
    };
    const auto shapeConfigIsValid = [](const PlayerColliderBuildResult& build)
    {
        return build.shapeConfig && build.shapeConfig->type != Physics::Box2DShapeConfig::Type::None;
    };

    bool attemptedConfigFallback = false;
    bool attemptedRegistryFallback = false;
    bool attemptedFallback = false;
    if (!colliderHasShapes(colliderBuild) || !shapeConfigIsValid(colliderBuild))
    {
        if (forceConfigCollider)
        {
            attemptedRegistryFallback = true;
            std::ostringstream oss;
            oss << kLogPrefix << "[PlayerCollider] Config build unavailable; attempting registry fallback for key='"
                << resolvedColliderKey << "'.\n";
            DebugTrace(oss.str());

            PlayerColliderBuildResult registryBuild = BuildPlayerColliderFromRegistry(
                resolvedColliderKey,
                trans,
                m_playerColliderTuning,
                playerBodyType);

            if (colliderHasShapes(registryBuild) && shapeConfigIsValid(registryBuild))
            {
                colliderBuild = registryBuild;
            }
        }
        else if (playerConfigAvailable)
        {
            attemptedConfigFallback = true;
            std::ostringstream oss;
            oss << kLogPrefix << "[PlayerCollider] Registry build unavailable; attempting config fallback for key='"
                << resolvedColliderKey << "'.\n";
            DebugTrace(oss.str());

            PlayerColliderBuildResult configBuild = BuildPlayerColliderFromConfig(
                resolvedColliderKey,
                trans,
                m_playerColliderTuning,
                m_playerColliderDefinitions,
                playerBodyType);

            if (colliderHasShapes(configBuild) && shapeConfigIsValid(configBuild))
            {
                colliderBuild = configBuild;
            }
        }
    }

    if (!colliderHasShapes(colliderBuild) || !shapeConfigIsValid(colliderBuild))
    {
        attemptedFallback = true;
        std::ostringstream oss;
        oss << kLogPrefix << "[PlayerCollider] Registry/config build failed; using fallback collider for key='"
            << resolvedColliderKey << "'.\n";
        DebugTrace(oss.str());

        PlayerColliderBuildResult fallbackBuild = BuildFallbackPlayerCollider(
            resolvedColliderKey,
            trans,
            m_playerColliderTuning,
            playerBodyType);
        colliderBuild = fallbackBuild;
    }

    if ((!colliderHasShapes(colliderBuild) || !shapeConfigIsValid(colliderBuild)) && playerConfigAvailable)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[PlayerCollider] Collider definition for key='" << resolvedColliderKey
            << "' is invalid; physics registration will be skipped.\n";
        DebugTrace(oss.str());
    }

    if (colliderBuild.collider && !colliderBuild.collider->Empty())
    {
        boxColliderComponent = colliderBuild.collider;
    }
    if (colliderBuild.shapeConfig && colliderBuild.shapeConfig->type != Physics::Box2DShapeConfig::Type::None)
    {
        playerShapeConfig = colliderBuild.shapeConfig;
    }
    playerColliderHalfExtents = colliderBuild.halfExtents;
    playerColliderOffset = colliderBuild.centerOffset;
    if (playerColliderHalfExtents.x > 0.0f && playerColliderHalfExtents.y > 0.0f)
    {
        m_playerPickupBoxWidth = playerColliderHalfExtents.x * 2.0f;
        m_playerPickupBoxHeight = playerColliderHalfExtents.y * 2.0f;
    }

    const bool colliderReady = colliderHasShapes(colliderBuild) && shapeConfigIsValid(colliderBuild);
    const std::string appliedSource = colliderBuild.shapeSource;
    const std::string appliedPath = colliderBuild.shapeSource;

    {
        std::ostringstream oss;
        const auto shapeToString = [](Physics::Box2DShapeConfig::Type type)
        {
            switch (type)
            {
            case Physics::Box2DShapeConfig::Type::Box: return "Box";
            case Physics::Box2DShapeConfig::Type::Polygon: return "Polygon";
            case Physics::Box2DShapeConfig::Type::Circle: return "Circle";
            default: return "None";
            }
        };

        oss << kLogPrefix << "[PlayerCollider] source=" << colliderBuild.source
            << " shapeSource=" << colliderBuild.shapeSource
            << " appliedSource=" << appliedSource
            << " appliedPath=" << appliedPath
            << " key='" << resolvedColliderKey << "' forceConfigRequested=" << std::boolalpha << m_forceConfigPlayerCollider
            << " shapePolicy=" << (forceConfigCollider ? "ConfigPrimary" : "RegistryPrimary")
            << " modelGeneration=Disabled"
            << " configLoaded=" << m_playerColliderConfigLoaded
            << " configAvailable=" << hasConfigDefinition
            << " registryHit=" << colliderBuild.registryHit
            << " registryShapes=" << colliderBuild.registryHasShapes
            << " configFallback=" << std::boolalpha << attemptedConfigFallback
            << " registryFallback=" << std::boolalpha << attemptedRegistryFallback
            << " fallback=" << attemptedFallback
            << " applied=" << colliderReady
            << " shapeCount=" << (boxColliderComponent ? boxColliderComponent->shapes.size() : 0u)
            << " shape=" << (playerShapeConfig ? shapeToString(playerShapeConfig->type) : "None")
            << " offset=(" << playerColliderOffset.x << ',' << playerColliderOffset.y << ',' << playerColliderOffset.z << ')'
            << " halfExtents=(" << playerColliderHalfExtents.x << ',' << playerColliderHalfExtents.y << ',' << playerColliderHalfExtents.z << ')'
            << " pickupBox=(" << m_playerPickupBoxWidth << ',' << m_playerPickupBoxHeight << ")\n";
        DebugTrace(oss.str());
    }

    m_playerPickupOriginOffset = playerColliderOffset;
    if (!std::isfinite(m_playerPickupOriginOffset.x) || !std::isfinite(m_playerPickupOriginOffset.y) || !std::isfinite(m_playerPickupOriginOffset.z))
    {
        m_playerPickupOriginOffset = float3{ 0.0f, 0.0f, 0.0f };
    }

    player = std::make_unique<Entities::Player>(
        trans,
        m_playerModel,
        physicsMachine,
        m_sweetsManager.get(),
        Collision::CollisionLayer::Player,
        playerIsKinematic,
        Core::UpdatePhase::Gameplay,
        0,
        "player");

    if (player)
    {
        if (m_physicsBinder)
        {
            m_physicsBinder->SetPlayer(player.get());
        }
        if (boxColliderComponent)
        {
            player->SetBoxColliderComponent(boxColliderComponent);
        }
        if (playerShapeConfig)
        {
            player->SetBox2DShapeConfig(playerShapeConfig);
        }
        if (playerColliderHalfExtents.x > 0.0f && playerColliderHalfExtents.y > 0.0f && playerColliderHalfExtents.z > 0.0f)
        {
            player->SetColliderFootprint(playerColliderHalfExtents, playerColliderOffset);
        }
        player->SetBaseMoveSpeed(m_playerBaseHorizontalSpeed, m_playerBaseVerticalSpeed);
        player->SetSpeedMultiplier(m_playerSpeedMultiplier);
        player->SetPickupArea(m_playerPickupBoxWidth, m_playerPickupBoxHeight, m_playerPickupOffset);
        player->SetPickupMaxDistance(m_playerPickupMaxDistance);
        player->SetPickupDepthThreshold(m_playerPickupDepthThreshold);
        player->SetPickupOriginOffset(m_playerPickupOriginOffset);
        if (colliderReady)
        {
            if (m_physicsBinder)
            {
                m_physicsBinder->RegisterPhysicsBody(*player);
            }
            {
                std::ostringstream oss;
                const auto configuredBodyType = playerShapeConfig ? playerShapeConfig->bodyType : playerBodyType;
                const b2BodyType createdBodyType = player->GetPhysical().GetBodyType();
                const auto bodyTypeToString = [](b2BodyType type)
                {
                    switch (type)
                    {
                    case b2_staticBody: return "static";
                    case b2_kinematicBody: return "kinematic";
                    case b2_dynamicBody: return "dynamic";
                    default: return "unknown";
                    }
                };

                const bool typeMatches = configuredBodyType == createdBodyType;
                oss << kLogPrefix << "Player physics body registered (expected="
                    << bodyTypeToString(configuredBodyType)
                    << ", created=" << bodyTypeToString(createdBodyType)
                    << ", match=" << std::boolalpha << typeMatches << ").\n";
                ::DebugTrace(oss.str());
            }
        }
        else
        {
            const auto shapeToString = [](Physics::Box2DShapeConfig::Type type)
            {
                switch (type)
                {
                case Physics::Box2DShapeConfig::Type::Box: return "Box";
                case Physics::Box2DShapeConfig::Type::Polygon: return "Polygon";
                case Physics::Box2DShapeConfig::Type::Circle: return "Circle";
                default: return "None";
                }
            };
            std::ostringstream oss;
            oss << kLogPrefix << "[PlayerCollider] Skipping physics registration; colliderReady="
                << std::boolalpha << colliderReady
                << " appliedSource=" << appliedSource
                << " shapes=" << (boxColliderComponent ? boxColliderComponent->shapes.size() : 0u)
                << " shapeType=" << (playerShapeConfig ? shapeToString(playerShapeConfig->type) : "None")
                << (attemptedConfigFallback ? " (config fallback attempted)" : "")
                << (attemptedFallback ? " (fallback attempted)" : "")
                << "\n";
            ::DebugTrace(oss.str());
        }
        if (m_drawSystem)
        {
            m_drawSystem->RegisterRenderable(*player);
        }
    }

    m_countdownTimer.Reset();
    m_totalPlaySeconds = kDefaultPlaySeconds;
    m_remainingCountdownSeconds = m_totalPlaySeconds;
    m_lastAnnouncedCountdownSeconds = static_cast<int>(std::ceil(m_totalPlaySeconds));
}

SceneGame::~SceneGame()
{
    if (m_uiOverlay)
    {
        m_uiOverlay->Shutdown();
        m_uiOverlay.reset();
    }

    if (m_physicsBinder)
    {
        m_physicsBinder->UnregisterAllPhysicsBodies();
    }
    if (m_inputRegistered)
    {
        if (auto* input = GetInput())
        {
            Unregister(*input);
        }
        m_inputRegistered = false;
    }

    if (m_physicsRegistered && m_physicsBinder)
    {
        if (auto* physicsMachine = m_physicsBinder->GetPhysicsMachine())
        {
            GetUpdateScheduler().Unregister(*physicsMachine);
            m_physicsRegistered = false;
        }
    }

    if (m_cameraRigRegistered && m_cameraRig)
    {
        Unregister(*m_cameraRig);
        m_cameraRigRegistered = false;
    }

    if (m_drawRegistered && m_drawSystem)
    {
        GetUpdateScheduler().Unregister(*m_drawSystem);
        m_drawRegistered = false;
    }

    if (m_sweetsLateRegistered && m_sweetsManager)
    {
        if (auto* lateNode = m_sweetsManager->GetLateUpdateNode())
        {
            GetUpdateScheduler().Unregister(*lateNode);
        }
        m_sweetsLateRegistered = false;
    }

    if (m_sweetsRegistered && m_sweetsManager)
    {
        GetUpdateScheduler().Unregister(*m_sweetsManager);
        m_sweetsRegistered = false;
    }

    if (player)
    {
        if (m_drawSystem)
        {
            m_drawSystem->UnregisterRenderable(*player);
        }
        Unregister(*player);
    }

    if (m_drawSystem)
    {
        m_drawSystem->ClearRenderables();
    }
        if (m_sweetsRegistered && m_sweetsManager)
        {
                GetUpdateScheduler().Unregister(*m_sweetsManager);
                m_sweetsRegistered = false;
        }

        if (m_pManager)
        {
            m_pManager->StopAllSounds();
        }
}

void SceneGame::ConfigureUpdates()
{
        if (m_physicsBinder && !m_physicsRegistered)
        {
                if (auto* physicsMachine = m_physicsBinder->GetPhysicsMachine())
                {
                        GetUpdateScheduler().Register(*physicsMachine);
                        m_physicsRegistered = true;

                        std::ostringstream oss;
                        oss << kLogPrefix << "ConfigureUpdates registered PhysicsMachine (phase="
                            << static_cast<int>(physicsMachine->GetPhase())
                            << ", priority=" << physicsMachine->GetPriority() << ").\n";
                        ::DebugTrace(oss.str());
                }
        }

        if (auto* input = GetInput())
        {
                Register(*input);
                m_inputRegistered = true;
        }

        RegisterSceneUpdateNode(Core::UpdatePhase::Gameplay, 0);

        if (m_drawSystem && !m_drawRegistered)
        {
                GetUpdateScheduler().Register(*m_drawSystem);
                m_drawRegistered = true;
        }

        if (m_cameraRig && !m_cameraRigRegistered)
        {
                Register(*m_cameraRig);
                m_cameraRigRegistered = true;
        }

        if (m_sweetsManager && !m_sweetsRegistered)
        {
                GetUpdateScheduler().Register(*m_sweetsManager);
                m_sweetsRegistered = true;
        }

        if (m_sweetsManager && !m_sweetsLateRegistered)
        {
                if (auto* lateNode = m_sweetsManager->GetLateUpdateNode())
                {
                        GetUpdateScheduler().Register(*lateNode);
                        m_sweetsLateRegistered = true;
                }
        }


        if (player)
        {
                Register(*player);

                if (m_physicsBinder)
                {
                        m_physicsBinder->RegisterPhysicsBody(*player);
                        std::ostringstream oss;
                        const bool isKinematic = player->IsKinematic();
                        oss << kLogPrefix << "ConfigureUpdates ensured player body is registered (kinematic="
                            << std::boolalpha << isKinematic << ").\n";
                        ::DebugTrace(oss.str());
                }

                const int playerFixtureCount = player->GetBoxColliderComponent()
                    ? player->GetBoxColliderComponent()->createdFixtureCount
                    : 0;
                if (!player->HasBox2DBody() || playerFixtureCount <= 0)
                {
                        DebugTrace(std::string(kLogPrefix) + "Player Box2D body missing after registration; attempting repair.\n");
                        if (m_physicsBinder)
                        {
                            m_physicsBinder->RegisterPhysicsBody(*player);
                        }

                        const int repairedFixtures = player->GetBoxColliderComponent()
                            ? player->GetBoxColliderComponent()->createdFixtureCount
                            : 0;
                        if (!player->HasBox2DBody() || repairedFixtures <= 0)
                        {
                                DebugTrace(std::string(kLogPrefix) + "[Fatal] Player physics body unavailable; pickup input may fail.\n");
                        }
                }
        }

        SetStageState(StageState::Playing);
}

void SceneGame::Update(Core::UpdateContext& ctx)
{
#if defined(_DEBUG)
    HandleColliderDebugCommands(ctx);
#endif


    
    if (m_physicsBinder)
    {
        m_physicsBinder->ValidatePlayerPhysicsRegistration();
    }
    const GameplayProgression::CountdownInput countdownInput{
        m_stageState == StageState::Playing,
        ctx.dt,
        &m_countdownTimer,
        kCountdownLogIntervalSeconds,
        DebugTrace,
        kLogPrefix
    };
    GameplayProgression::CountdownState countdownState{
        m_totalPlaySeconds,
        m_remainingCountdownSeconds,
        m_lastAnnouncedCountdownSeconds,
        m_pendingTimeBonuses
    };
    GameplayProgression::CountdownResult countdownResult =
        GameplayProgression::UpdateCountdown(countdownInput, std::move(countdownState));
    m_totalPlaySeconds = countdownResult.state.totalSeconds;
    m_remainingCountdownSeconds = countdownResult.state.remainingSeconds;
    m_lastAnnouncedCountdownSeconds = countdownResult.state.lastAnnouncedSeconds;
    m_pendingTimeBonuses = std::move(countdownResult.state.pendingBonuses);
    if (countdownResult.timeUp && m_stageState == StageState::Playing)
    {
        SetStageState(StageState::TimeUp);
    }
    if (m_guidelineCooldownRemainingSeconds > 0.0f)
    {
        m_guidelineCooldownRemainingSeconds = std::max(0.0f, m_guidelineCooldownRemainingSeconds - ctx.dt);
    }

    if (m_guidelineTriggerCooldownSeconds > 0.0f)
    {
        m_guidelineTriggerCooldownSeconds = std::max(0.0f, m_guidelineTriggerCooldownSeconds - ctx.dt);
    }

#if defined(_DEBUG)
    const auto& stacked = m_sweetsManager->GetByState(SweetsSpace::SweetsState::Stacked);
    for (const auto& sweets : stacked)
    {
        if (!sweets) continue;
        const AABB bounds = sweets->GetAABB();
        ::DebugTrace("[DEBUG] sweetsTop=" + std::to_string(bounds.max.y) + "\n");
    }
#endif

    //高さボーナス判定
    const GameplayProgression::TowerTelemetryInput towerTelemetryInput{
        m_sweetsManager.get(),
        &m_phaseManager,
        m_clearPhaseCount,
        m_remainingCountdownSeconds,
        DebugTrace,
        kLogPrefix
    };
    GameplayProgression::TowerTelemetryState towerTelemetryState{
        m_currentTowerHeight,
        m_peakTowerHeight,
        m_maxTowerHeight,
        m_phaseUpCount,
        m_isGameClear
    };
    GameplayProgression::TowerTelemetryResult towerTelemetryResult =
        GameplayProgression::UpdateTowerHeightTelemetry(towerTelemetryInput, std::move(towerTelemetryState));
    m_currentTowerHeight = towerTelemetryResult.state.currentHeight;
    m_peakTowerHeight = towerTelemetryResult.state.peakHeight;
    m_maxTowerHeight = towerTelemetryResult.state.maxHeight;
    m_phaseUpCount = towerTelemetryResult.state.phaseUpCount;
    m_isGameClear = towerTelemetryResult.state.isGameClear;
    if (towerTelemetryResult.phaseAdvanced && towerTelemetryResult.currentPhase > towerTelemetryResult.previousPhase)
    {
        BroadcastPhaseAdvance(towerTelemetryResult.currentPhase);
    }

    
    float deltaTime = ctx.timer.DeltaTime();
    for (auto& obj : m_stageObjects)
    {
        if (obj.spinEnabled)
        {
            obj.currentRotation += obj.spinSpeed * deltaTime;
        }
    }

    // =========================================
    // SIMPLE & CORRECT PLAYER WORLD-BOUNDS
    // =========================================

    if (player && m_gameCamera)
    {
        const float playerZ = player->GetTransform().position.z;
        const float3 camPos = m_gameCamera->GetPosition();

        // Distance camera → player plane
        float d = std::fabs(camPos.z - playerZ);
        if (d < 0.001f) d = 0.001f;

        // Camera params
        const float fovY = DirectX::XMConvertToRadians(m_gameCamera->GetFieldOfViewDegrees());
        const float aspect = m_gameCamera->GetAspectRatio();
        const float fovX = 2.0f * std::atan(std::tan(fovY * 0.5f) * aspect);

        // Visible half-width
        const float halfWidth = d * std::tan(fovX * 0.5f);
        const float halfHeight = d * std::tan(fovY * 0.5f);

        // World bounds on X
        const float minX = camPos.x - halfWidth;
        const float maxX = camPos.x + halfWidth;
        const float minY = camPos.y - halfHeight - kPlayerMinimumYMargin;

        // Log bounds if changed
        const bool changed =
            !m_cachedPlayerBoundsValid ||
            HasMeaningfulDifference(m_cachedPlayerMinX, minX) ||
            HasMeaningfulDifference(m_cachedPlayerMaxX, maxX);

        if (kEnablePlayerBoundsLogging)
        {
            static float playerBoundsLogCooldownSeconds = 0.0f;
            if (playerBoundsLogCooldownSeconds > 0.0f)
            {
                playerBoundsLogCooldownSeconds = std::max(0.0f, playerBoundsLogCooldownSeconds - ctx.dt);
            }

            if (changed && playerBoundsLogCooldownSeconds <= 0.0f)
            {
                std::ostringstream oss;
                oss << "[SceneGame] [PlayerBounds] camX=" << camPos.x
                    << " d=" << d
                    << " fovX=" << DirectX::XMConvertToDegrees(fovX)
                    << " halfWidth=" << halfWidth
                    << " => minX=" << minX
                    << " maxX=" << maxX << "\n";
                ::DebugTrace(oss.str());
                playerBoundsLogCooldownSeconds = kPlayerBoundsLogIntervalSeconds;
            }
        }

        // Apply to player
        if (player)
        {
            player->SetScreenBounds(minX, maxX, std::nullopt);
            player->SetMinimumY(minY);
        }

        // Apply to sweets manager
        if (m_sweetsManager)
            m_sweetsManager->SetHorizontalBounds(minX, maxX);

        // Cache
        m_cachedPlayerMinX = minX;
        m_cachedPlayerMaxX = maxX;
        m_cachedPlayerBoundsValid = true;



    }

    switch (m_stageState)
    {
    case StageState::Intro:
        break;
    case StageState::Playing:
    {
        Core::Input& input = ctx.input;
        if (player)
        {
            const auto adjustMoveSpeed = [&](float requestedMultiplier, const char* reason)
            {
                if (!(m_playerBaseHorizontalSpeed > 0.0f) && !(m_playerBaseVerticalSpeed > 0.0f))
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[SpeedBoost] " << reason
                        << " ignored; base speeds invalid (" << m_playerBaseHorizontalSpeed
                        << ',' << m_playerBaseVerticalSpeed << ").\n";
                    ::DebugTrace(oss.str());
                    return;
                }

                const float currentMultiplier = m_playerSpeedMultiplier;
                const float clampedMultiplier =
                    std::clamp(requestedMultiplier, 0.0f, kMaxPlayerSpeedMultiplier);
                const bool clamped =
                    std::fabs(clampedMultiplier - requestedMultiplier)
                    > std::numeric_limits<float>::epsilon();

                if (std::fabs(currentMultiplier - clampedMultiplier)
                    <= std::numeric_limits<float>::epsilon())
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[SpeedBoost] " << reason
                        << " requestedMultiplier=" << requestedMultiplier
                        << " has no effect (current=" << currentMultiplier << ").\n";
                    ::DebugTrace(oss.str());
                    return;
                }

                const float previousHorizontal = m_playerBaseHorizontalSpeed * currentMultiplier;
                const float previousVertical   = m_playerBaseVerticalSpeed * currentMultiplier;

                const float newMultiplier = clampedMultiplier;
                m_playerSpeedMultiplier = newMultiplier;
                player->SetSpeedMultiplier(newMultiplier);

                const float newHorizontalSpeed = m_playerBaseHorizontalSpeed * newMultiplier;
                const float newVerticalSpeed   = m_playerBaseVerticalSpeed * newMultiplier;

                std::ostringstream oss;
                oss << kLogPrefix << "[SpeedBoost] " << reason
                    << " baseXY=(" << m_playerBaseHorizontalSpeed << ',' << m_playerBaseVerticalSpeed << ')'
                    << " prevMultiplier=" << currentMultiplier
                    << " newMultiplier=" << newMultiplier
                    << " prevSpeedXY=(" << previousHorizontal << ',' << previousVertical << ')'
                    << " newSpeedXY=(" << newHorizontalSpeed << ',' << newVerticalSpeed << ')'
                    << " clamped=" << std::boolalpha << clamped << "\n";
                ::DebugTrace(oss.str());
            };

            if (input.IsTrigger(Core::ActionId::SpeedBoost))
            {
                adjustMoveSpeed(m_playerSpeedMultiplier * kPlayerSpeedBoostStep, "Boost key '1'");
            }

            if (input.IsTrigger(Core::ActionId::SpeedReset))
            {
                adjustMoveSpeed(1.0f, "Reset key '2'");
            }
        }


        HandleManualRetryInput(ctx);
        fRads.z += 0.001f;

        GameplayProgression::GuidelineState guidelineState{
            m_guidelineHeightMeters,
            m_guidelineStepMeters,
            m_guidelineCooldownRemainingSeconds,
            m_guidelineCooldownDurationSeconds,
            m_guidelineTriggerCooldownSeconds,
            m_guidelineTriggeredThisHeight
        };
        const GameplayProgression::GuidelineInput guidelineInput{
            m_sweetsManager.get(),
            m_guidelineActor.get(),
            m_guidelineCenterX,
            kGuidelineTimeBonusSeconds,
            kGuidelineGlobalCooldownSeconds,
            DebugTrace,
            kLogPrefix
        };

        if (m_guidelineTriggerCooldownSeconds <= 0.0f)
        {
            GameplayProgression::GuidelineCheckResult guidelineCheck =
                GameplayProgression::CheckGuidelineCrossing(guidelineInput, std::move(guidelineState));
            guidelineState = std::move(guidelineCheck.state);
            if (guidelineCheck.crossed)
            {
                GameplayProgression::GuidelineAdvanceResult guidelineAdvance =
                    GameplayProgression::HandleGuidelineAdvance(guidelineInput,
                                                                std::move(guidelineState),
                                                                std::move(m_pendingTimeBonuses),
                                                                guidelineCheck.triggerHeight);
                guidelineState = std::move(guidelineAdvance.state);
                m_pendingTimeBonuses = std::move(guidelineAdvance.pendingBonuses);
            }
        }

        m_guidelineHeightMeters = guidelineState.heightMeters;
        m_guidelineStepMeters = guidelineState.stepMeters;
        m_guidelineCooldownRemainingSeconds = guidelineState.cooldownRemainingSeconds;
        m_guidelineCooldownDurationSeconds = guidelineState.cooldownDurationSeconds;
        m_guidelineTriggerCooldownSeconds = guidelineState.triggerCooldownSeconds;
        m_guidelineTriggeredThisHeight = std::move(guidelineState.triggeredThisHeight);
        break;
    }
    case StageState::TimeUp:
    case StageState::Cleared:
        SetStageState(StageState::ShowingResult);
        break;
    case StageState::ShowingResult:
        break;
    }

    if (m_stageState == StageState::ShowingResult)
    {
        ProcessPostGameInput(ctx);
    }

    if (m_uiOverlay)
    {
        const bool isTimeUp = (m_stageState == StageState::TimeUp)
            || (m_stageState == StageState::Cleared)
            || (m_stageState == StageState::ShowingResult);

        UIManager::HUDUpdateData uiData{};
        uiData.hud.remainingSeconds = std::max(0.0f, m_remainingCountdownSeconds);
        uiData.hud.currentHeight = m_currentTowerHeight;
        uiData.hud.peakHeight = m_peakTowerHeight;
        const GameplayRules::ScoreInput scoreInput{
            &m_phaseManager,
            kBaseScorePerMeter,
            DebugTrace,
            kLogPrefix
        };
        GameplayRules::ScoreState scoreState{
            m_peakTowerHeight,
            m_lastScoreLogValue,
            m_lastScoreLogMultiplier
        };
        GameplayRules::ScoreResult scoreResult = GameplayRules::ComputeScore(scoreInput, scoreState);
        m_lastScoreLogValue = scoreResult.state.lastScoreLogValue;
        m_lastScoreLogMultiplier = scoreResult.state.lastScoreLogMultiplier;
        uiData.hud.score = scoreResult.score;
        uiData.hud.sweetsCount = GetSweetsCount();
        uiData.hud.timeUp = isTimeUp;
        uiData.hud.moveSpeedMultiplier = m_playerSpeedMultiplier;
        if (player)
        {
            uiData.hud.moveSpeedUnitsPerSecond = player->GetMoveSpeed();
        }

        uiData.result.remainingSeconds = std::max(0.0f, m_remainingCountdownSeconds);
        uiData.result.currentHeight = m_currentTowerHeight;
        uiData.result.peakHeight = m_peakTowerHeight;
        uiData.result.score = scoreResult.score;
        uiData.result.sweetsCount = GetSweetsCount();
        uiData.result.timeUp = isTimeUp;
        uiData.result.moveSpeedMultiplier = m_playerSpeedMultiplier;
        if (player)
        {
            uiData.result.moveSpeedUnitsPerSecond = player->GetMoveSpeed();
        }

        uiData.showHud = g_DebugDrawEnabled && !isTimeUp;
        uiData.showResult = isTimeUp;
        m_uiOverlay->Update(uiData);
    }

    if (ctx.input.IsKeyPress(VK_RETURN))
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<TitleScene>(*m_manager));
        }

        return;
    }

}


void SceneGame::ResetGuidelineHeight()
{
    m_guidelineHeightMeters = m_stageData.GetGuidelineHeight();
    m_guidelineStepMeters = m_stageData.GetGuidelineStep();
    if (m_guidelineActor)
    {
        auto& transform = m_guidelineActor->GetTransform();
        transform.position.x = m_guidelineCenterX;
        transform.position.y = m_guidelineHeightMeters;
    }
}


#if defined(_DEBUG)
void SceneGame::HandleColliderDebugCommands(Core::UpdateContext& ctx)
{
        if (!player)
        {
                return;
        }

        Core::Input& input = ctx.input;
        if (input.IsTrigger(Core::ActionId::DebugToggle))
        {
                g_DebugDrawEnabled = !g_DebugDrawEnabled;
                std::ostringstream oss;
                oss << kLogPrefix << "[DebugDraw] r.DebugDrawEnabled="
                        << (g_DebugDrawEnabled ? 1 : 0) << " (F5).\n";
                DebugTrace(oss.str());
        }

        if (input.IsTrigger(VK_F4))
        {
                g_DrawColliders = !g_DrawColliders;
                std::ostringstream oss;
                oss << kLogPrefix << "[ColliderDebug] r.DebugColliders="
                        << (g_DrawColliders ? 1 : 0) << " (F4).\n";
                DebugTrace(oss.str());
        }

        if (input.IsTrigger(VK_F6))
        {
                g_DrawColliderBounds = !g_DrawColliderBounds;
                std::ostringstream oss;
                oss << kLogPrefix << "[ColliderDebug] r.DebugColliderBounds="
                        << (g_DrawColliderBounds ? 1 : 0) << " (F6).\n";
                DebugTrace(oss.str());
        }
        constexpr float kDebugNudge = 0.25f;
        float3 delta{ 0.0f, 0.0f, 0.0f };

        if (input.IsTrigger(Core::ActionId::ColliderNudgeUp))
        {
                delta.y += kDebugNudge;
        }
        if (input.IsTrigger(Core::ActionId::ColliderNudgeDown))
        {
                delta.y -= kDebugNudge;
        }

        if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f)
        {
                return;
        }

        Entities::TranslatePhysicalEntity(*player, delta);

        const auto& transform = player->GetTransform();

        player->SyncColliderBounds();
        const AABB bounds = player->GetAABB();
        std::ostringstream oss;
        oss << kLogPrefix << "[ColliderDebug] Nudged player by (" << delta.x << ',' << delta.y << ',' << delta.z << ")"
            << " newPos=(" << transform.position.x << ',' << transform.position.y << ',' << transform.position.z << ")"
            << " aabbMin=(" << bounds.min.x << ',' << bounds.min.y << ',' << bounds.min.z << ")"
            << " aabbMax=(" << bounds.max.x << ',' << bounds.max.y << ',' << bounds.max.z << ")\n";
        ::DebugTrace(oss.str());
}
#endif

#if defined(_DEBUG)
void SceneGame::DrawPhysicsDebugOverlay()
{
    if (!g_DebugDrawEnabled)
    {
        return;
    }

    if (!g_DrawColliders)
    {
        return;
    }

    const Color fixtureColor = MakeColor(1.0f, 0.1f, 0.1f, 1.0f);
    const Color boundsColor = MakeColor(0.0f, 0.7f, 1.0f, 1.0f);

    const auto drawEntity = [&](Entities::PhysicalEntity* entity)
    {
        if (!entity)
        {
            return;
        }

        if (g_DrawColliderBounds)
        {
            DrawModelBoundsOverlay(*entity, boundsColor);
        }
        DrawColliderOverlay(*entity, fixtureColor);
    };

    drawEntity(player.get());

    if (m_sweetsManager)
    {
        const auto drawState = [&](SweetsSpace::SweetsState state)
        {
            const auto& list = m_sweetsManager->GetByState(state);
            for (const auto& sweets : list)
            {
                drawEntity(sweets.get());
            }
        };

        drawState(SweetsSpace::SweetsState::Active);
        drawState(SweetsSpace::SweetsState::Stacked);
        drawState(SweetsSpace::SweetsState::Carried);
    }

    Geometory::DrawLines();
}
#endif

void SceneGame::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
        if (m_backgroundRenderer)
        {
                m_backgroundRenderer->Draw();
        }

        // 描画確認用の数値
        if (!activeCamera)
        {
                DebugTrace(std::string(kLogPrefix) + "Draw skipped: no active camera registered.\n");
                return;
        }

        if (m_render)
        {
#if defined(_DEBUG) && SCENEGAME_VERBOSE_DRAW_LOG
            DebugTrace(std::string(kLogPrefix) + "描画開始: " + std::to_string(m_stageObjects.size()) + "個のオブジェクト\n");
#endif

            for (const auto& obj : m_stageObjects)
            {
                if (obj.model)
                {
#if defined(_DEBUG) && SCENEGAME_VERBOSE_DRAW_LOG
                    DebugTrace(std::string(kLogPrefix) + "描画中: " + obj.modelPath + "\n");
#endif

                    Transform renderTransform = obj.transform;
                    
                    // 回転アニメーションを適用
                    if (obj.spinEnabled)
                    {
                        if (obj.spinAxis.y > 0.5f)  // Y軸回転
                            renderTransform.rotation.y += obj.currentRotation;
                        else if (obj.spinAxis.x > 0.5f)  // X軸回転
                            renderTransform.rotation.x += obj.currentRotation;
                        else if (obj.spinAxis.z > 0.5f)  // Z軸回転
                            renderTransform.rotation.z += obj.currentRotation;
                    }

                    m_render->Draw(*obj.model, renderTransform, activeCamera.get(), nullptr);
                }
                else
                {
                    DebugTrace(std::string(kLogPrefix) + "モデルがnull\n");
                }
            }
        }
        else
        {
            DebugTrace(std::string(kLogPrefix) + "m_renderがnull\n");
        }

        Geometory::SetView(activeCamera->GetView());
        Geometory::SetProjection(activeCamera->GetProjection());

        if (!m_drawSystem)
        {
                DebugTrace(std::string(kLogPrefix) + "Draw skipped: draw system missing.\n");
                return;
        }

        m_drawSystem->RenderQueued(activeCamera);

        m_drawSystem->DebugDrawQueued(activeCamera);

#if defined(_DEBUG)
        DrawPhysicsDebugOverlay();
#endif

}

void SceneGame::DrawOverlay()
{
    if (m_uiOverlay)
    {
        m_uiOverlay->Draw();
    }

#if defined(_DEBUG)
    //RenderRunTelemetryOverlay();　なぜコメントアウトされたかドロスに聞くといい
#endif
}

void SceneGame::ConfigureCamera(const CameraPrototypeSettings& settings)
{
    if (m_gameCamera)
    {
        m_gameCamera->SetFieldOfViewDegrees(settings.fovDegrees);
    }

    if (m_cameraRig)
    {
        m_cameraRig->SetOffset(settings.offset);
        m_cameraRig->SetYaw(settings.yawRadians);
        m_cameraRig->SetPitch(settings.pitchRadians);
        m_cameraRig->SetFollowSpeed(settings.fixedPosition ? 0.0f : settings.followSpeed);
        m_cameraRig->SetTranslationLocked(settings.fixedPosition);
        m_cameraRig->SetYawPitchLocked(settings.fixedPosition);
    }

    if (!m_gameCamera)
    {
        DebugTrace(std::string(kLogPrefix) + "ConfigureCamera skipped: gameplay camera missing.\n");
        return;
    }

    float3 cameraRotation{ settings.pitchRadians, settings.yawRadians, 0.0f };
    float3 cameraPosition = settings.offset;
    if (!settings.fixedPosition && m_cameraAnchor)
    {
        const auto& anchorTransform = m_cameraAnchor->GetTransform();
        cameraRotation.x += anchorTransform.rotation.x;
        cameraRotation.y += anchorTransform.rotation.y;
        cameraRotation.z += anchorTransform.rotation.z;

        DirectX::XMVECTOR anchorPos = LoadFloat3(anchorTransform.position);
        DirectX::XMVECTOR rotatedOffset = RotateOffset(settings.offset, cameraRotation.y, cameraRotation.x);
        DirectX::XMVECTOR desiredPos = DirectX::XMVectorAdd(anchorPos, rotatedOffset);
        cameraPosition = StoreFloat3(desiredPos);
    }

    m_gameCamera->SetPosition(cameraPosition);
    m_gameCamera->SetRotation(cameraRotation);
// BaseCamera::RecalculateViewを直接呼び出す代わりに、パブリック API 経由でビューの更新を強制
// BaseCamera::RecalculateViewは保護されたメソッドです GetView はトランスフォームが変更された際に遅延評価で再計算
    (void)m_gameCamera->GetView();

    std::ostringstream oss;
    oss << kLogPrefix << "Configured camera prototype: fixed=" << std::boolalpha << settings.fixedPosition
        << ", fov=" << settings.fovDegrees
        << ", followSpeed=" << settings.followSpeed
        << ", offset=" << settings.offset.x << ',' << settings.offset.y << ',' << settings.offset.z
        << ", yaw=" << settings.yawRadians << ", pitch=" << settings.pitchRadians << "\n";
    if (settings.fixedPosition)
    {
        const float yawDegrees = DirectX::XMConvertToDegrees(settings.yawRadians);
        const float pitchDegrees = DirectX::XMConvertToDegrees(settings.pitchRadians);
        oss << kLogPrefix << "[CameraStatic] Final position (" << cameraPosition.x << ',' << cameraPosition.y << ','
            << cameraPosition.z << ") yaw=" << yawDegrees << "deg pitch=" << pitchDegrees << "deg\n";
    }
    ::DebugTrace(oss.str());

}

void SceneGame::SetStageState(StageState newState)
{
    if (m_stageState == newState)
    {
        if (newState == StageState::Playing)
        {
            OnStageStart();
        }
        return;
    }

    m_stageState = newState;
    ResetGuidelineHeight();

    switch (newState)
    {
    case StageState::Playing:
        OnStageStart();
        break;
    case StageState::Cleared:
        OnStageClear();
        break;
    case StageState::TimeUp:
        OnStageFail();
        break;
    default:
        break;
    }
}

void SceneGame::OnStageStart()
{
    ResetPlaySession();
}

void SceneGame::OnStageClear()
{
    ToggleGameplaySystems(false);

    std::ostringstream oss;
    const GameplayRules::ScoreInput scoreInput{
        &m_phaseManager,
        kBaseScorePerMeter,
        DebugTrace,
        kLogPrefix
    };
    GameplayRules::ScoreState scoreState{
        m_peakTowerHeight,
        m_lastScoreLogValue,
        m_lastScoreLogMultiplier
    };
    GameplayRules::ScoreResult scoreResult = GameplayRules::ComputeScore(scoreInput, scoreState);
    m_lastScoreLogValue = scoreResult.state.lastScoreLogValue;
    m_lastScoreLogMultiplier = scoreResult.state.lastScoreLogMultiplier;
    oss << kLogPrefix << "[Stage] cleared peakTop=" << std::fixed << std::setprecision(2)
        << m_peakTowerHeight << "m score=" << scoreResult.score << "\n";
    ::DebugTrace(oss.str());
}

void SceneGame::OnStageFail()
{
    ToggleGameplaySystems(false);

    std::ostringstream oss;
    const GameplayRules::ScoreInput scoreInput{
        &m_phaseManager,
        kBaseScorePerMeter,
        DebugTrace,
        kLogPrefix
    };
    GameplayRules::ScoreState scoreState{
        m_peakTowerHeight,
        m_lastScoreLogValue,
        m_lastScoreLogMultiplier
    };
    GameplayRules::ScoreResult scoreResult = GameplayRules::ComputeScore(scoreInput, scoreState);
    m_lastScoreLogValue = scoreResult.state.lastScoreLogValue;
    m_lastScoreLogMultiplier = scoreResult.state.lastScoreLogMultiplier;
    oss << kLogPrefix << "[Countdown] timeUp peakTop=" << std::fixed << std::setprecision(2)
        << m_peakTowerHeight << "m score=" << scoreResult.score << "\n";
    ::DebugTrace(oss.str());
}

void SceneGame::BroadcastPhaseAdvance(int newPhase)
{
    std::ostringstream oss;
    oss << kLogPrefix << "Broadcasting phase " << newPhase
        << " (stackedPeak=" << std::fixed << std::setprecision(2) << m_peakTowerHeight << ")\n";
    ::DebugTrace(oss.str());

    if (m_sweetsManager)
    {
        m_sweetsManager->NotifyPhaseChanged(newPhase);
    }
    // BroadcastPhaseAdvance は、他のセッション制御が開始される前にここで終了すべき
}

void SceneGame::ForwardSweetsPhaseChange(int newPhase)
{
    std::ostringstream oss;
    oss << kLogPrefix << "[SweetsPhase] Forwarding phase " << newPhase
        << " to spawn pacing system.\n";
    ::DebugTrace(oss.str());

    if (m_sweetsManager)
    {
        m_sweetsManager->ForceSpawnRefresh();
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "[SweetsPhase] Spawn pacing refresh skipped; manager missing.\n");
    }
}
void SceneGame::ResetPlaySession()
{
    if (m_sweetsManager)
    {
        m_sweetsManager->ClearAllSweets();
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "ResetPlaySession could not clear sweets: manager missing.\n");
    }

    if (player)
    {
        player->ClearCarriedSweets();
        if (m_playerBaseHorizontalSpeed > 0.0f || m_playerBaseVerticalSpeed > 0.0f)
        {
            m_playerSpeedMultiplier = 1.0f;
            player->SetBaseMoveSpeed(m_playerBaseHorizontalSpeed, m_playerBaseVerticalSpeed);
            player->SetSpeedMultiplier(m_playerSpeedMultiplier);
        }

        player->SetPickupArea(m_playerPickupBoxWidth, m_playerPickupBoxHeight, m_playerPickupOffset);
        player->SetPickupMaxDistance(m_playerPickupMaxDistance);
        player->SetPickupDepthThreshold(m_playerPickupDepthThreshold);
        player->SetPickupOriginOffset(m_playerPickupOriginOffset);
    }

    else if (!m_cameraAnchor)
    {
        DebugTrace(std::string(kLogPrefix) + "ResetPlaySession skipped anchor realignment: camera anchor missing.\n");
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "ResetPlaySession found no stage anchors to realign.\n");
    }

    m_countdownTimer.Reset();
    m_totalPlaySeconds = kDefaultPlaySeconds;
    m_remainingCountdownSeconds = m_totalPlaySeconds;
    m_lastAnnouncedCountdownSeconds = static_cast<int>(std::ceil(m_totalPlaySeconds));
    m_currentTowerHeight = 0.0f;
    m_peakTowerHeight = 0.0f;
    m_maxTowerHeight = 0.0f;
    m_pendingTimeBonuses.clear();
    //m_heightBonusArmed = (m_heightBonusThresholdMeters > 0.0f && m_heightBonusSeconds > 0.0f);
    m_guidelineTriggeredThisHeight.clear();
    ResetGuidelineHeight();
    m_guidelineCooldownRemainingSeconds = 0.0f;
    m_guidelineTriggerCooldownSeconds = 0.0f;
    ToggleGameplaySystems(true);
    m_phaseManager.Reset();

    if (m_sweetsManager)
    {
        m_sweetsManager->NotifyPhaseChanged(m_phaseManager.GetCurrentPhase());
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "SweetsManager missing during ResetPlaySession; phase reset not broadcast.\n");
    }

    std::ostringstream oss;
    oss << kLogPrefix << "[Countdown] start duration=" << m_totalPlaySeconds << "s\n";
    ::DebugTrace(oss.str());
}


void SceneGame::ProcessPostGameInput(Core::UpdateContext& ctx)
{
    if (m_stageState != StageState::ShowingResult)
    {
        return;
    }

    Core::Input& input = ctx.input;
    if (input.IsTrigger(Core::ActionId::Confirm))
    {
        std::ostringstream oss;
        const GameplayRules::ScoreInput scoreInput{
            &m_phaseManager,
            kBaseScorePerMeter,
            DebugTrace,
            kLogPrefix
        };
        GameplayRules::ScoreState scoreState{
            m_peakTowerHeight,
            m_lastScoreLogValue,
            m_lastScoreLogMultiplier
        };
        GameplayRules::ScoreResult scoreResult = GameplayRules::ComputeScore(scoreInput, scoreState);
        m_lastScoreLogValue = scoreResult.state.lastScoreLogValue;
        m_lastScoreLogMultiplier = scoreResult.state.lastScoreLogMultiplier;
        oss << kLogPrefix << "[Result] Confirm accepted peakTop=" << std::fixed << std::setprecision(2)
            << m_peakTowerHeight << "m score=" << scoreResult.score << "\n";
        ::DebugTrace(oss.str());
        SetStageState(StageState::Playing);
        return;
    }

    if (input.IsTrigger(Core::ActionId::Retry))
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[Result] Retry input ignored until confirm. peakTop="
            << std::fixed << std::setprecision(2) << m_peakTowerHeight << "m\n";
        ::DebugTrace(oss.str());
    }
}

void SceneGame::HandleManualRetryInput(Core::UpdateContext& ctx)
{
    if (m_stageState != StageState::Playing)
    {
        return;
    }

    if (!ctx.input.IsTrigger(Core::ActionId::Retry))
    {
        return;
    }

    std::ostringstream oss;
    oss << kLogPrefix << "[Countdown] Retry requested elapsed="
        << std::fixed << std::setprecision(2) << (m_totalPlaySeconds - m_remainingCountdownSeconds)
        << "s peak=" << std::fixed << std::setprecision(2) << m_peakTowerHeight << "m\n";
    ::DebugTrace(oss.str());
    SetStageState(StageState::Playing);
}

void SceneGame::ToggleGameplaySystems(bool enabled)
{
    if (m_physicsBinder)
    {
        m_physicsBinder->Enable(enabled);
    }
    if (m_sweetsManager)
    {
        m_sweetsManager->Enable(enabled);
    }
    if (player)
    {
        player->Enable(enabled);
    }
}

int SceneGame::GetSweetsCount() const
{
    if (m_sweetsManager)
        return m_sweetsManager->GetSweetsCount();
    return 0;
}



void SceneGame::Initialize(const Stage::StageData& stageData)
{
    
    //高さボーナス設定
    m_heightBonusThresholdMeters = stageData.GetHeightBonusThreshold();
    m_heightBonusSeconds = stageData.GetHeightBonusSeconds();

    //デバッグ確認用
    std::ostringstream oss;
    oss << "[DEBUG] HeightBonus initialized=" << m_heightBonusThresholdMeters
        << " second=" << m_heightBonusSeconds << "\n";
    ::DebugTrace(oss.str());
}
