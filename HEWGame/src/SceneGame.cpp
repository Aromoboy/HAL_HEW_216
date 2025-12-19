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
#include "Systems/PhysicsMachine.h"
#include "Systems/Box2DContactListener.h"
#include "Systems/DrawSystem.h"
#include "BoxColliderComponent.h"
#include "ModelCache.h"
#include "TextureCache.h"
#include "DirectX.h"
#include "BaseCamera.h"
#include "SweetsManager.h"
#include "SweetsPrototypeManifest.h"
#include "BackgroundRenderer.h"
#include "Sweets/BaseSweets.h"
#include "Entities/Player.h"
#include "Collision/ConvexHullBuilder.h"
#include "ColliderConfigLoader.h"
#include "DebugColliderRenderer.h"
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
#include <filesystem>
#include <DirectXMath.h>
#include "Physical.h"
#include "ColliderRegistry.h"

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

        constexpr const char* kLogPrefix = "[SceneGame] ";
        constexpr const char* kPhysicsConfigPath = "Assets/Config/physics_tuning.cfg";
        constexpr bool kEnableDrawSystemVerboseLogging = false;
        constexpr const char* kStageChecklistConfigPath = "Assets/Config/stage_checklist.cfg";
        constexpr const char* kStageDefaultConfigPath = "Assets/Config/stage_default.cfg";
        constexpr const char* kStageColliderConfigPath = "Assets/Config/stage_colliders.cfg";
        constexpr const char* kSweetsSpawnConfigPath = "Assets/Config/sweets_spawn.cfg";
        constexpr const char* kPlayerColliderConfigPath = "Assets/Config/colliders_player.cfg";
        constexpr const char* kPlayerColliderTuningPath = "Assets/Config/player_collider.csv";
        constexpr const char* kPlayerTransformConfigPath = "Assets/Config/player_transform.cfg";
        constexpr const char* kRenderSettingsConfigPath = "Assets/Config/render_settings.cfg";
        constexpr const char* kColliderRegistryPath = "Assets/Config/collider_registry.cfg";
        constexpr float kDefaultPlaySeconds = 30.0f;
        constexpr float kCountdownLogIntervalSeconds = 5.0f;
        constexpr float kBaseScorePerMeter = 100.0f;
        constexpr float kPlayerSpeedBoostStep = 1.2f;
        constexpr float kMaxPlayerSpeedMultiplier = 2.0f;
        constexpr float kFallbackPlayerMoveSpeed = 10.0f;
        constexpr float kDefaultPickupMaxDistance = 0.5f;
        constexpr float kDefaultPickupDepthThreshold = std::numeric_limits<float>::infinity();
        constexpr const char* kFallbackPlayerModelPath = "Assets/Model/SweetsPrototype/FairyPlaceholder.obj";
        constexpr float kDefaultGeneratedThicknessPadding = 1.1f;
        constexpr float kMinGeneratedThickness = 0.05f;
        constexpr float kPlayerMinimumYMargin = 1.0f;
        constexpr float kGuidelineTimeBonusSeconds = 5.0f;
        constexpr float kGuidelineGlobalCooldownSeconds = 0.15f;
        constexpr float3 kDefaultPlayerColliderPadding{ 0.02f, 0.05f, 0.02f };
        constexpr float3 kDefaultPlayerColliderOffset{ 0.0f, 0.0f, 0.0f };
        constexpr float3 kDefaultPlayerFallbackPadding{ 0.02f, 0.05f, 0.02f };
        constexpr float3 kDefaultPlayerFallbackOffset{ 0.0f, 0.05f, 0.0f };
        constexpr float3 kDefaultPlayerFallbackSize{ 0.6f, 0.9f, 0.12f };
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
        const HudLayoutConfig kHudLayoutConfig
        {
            512u,
            192u,
            32.0f,
            12.0f,
            28.0f,
            5.0f,
            1.5f,
            2.0f,
            DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            DirectX::XMFLOAT4(1.0f, 0.55f, 0.25f, 1.0f),
            3.0f,
            32.0f,
            DirectX::XMFLOAT4(0.95f, 0.95f, 1.0f, 0.8f)
        };

        constexpr float kGuidelineViewHeightScale = 12.0f;

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

        std::string TrimWhitespace(const std::string& input)
        {
            const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
            auto begin = input.begin();
            auto end = input.end();
            while (begin != end && isSpace(static_cast<unsigned char>(*begin)))
            {
                ++begin;
            }
            if (begin == end)
            {
                return std::string{};
            }
            do
            {
                --end;
            } while (end != begin && isSpace(static_cast<unsigned char>(*end)));
            return std::string(begin, end + 1);
        }

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

        struct StageColliderPreset
        {
            std::string presetId;
            std::string registryId;
            Collision::CollisionLayer layer{ Collision::CollisionLayer::Floor };
            b2BodyType bodyType{ b2_staticBody };
            bool fixedRotation{ true };
            bool allowRotation{ false };
            b2Vec2 offset{ 0.0f, 0.0f };
        };

        struct StageColliderConfig
        {
            std::unordered_map<std::string, StageColliderPreset> presetsByModelKey;
            std::unordered_map<std::string, StageColliderPreset> presetsById;
        };

        std::string ToLowerCopy(const std::string& value)
        {
            std::string lowered = value;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });
            return lowered;
        }

        Collision::CollisionLayer ParseCollisionLayer(const std::string& token, Collision::CollisionLayer fallback)
        {
            const std::string lowered = ToLowerCopy(token);
            if (lowered == "floor") return Collision::CollisionLayer::Floor;
            if (lowered == "player") return Collision::CollisionLayer::Player;
            if (lowered == "sweets") return Collision::CollisionLayer::Sweets;
            if (lowered == "default") return Collision::CollisionLayer::Default;
            return fallback;
        }

        b2BodyType ParseBodyType(const std::string& token, b2BodyType fallback)
        {
            const std::string lowered = ToLowerCopy(token);
            if (lowered == "static" || lowered == "staticbody") return b2_staticBody;
            if (lowered == "dynamic" || lowered == "dynamicbody") return b2_dynamicBody;
            if (lowered == "kinematic" || lowered == "kinematicbody") return b2_kinematicBody;
            return fallback;
        }

        bool ParseBoolToken(const std::string& token, bool fallback)
        {
            const std::string lowered = ToLowerCopy(token);
            if (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "on")
            {
                return true;
            }
            if (lowered == "false" || lowered == "0" || lowered == "no" || lowered == "off")
            {
                return false;
            }
            return fallback;
        }

        std::string BuildModelKey(const std::string& modelPath)
        {
            std::string key = modelPath;
            try
            {
                const std::filesystem::path path(modelPath);
                key = path.filename().string();
            }
            catch (...)
            {
                // そのまま modelPath を使う
            }

            return NormalizeColliderKey(key);
        }

        StageColliderConfig LoadStageColliderConfig(const std::string& path)
        {
            StageColliderConfig config{};
            std::ifstream file(path);
            if (!file.is_open())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Stage collider config missing at '" << path << "'\n";
                DebugTrace(oss.str());
                return config;
            }

            std::string line;
            size_t lineNumber = 0;
            while (std::getline(file, line))
            {
                ++lineNumber;
                line = TrimWhitespace(line);
                if (line.empty() || line[0] == '#')
                {
                    continue;
                }

                std::vector<std::string> tokens;
                std::stringstream ss(line);
                std::string token;
                while (std::getline(ss, token, ','))
                {
                    tokens.push_back(TrimWhitespace(token));
                }

                if (tokens.size() < 5)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[StageColliders] Line " << lineNumber << " malformed ('" << line
                        << "'); expected at least 5 tokens.\n";
                    DebugTrace(oss.str());
                    continue;
                }

                StageColliderPreset preset{};
                preset.presetId = NormalizeColliderKey(tokens[0]);
                const std::string modelKey = NormalizeColliderKey(tokens[1]);
                preset.registryId = NormalizeColliderKey(tokens[2]);
                preset.layer = ParseCollisionLayer(tokens[3], Collision::CollisionLayer::Floor);
                preset.bodyType = ParseBodyType(tokens[4], b2_staticBody);
                if (tokens.size() > 5)
                {
                    preset.fixedRotation = ParseBoolToken(tokens[5], true);
                }
                if (tokens.size() > 6)
                {
                    preset.allowRotation = ParseBoolToken(tokens[6], false);
                }
                if (tokens.size() > 7)
                {
                    try { preset.offset.x = std::stof(tokens[7]); } catch (...) {}
                }
                if (tokens.size() > 8)
                {
                    try { preset.offset.y = std::stof(tokens[8]); } catch (...) {}
                }

                if (!preset.presetId.empty())
                {
                    config.presetsById[preset.presetId] = preset;
                }
                if (!modelKey.empty())
                {
                    config.presetsByModelKey[modelKey] = preset;
                }
            }

            std::ostringstream oss;
            oss << kLogPrefix << "[StageColliders] Loaded " << config.presetsById.size()
                << " presets from '" << path << "'.\n";
            DebugTrace(oss.str());

            return config;
        }

        std::optional<StageColliderPreset> ResolveStageColliderPreset(
            const StageColliderConfig& config,
            const std::string& explicitPreset,
            const std::string& modelPath)
        {
            const std::string requested = NormalizeColliderKey(explicitPreset);
            if (!requested.empty())
            {
                auto it = config.presetsById.find(requested);
                if (it != config.presetsById.end())
                {
                    return it->second;
                }
            }

            const std::string modelKey = BuildModelKey(modelPath);
            auto byModel = config.presetsByModelKey.find(modelKey);
            if (byModel != config.presetsByModelKey.end())
            {
                return byModel->second;
            }

            auto fallbackPreset = config.presetsById.find(modelKey);
            if (fallbackPreset != config.presetsById.end())
            {
                return fallbackPreset->second;
            }

            return std::nullopt;
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

        SceneGame::PlayerColliderTuning LoadPlayerColliderTuning(const std::string& path)
        {
            SceneGame::PlayerColliderTuning tuning{};
            tuning.thicknessPadding = kDefaultGeneratedThicknessPadding;
            tuning.padding = kDefaultPlayerColliderPadding;
            tuning.offset = kDefaultPlayerColliderOffset;
            tuning.fallbackPadding = kDefaultPlayerFallbackPadding;
            tuning.fallbackOffset = kDefaultPlayerFallbackOffset;
            tuning.fallbackSize = kDefaultPlayerFallbackSize;

            std::ifstream file(path);
            if (!file)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Tuning file missing; using defaults path='" << path << "'.\n";
                DebugTrace(oss.str());
                return tuning;
            }

            std::string header;
            if (!std::getline(file, header))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Tuning file empty; using defaults path='" << path << "'.\n";
                DebugTrace(oss.str());
                return tuning;
            }

            std::string line;
            while (std::getline(file, line))
            {
                if (line.empty() || line[0] == '#')
                {
                    continue;
                }

                std::vector<std::string> tokens;
                std::stringstream stream(line);
                std::string token;
                while (std::getline(stream, token, ','))
                {
                    tokens.push_back(token);
                }

                if (tokens.size() < 7)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[PlayerCollider] Expected at least 7 tokens in tuning CSV; got " << tokens.size() << " from '" << line << "'.\n";
                    DebugTrace(oss.str());
                    continue;
                }

                try
                {
                    tuning.thicknessPadding = std::max(std::stof(tokens[0]), 1.0f);
                    tuning.padding.x = std::stof(tokens[1]);
                    tuning.padding.y = std::stof(tokens[2]);
                    tuning.padding.z = std::stof(tokens[3]);
                    tuning.offset.x = std::stof(tokens[4]);
                    tuning.offset.y = std::stof(tokens[5]);
                    tuning.offset.z = std::stof(tokens[6]);

                    if (tokens.size() >= 10)
                    {
                        tuning.fallbackPadding.x = std::stof(tokens[7]);
                        tuning.fallbackPadding.y = std::stof(tokens[8]);
                        tuning.fallbackPadding.z = std::stof(tokens[9]);
                    }

                    if (tokens.size() >= 13)
                    {
                        tuning.fallbackOffset.x = std::stof(tokens[10]);
                        tuning.fallbackOffset.y = std::stof(tokens[11]);
                        tuning.fallbackOffset.z = std::stof(tokens[12]);
                    }

                    if (tokens.size() >= 16)
                    {
                        tuning.fallbackSize.x = std::stof(tokens[13]);
                        tuning.fallbackSize.y = std::stof(tokens[14]);
                        tuning.fallbackSize.z = std::stof(tokens[15]);
                    }
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[PlayerCollider] Failed to parse tuning row: " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    continue;
                }

                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Tuning applied pad=(" << tuning.padding.x << ',' << tuning.padding.y << ',' << tuning.padding.z
                    << ") offset=(" << tuning.offset.x << ',' << tuning.offset.y << ',' << tuning.offset.z
                    << ") fallbackPad=(" << tuning.fallbackPadding.x << ',' << tuning.fallbackPadding.y << ',' << tuning.fallbackPadding.z
                    << ") fallbackOffset=(" << tuning.fallbackOffset.x << ',' << tuning.fallbackOffset.y << ',' << tuning.fallbackOffset.z
                    << ") fallbackSize=(" << tuning.fallbackSize.x << ',' << tuning.fallbackSize.y << ',' << tuning.fallbackSize.z
                    << ") thicknessPadding=" << tuning.thicknessPadding << "\n";
                DebugTrace(oss.str());
                break;
            }

            return tuning;
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
            float fallbackWidth,
            float fallbackHeight,
            b2BodyType bodyType)
        {
            (void)fallbackWidth;
            (void)fallbackHeight;

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

            Physics::Box2DShapeConfig shapeConfig{};
            shapeConfig.bodyType = bodyType;
            shapeConfig.fixedRotation = true;
            shapeConfig.allowRotation = false;
            shapeConfig.angularDamping = 5.0f;
            shapeConfig.density = std::max(0.0f, registryDefinition->shapeDef.density);
            shapeConfig.friction = std::max(0.0f, registryDefinition->shapeDef.material.friction);
            shapeConfig.restitution = std::clamp(registryDefinition->shapeDef.material.restitution, 0.0f, 1.0f);

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

            const float3 scaledOffset = ScaleVector(tuning.offset, transform.scale);
            result.centerOffset = footprint.Center(scaledOffset.z);
            result.centerOffset.x += scaledOffset.x;
            result.centerOffset.y += scaledOffset.y;
            result.halfExtents = footprint.HalfExtents(kMinGeneratedThickness * 0.5f);

            const auto& primaryShape = collider.shapes.front();
            switch (shapeConfig.type)
            {
            case Physics::Box2DShapeConfig::Type::Polygon:
            case Physics::Box2DShapeConfig::Type::Box:
                shapeConfig.vertices = primaryShape.vertices;
                shapeConfig.offset = b2Vec2{ result.centerOffset.x, result.centerOffset.y };
                break;
            case Physics::Box2DShapeConfig::Type::Circle:
                shapeConfig.radius = primaryShape.radius;
                shapeConfig.offset = primaryShape.vertices.empty() ? b2Vec2{ 0.0f, 0.0f } : primaryShape.vertices.front();
                break;
            default:
                break;
            }

            result.collider = collider;
            result.shapeConfig = shapeConfig;
            result.source = "Registry";
            return result;
        }

        PlayerColliderBuildResult BuildPlayerCollider(
            const std::string& resolvedKey,
            const Transform& transform,
            const std::string& modelPath,
            const SceneGame::PlayerColliderTuning& tuning,
            const std::map<std::string, Collision::ColliderDefinition>& definitions,
            bool forceConfig,
            b2BodyType bodyType)
        {
            PlayerColliderBuildResult result{};

            (void)forceConfig;
            (void)modelPath;

            Physics::Box2DShapeConfig shapeConfig{};
            shapeConfig.type = Physics::Box2DShapeConfig::Type::None;
            shapeConfig.bodyType = bodyType;
            shapeConfig.fixedRotation = true;
            shapeConfig.allowRotation = false;
            shapeConfig.angularDamping = 5.0f;

            const auto registryDefinition = Physics::ColliderRegistry::Instance().Find(resolvedKey);
            result.registryHit = registryDefinition.has_value();
            result.registryHasShapes = registryDefinition && !registryDefinition->shapes.empty();

            if (!registryDefinition)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Registry materials missing for collider='" << resolvedKey
                    << "'; player collider cannot be built.\n";
                DebugTrace(oss.str());
                result.source = "RegistryMissing";
                return result;
            }

            if (!result.registryHasShapes)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Registry collider '" << resolvedKey
                    << "' has no shapes; player collider cannot be built.\n";
                DebugTrace(oss.str());
                result.source = "RegistryEmpty";
                return result;
            }

            const auto configIt = definitions.find(resolvedKey);
            if (configIt == definitions.end())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Definition missing for key='" << resolvedKey
                    << "'; player collider cannot be created.\n";
                DebugTrace(oss.str());
                result.source = "ConfigMissing";
                return result;
            }

            const Collision::ColliderDefinition& configDef = configIt->second;

            Physics::BoxColliderComponent collider{};
            collider.colliderId = resolvedKey;
            collider.shapeDef = registryDefinition->shapeDef;

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

            shapeConfig.density = std::max(0.0f, registryDefinition->shapeDef.density);
            shapeConfig.friction = std::max(0.0f, registryDefinition->shapeDef.material.friction);
            shapeConfig.restitution = std::clamp(registryDefinition->shapeDef.material.restitution, 0.0f, 1.0f);

            result.centerOffset = footprint.Center(tuning.offset.z);
            result.halfExtents = footprint.HalfExtents(kMinGeneratedThickness * 0.5f);

            const auto& primaryShape = collider.shapes.front();
            switch (shapeConfig.type)
            {
            case Physics::Box2DShapeConfig::Type::Polygon:
            case Physics::Box2DShapeConfig::Type::Box:
                shapeConfig.vertices = primaryShape.vertices;
                shapeConfig.offset = b2Vec2{ result.centerOffset.x, result.centerOffset.y };
                break;
            case Physics::Box2DShapeConfig::Type::Circle:
                shapeConfig.radius = primaryShape.radius;
                shapeConfig.offset = primaryShape.vertices.empty() ? b2Vec2{ 0.0f, 0.0f } : primaryShape.vertices.front();
                break;
            default:
                break;
            }

            result.collider = collider;
            result.shapeConfig = shapeConfig;
            result.source = "Config";
            return result;
        }

    std::vector<b2Vec2> BuildHullVertices(const std::vector<float2>& points, const float2& offset, float scale, float margin)
    {
        std::vector<b2Vec2> vertices;
        vertices.reserve(points.size());

        const float effectiveScale = std::max(scale, 0.0f);
        for (const auto& pt : points)
        {
            float2 scaled{ (pt.x + offset.x) * effectiveScale, (pt.y + offset.y) * effectiveScale };
            if (std::abs(margin) > 1e-6f)
            {
                const float lenSq = (scaled.x * scaled.x) + (scaled.y * scaled.y);
                if (lenSq > 1e-8f)
                {
                    const float invLen = 1.0f / std::sqrt(lenSq);
                    scaled.x += scaled.x * invLen * margin;
                    scaled.y += scaled.y * invLen * margin;
                }
                else
                {
                    scaled.x += margin;
                }
            }

            vertices.push_back(b2Vec2{ scaled.x, scaled.y });
        }

        return vertices;
    }

        bool ParseBool(const std::string& value, bool& outValue)
        {
            std::string lowered = value;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "on")
            {
                outValue = true;
                return true;
            }

            if (lowered == "false" || lowered == "0" || lowered == "no" || lowered == "off")
            {
                outValue = false;
                return true;
            }

            return false;
        }

        bool LoadPlayerColliderPreference(const std::string& path, bool defaultForceConfig)
        {
            bool forceConfig = defaultForceConfig;
            bool parsed = false;

            std::ifstream file(path);
            if (!file)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] Preference file missing; using default forceConfig=" << std::boolalpha << defaultForceConfig
                    << " path='" << path << "'\n";
                DebugTrace(oss.str());
                return forceConfig;
            }

            std::string line;
            int lineNumber = 0;
            while (std::getline(file, line))
            {
                ++lineNumber;
                const std::string trimmed = TrimWhitespace(line);
                if (trimmed.empty() || trimmed[0] == '#')
                {
                    continue;
                }

                const std::size_t eq = trimmed.find('=');
                if (eq == std::string::npos)
                {
                    continue;
                }

                const std::string key = TrimWhitespace(trimmed.substr(0, eq));
                if (key != "forceConfigCollider")
                {
                    continue;
                }

                const std::string value = TrimWhitespace(trimmed.substr(eq + 1));
                if (ParseBool(value, forceConfig))
                {
                    parsed = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[PlayerCollider] Parsed forceConfigCollider=" << std::boolalpha << forceConfig
                        << " from line=" << lineNumber << " path='" << path << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[PlayerCollider] Failed to parse forceConfigCollider value='" << value
                        << "' on line=" << lineNumber << "; using default " << std::boolalpha << defaultForceConfig << "\n";
                    DebugTrace(oss.str());
                }
                break;
            }

            if (!parsed)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerCollider] forceConfigCollider not specified; defaulting to "
                    << std::boolalpha << defaultForceConfig << " (path='" << path << "')\n";
                DebugTrace(oss.str());
            }

            return forceConfig;
        }

        bool LoadModelTextureLoadingSetting()
        {
            bool enabled = true;
            std::ifstream config(kRenderSettingsConfigPath);
            if (!config.is_open())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[RenderSettings] Missing config; defaulting loadModelTextures="
                    << std::boolalpha << enabled << " path='" << kRenderSettingsConfigPath << "'\n";
                DebugTrace(oss.str());
                return enabled;
            }

            bool parsed = false;
            std::string line;
            std::size_t lineNumber = 0;
            while (std::getline(config, line))
            {
                ++lineNumber;
                const auto commentPos = line.find_first_of("#;");
                if (commentPos != std::string::npos)
                {
                    line = line.substr(0, commentPos);
                }

                line = TrimWhitespace(line);
                if (line.empty())
                {
                    continue;
                }

                const auto delimiterPos = line.find('=');
                if (delimiterPos == std::string::npos)
                {
                    continue;
                }

                std::string key = TrimWhitespace(line.substr(0, delimiterPos));
                std::string value = TrimWhitespace(line.substr(delimiterPos + 1));
                std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c)
                    { return static_cast<char>(std::tolower(c)); });

                if (key == "loadmodeltextures" || key == "modeltextures")
                {
                    if (ParseBool(value, enabled))
                    {
                        parsed = true;
                        std::ostringstream oss;
                        oss << kLogPrefix << "[RenderSettings] Parsed loadModelTextures=" << std::boolalpha << enabled
                            << " from line=" << lineNumber << " path='" << kRenderSettingsConfigPath << "'\n";
                        DebugTrace(oss.str());
                    }
                    else
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "[RenderSettings] Invalid loadModelTextures value='" << value
                            << "' on line=" << lineNumber << "; keeping default=" << std::boolalpha << enabled << "\n";
                        DebugTrace(oss.str());
                    }
                }
            }

            if (!parsed)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[RenderSettings] loadModelTextures not found; using default="
                    << std::boolalpha << enabled << " (path='" << kRenderSettingsConfigPath << "')\n";
                DebugTrace(oss.str());
            }

            return enabled;
        }

        struct PhysicsTuningSettings
        {
            float gravity = 11.5f;
            int subStepCount = 8;
            float linearDamping = 0.97f;
            float angularDamping = 0.97f;
            float defaultFriction = 0.52f;
            float defaultRestitution = 0.06f;
            float restitutionThreshold = 5.0f;
            float hitEventThreshold = 0.0f;
            float maxStepDt = 1.0f / 60.0f;
            int maxAccumulatedSteps = 8;
            bool continuousCollisionForSweets = true;
            bool continuousCollisionForSmallDynamics = false;
            float smallDynamicContinuousThreshold = 0.6f;
            bool continuousCollisionForPlayerProjectiles = true;
        };

        SceneGame::PlayerSpawnTransform LoadPlayerTransformConfig(const std::string& path)
        {
            SceneGame::PlayerSpawnTransform result{};
            constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

            auto parseFloat3 = [&](const std::string& value, float3& out, const std::string& key, int lineNumber) -> bool
                {
                    std::vector<float> values;
                    std::stringstream ss(value);
                    std::string token;
                    while (std::getline(ss, token, ','))
                    {
                        const std::string trimmed = TrimWhitespace(token);
                        if (trimmed.empty())
                        {
                            continue;
                        }

                        try
                        {
                            values.push_back(std::stof(trimmed));
                        }
                        catch (const std::exception& ex)
                        {
                            std::ostringstream oss;
                            oss << kLogPrefix << "[PlayerTransform] Failed to parse token line=" << lineNumber
                                << " key='" << key << "' token='" << trimmed << "' reason='" << ex.what() << "'\n";
                            DebugTrace(oss.str());
                            return false;
                        }
                    }

                    if (values.size() != 3)
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "[PlayerTransform] Expected 3 components for key='" << key
                            << "' on line " << lineNumber << "; got " << values.size() << "\n";
                        DebugTrace(oss.str());
                        return false;
                    }

                    out = float3{ values[0], values[1], values[2] };
                    return true;
                };

            std::ifstream config(path);
            if (!config.is_open())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerTransform] Config missing; using defaults path='" << path << "'.\n";
                DebugTrace(oss.str());
                return result;
            }

            std::string line;
            int lineNumber = 0;
            int parsedFields = 0;
            while (std::getline(config, line))
            {
                ++lineNumber;
                const std::string trimmed = TrimWhitespace(line);
                if (trimmed.empty() || trimmed[0] == '#')
                {
                    continue;
                }

                const std::size_t eq = trimmed.find('=');
                if (eq == std::string::npos)
                {
                    continue;
                }

                const std::string key = TrimWhitespace(trimmed.substr(0, eq));
                const std::string value = TrimWhitespace(trimmed.substr(eq + 1));
                if (key.empty() || value.empty())
                {
                    continue;
                }

                float3 parsed{};
                if (key == "position" || key == "positionOffset")
                {
                    if (parseFloat3(value, parsed, key, lineNumber))
                    {
                        result.transform.position = parsed;
                        ++parsedFields;
                    }
                }
                else if (key == "rotationDegrees" || key == "rotationDeg")
                {
                    if (parseFloat3(value, parsed, key, lineNumber))
                    {
                        result.transform.rotation = float3{ parsed.x * kDegToRad, parsed.y * kDegToRad, parsed.z * kDegToRad };
                        result.rotationFromDegrees = true;
                        ++parsedFields;
                    }
                }
                else if (key == "rotation" || key == "rotationRadians")
                {
                    if (parseFloat3(value, parsed, key, lineNumber))
                    {
                        result.transform.rotation = parsed;
                        result.rotationFromDegrees = false;
                        ++parsedFields;
                    }
                }
                else if (key == "scale")
                {
                    if (parseFloat3(value, parsed, key, lineNumber))
                    {
                        result.transform.scale = parsed;
                        ++parsedFields;
                    }
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[PlayerTransform] Ignored unknown key '" << key << "' on line " << lineNumber << "\n";
                    DebugTrace(oss.str());
                }
                
            }

            result.loadedFromFile = true;

            std::ostringstream oss;
            oss << kLogPrefix << "[PlayerTransform] Applied path='" << path
                << "' loaded=" << std::boolalpha << result.loadedFromFile
                << " parsedFields=" << parsedFields
                << " position=(" << result.transform.position.x << ',' << result.transform.position.y << ',' << result.transform.position.z << ')'
                << " rotationRad=(" << result.transform.rotation.x << ',' << result.transform.rotation.y << ',' << result.transform.rotation.z << ')'
                << " scale=(" << result.transform.scale.x << ',' << result.transform.scale.y << ',' << result.transform.scale.z << ')'
                << " rotationSource=" << (result.rotationFromDegrees ? "degrees" : "radians_or_default") << "\n";
            DebugTrace(oss.str());

            return result;
        }

        PhysicsTuningSettings LoadPhysicsTuningSettings()
        {
            PhysicsTuningSettings settings;
            std::ifstream config(kPhysicsConfigPath);
            if (!config.is_open())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Physics tuning config not found at '" << kPhysicsConfigPath
                    << "'. Using defaults (gravity=" << settings.gravity
                    << ", subSteps=" << settings.subStepCount
                    << ", linearDamping=" << settings.linearDamping
                    << ", angularDamping=" << settings.angularDamping
                    << ", defaultFriction=" << settings.defaultFriction
                    << ", defaultRestitution=" << settings.defaultRestitution
                    << ", restitutionThreshold=" << settings.restitutionThreshold
                    << ", hitEventThreshold=" << settings.hitEventThreshold
                    << ", maxStepDt=" << settings.maxStepDt
                    << ", maxAccumulatedSteps=" << settings.maxAccumulatedSteps
                    << ", ccdSweets=" << std::boolalpha << settings.continuousCollisionForSweets
                    << ", ccdSmallDynamics=" << settings.continuousCollisionForSmallDynamics
                    << ", ccdSmallDynamicThreshold=" << settings.smallDynamicContinuousThreshold
                    << ", ccdPlayerProjectiles=" << settings.continuousCollisionForPlayerProjectiles
                    << ").\n";
                ::DebugTrace(oss.str());
                return settings;
            }

            auto parseBool = [](const std::string& value) -> std::optional<bool>
            {
                std::string lower = value;
                std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });

                if (lower == "1" || lower == "true")
                {
                    return true;
                }

                if (lower == "0" || lower == "false")
                {
                    return false;
                }

                return std::nullopt;
            };

            std::string line;
            size_t lineNumber = 0;
            while (std::getline(config, line))
            {
                ++lineNumber;
                const auto commentPos = line.find_first_of("#;");
                if (commentPos != std::string::npos)
                {
                    line = line.substr(0, commentPos);
                }
                line = TrimWhitespace(line);
                if (line.empty())
                {
                    continue;
                }

                const auto delimiterPos = line.find('=');
                if (delimiterPos == std::string::npos)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Invalid physics tuning line " << lineNumber
                        << " (missing '='): " << line << "\n";
                    ::DebugTrace(oss.str());
                    continue;
                }

                std::string key = TrimWhitespace(line.substr(0, delimiterPos));
                std::string value = TrimWhitespace(line.substr(delimiterPos + 1));
                if (key.empty() || value.empty())
                {
                    continue;
                }

                try
                {
                    if (key == "gravity")
                    {
                        settings.gravity = std::stof(value);
                    }
                    else if (key == "subSteps")
                    {
                        settings.subStepCount = std::max(1, std::stoi(value));
                    }
                    else if (key == "linearDamping")
                    {
                        settings.linearDamping = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "angularDamping")
                    {
                        settings.angularDamping = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "defaultFriction")
                    {
                        settings.defaultFriction = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "defaultRestitution")
                    {
                        settings.defaultRestitution = std::clamp(std::stof(value), 0.0f, 1.0f);
                    }
                    else if (key == "restitutionThreshold")
                    {
                        settings.restitutionThreshold = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "hitEventThreshold")
                    {
                        settings.hitEventThreshold = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "maxStepDt")
                    {
                        settings.maxStepDt = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "maxAccumulatedSteps")
                    {
                        settings.maxAccumulatedSteps = std::max(1, std::stoi(value));
                    }
                    else if (key == "ccdSweets")
                    {
                        if (auto parsed = parseBool(value))
                        {
                            settings.continuousCollisionForSweets = *parsed;
                        }
                        else
                        {
                            std::ostringstream oss;
                            oss << kLogPrefix << "Invalid bool for key '" << key << "' on line "
                                << lineNumber << "\n";
                            ::DebugTrace(oss.str());
                        }
                    }
                    else if (key == "ccdSmallDynamics")
                    {
                        if (auto parsed = parseBool(value))
                        {
                            settings.continuousCollisionForSmallDynamics = *parsed;
                        }
                        else
                        {
                            std::ostringstream oss;
                            oss << kLogPrefix << "Invalid bool for key '" << key << "' on line "
                                << lineNumber << "\n";
                            ::DebugTrace(oss.str());
                        }
                    }
                    else if (key == "ccdSmallDynamicThreshold")
                    {
                        settings.smallDynamicContinuousThreshold = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "ccdPlayerProjectiles")
                    {
                        if (auto parsed = parseBool(value))
                        {
                            settings.continuousCollisionForPlayerProjectiles = *parsed;
                        }
                        else
                        {
                            std::ostringstream oss;
                            oss << kLogPrefix << "Invalid bool for key '" << key << "' on line "
                                << lineNumber << "\n";
                            ::DebugTrace(oss.str());
                        }
                    }
                    else
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Unknown physics tuning key '" << key << "' on line "
                            << lineNumber << "\n";
                        ::DebugTrace(oss.str());
                    }
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse value for key '" << key << "' on line "
                        << lineNumber << ": " << ex.what() << "\n";
                    ::DebugTrace(oss.str());
                }
            }

            std::ostringstream oss;
            oss << kLogPrefix << "Loaded physics tuning from '" << kPhysicsConfigPath
                << "' (gravity=" << settings.gravity
                << ", subSteps=" << settings.subStepCount
                << ", linearDamping=" << settings.linearDamping
                << ", angularDamping=" << settings.angularDamping
                << ", defaultFriction=" << settings.defaultFriction
                << ", defaultRestitution=" << settings.defaultRestitution
                << ", restitutionThreshold=" << settings.restitutionThreshold
                << ", hitEventThreshold=" << settings.hitEventThreshold
                << ", maxStepDt=" << settings.maxStepDt
                << ", maxAccumulatedSteps=" << settings.maxAccumulatedSteps
                << ", ccdSweets=" << std::boolalpha << settings.continuousCollisionForSweets
                << ", ccdSmallDynamics=" << settings.continuousCollisionForSmallDynamics
                << ", ccdSmallDynamicThreshold=" << settings.smallDynamicContinuousThreshold
                << ", ccdPlayerProjectiles=" << settings.continuousCollisionForPlayerProjectiles
                << ").\n";
            ::DebugTrace(oss.str());
            return settings;
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

SceneGame::SceneGame()
{
m_playerColliderConfigLoaded = Collision::LoadColliderDefinitions(kPlayerColliderConfigPath, m_playerColliderDefinitions);
m_forceConfigPlayerCollider = LoadPlayerColliderPreference(kPlayerColliderConfigPath, false);
m_playerColliderTuning = LoadPlayerColliderTuning(kPlayerColliderTuningPath);
m_playerSpawnTransform = LoadPlayerTransformConfig(kPlayerTransformConfigPath);
{
    std::ostringstream oss;
    oss << kLogPrefix << "[PlayerCollider] ConfigLoaded=" << std::boolalpha << m_playerColliderConfigLoaded
        << " definitions=" << m_playerColliderDefinitions.size()
        << " tuningPadding=" << m_playerColliderTuning.thicknessPadding
        << " padXYZ=(" << m_playerColliderTuning.padding.x << ',' << m_playerColliderTuning.padding.y << ',' << m_playerColliderTuning.padding.z << ')'
        << " offsetXYZ=(" << m_playerColliderTuning.offset.x << ',' << m_playerColliderTuning.offset.y << ',' << m_playerColliderTuning.offset.z << ')'
        << " fallbackPadXYZ=(" << m_playerColliderTuning.fallbackPadding.x << ',' << m_playerColliderTuning.fallbackPadding.y << ',' << m_playerColliderTuning.fallbackPadding.z << ')'
        << " fallbackOffsetXYZ=(" << m_playerColliderTuning.fallbackOffset.x << ',' << m_playerColliderTuning.fallbackOffset.y << ',' << m_playerColliderTuning.fallbackOffset.z << ')'
        << " fallbackSizeXYZ=(" << m_playerColliderTuning.fallbackSize.x << ',' << m_playerColliderTuning.fallbackSize.y << ',' << m_playerColliderTuning.fallbackSize.z << ")\n";
    ::DebugTrace(oss.str());
}

    std::string playerColliderKey = "Player";
    m_playerColliderMismatchLogged = false;

    bool stageLoaded = m_stageData.LoadFromFile(kStageChecklistConfigPath);
    const char* stageConfigPathUsed = kStageChecklistConfigPath;
    if (!stageLoaded)
    {
        DebugTrace(std::string(kLogPrefix) + "Checklist stage config missing; falling back to default stage.\n");
        stageConfigPathUsed = kStageDefaultConfigPath;
        stageLoaded = m_stageData.LoadFromFile(stageConfigPathUsed);
    }
    m_phaseManager.Initialize(
        m_stageData.GetPhaseThresholds(),
        m_stageData.GetPhaseRewardMultipliers(),
        m_stageData.GetPhaseMiniThresholds(),
        m_stageData.GetPhaseRewardMiniMultipliers()
    );
    m_playerBaseHorizontalSpeed = m_stageData.GetPlayerHorizontalSpeedWorld();
    m_playerBaseVerticalSpeed = m_stageData.GetPlayerVerticalSpeedWorld();
    m_playerPickupBoxWidth = m_stageData.GetPickupBoxWidthWorld();
    m_playerPickupBoxHeight = m_stageData.GetPickupBoxHeightWorld();
    m_playerPickupOffset = m_stageData.GetPickupOffset();
    m_playerPickupMaxDistance = m_stageData.GetPickupMaxDistance();
    m_playerPickupDepthThreshold = m_stageData.GetPickupDepthThreshold();
    const auto sanitizePositive = [](float value, float fallback, const char* label)
    {
        if (std::isfinite(value) && value > 0.0f)
        {
            return value;
        }

        std::ostringstream oss;
        oss << kLogPrefix << label << " " << value
            << " invalid; using fallback " << fallback << "\n";
        ::DebugTrace(oss.str());
        return fallback;
    };

    const auto sanitizeDepthThreshold = [](float value, float fallback, const char* label)
    {
        if (std::isfinite(value))
        {
            if (value >= 0.0f)
            {
                return value;
            }
        }
        else if (std::isinf(value))
        {
            return value;
        }

        std::ostringstream oss;
        oss << kLogPrefix << label << " " << value
            << " invalid; using fallback " << fallback << "\n";
        ::DebugTrace(oss.str());
        return fallback;
    };

    m_playerBaseHorizontalSpeed = sanitizePositive(m_playerBaseHorizontalSpeed, kFallbackPlayerMoveSpeed, "Player horizontal speed");
    m_playerBaseVerticalSpeed = sanitizePositive(m_playerBaseVerticalSpeed, m_playerBaseHorizontalSpeed, "Player vertical speed");
    const float pickupFallback = std::max(m_stageData.GetCellSizeWorld(), 1.0f);
    m_playerPickupBoxWidth = sanitizePositive(m_playerPickupBoxWidth, pickupFallback, "Player pickup width");
    m_playerPickupBoxHeight = sanitizePositive(m_playerPickupBoxHeight, pickupFallback, "Player pickup height");
    m_playerPickupMaxDistance = sanitizePositive(m_playerPickupMaxDistance, kDefaultPickupMaxDistance, "Player pickup max distance");
    m_playerPickupDepthThreshold = sanitizeDepthThreshold(m_playerPickupDepthThreshold, kDefaultPickupDepthThreshold, "Player pickup depth threshold");
    m_playerSpeedMultiplier = 1.0f;
    {
        const auto pickupOffsetModeName = [](Stage::PickupOffsetMode mode)
        {
            switch (mode)
            {
            case Stage::PickupOffsetMode::None: return "none";
            case Stage::PickupOffsetMode::FacingDirection: return "facing-direction";
            case Stage::PickupOffsetMode::WorldX: return "world-x";
            default: return "unknown";
            }
        };
        std::ostringstream oss;
        oss << kLogPrefix << "Stage data " << (stageLoaded ? "loaded" : "defaulted")
            << " from '" << stageConfigPathUsed << "'"
            << " planeZ=" << m_stageData.GetPlaneZ()
            << " thresholds=" << m_stageData.GetPhaseThresholds().size()
            << " multipliers=" << m_stageData.GetPhaseRewardMultipliers().size()
            << " spawnPointsPrimary=" << m_stageData.GetSpawnPointsPrimary().size()
            << " spawnPointsSecondary=" << m_stageData.GetSpawnPointsSecondary().size()
            << " platforms=" << m_stageData.GetPlatformCount()
            << " playerSpeedXY=(" << m_playerBaseHorizontalSpeed << ',' << m_playerBaseVerticalSpeed << ')'
            << " pickupBox=(" << m_playerPickupBoxWidth << ',' << m_playerPickupBoxHeight << ')'
            << " pickupMaxDistance=" << m_playerPickupMaxDistance
            << " pickupDepthThreshold=" << m_playerPickupDepthThreshold
            << " pickupOffsetMode=" << pickupOffsetModeName(m_playerPickupOffset.mode)
            << " pickupOffsetLocalWorld=(" << m_playerPickupOffset.localOffset.x << ','
            << m_playerPickupOffset.localOffset.y << ',' << m_playerPickupOffset.localOffset.z << ')'
            << " guidelineHeight=" << m_stageData.GetGuidelineHeight()
            << " guidelineStep=" << m_stageData.GetGuidelineStep()
            << " heightBonusThreshold=" << m_stageData.GetHeightBonusThreshold()
            << " heightBonusSeconds=" << m_stageData.GetHeightBonusSeconds()
            << " background='" << m_stageData.GetBackgroundTexturePath() << "'\n";
        for (std::size_t i = 0; i < m_stageData.GetPlatformCount(); ++i)
        {
            const auto& descriptor = m_stageData.GetPlatform(i);
            oss << kLogPrefix << " PlatformDesc[" << i << "] center=(" << descriptor.center.x << ',' << descriptor.center.y
                << ',' << descriptor.center.z << ") scale=(" << descriptor.scale.x << ',' << descriptor.scale.y << ','
                << descriptor.scale.z << ") depth=" << descriptor.depth
                << " worldScale=(" << descriptor.widthWorld << ',' << descriptor.heightWorld << ',' << descriptor.depthWorld
                << ")"
                << " widthFraction=" << descriptor.widthFraction << "\n";
        }
        ::DebugTrace(oss.str());
    }

    auto& manifest = SweetsSpace::SweetsPrototypeManifest::Instance();
    const bool manifestLoaded = manifest.ReloadFromCsv();
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Manifest CSV " << (manifestLoaded ? "applied" : "missing; using defaults")
            << " path='" << SweetsSpace::SweetsPrototypeManifest::kDefaultCsvPath << "'\n";
        const auto prototypes = manifest.Enumerate();
        oss << kLogPrefix << " Manifest entries=" << prototypes.size() << "\n";
        for (const auto& entry : prototypes)
        {
            oss << kLogPrefix << "  Entry " << SweetsSpace::ToString(entry.type) << " key='"
                << entry.descriptor.key << "' model='" << entry.descriptor.modelPath << "'\n";
        }
        ::DebugTrace(oss.str());
    }

    Render::ModelCache& cache = Render::ModelCache::Instance();
    // シーンのアセット読み込み開始前に、モデルキャッシュを完全にクリアする
    // （ローダーやデバイスの切り替え漏れによるハングを防止）
    cache.Clear();

    const bool modelTexturesEnabled = LoadModelTextureLoadingSetting();
    cache.SetTextureLoadingEnabled(modelTexturesEnabled);

    {
        std::ostringstream oss;
        oss << kLogPrefix << "Render settings: loadModelTextures=" << std::boolalpha << modelTexturesEnabled << "\n";
        ::DebugTrace(oss.str());
    }

    NormalizePlayerColliderDefinitions();

    m_guidelineHeightMeters = m_stageData.GetGuidelineHeight();
    m_guidelineStepMeters = m_stageData.GetGuidelineStep();
    if (!std::isfinite(m_guidelineHeightMeters) || m_guidelineHeightMeters <= 0.0f)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Guideline height " << m_guidelineHeightMeters
            << " invalid; overlay guideline disabled.\n";
        ::DebugTrace(oss.str());
        m_guidelineHeightMeters = 0.0f;
    }
    if (!std::isfinite(m_guidelineStepMeters) || m_guidelineStepMeters <= 0.0f)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Guideline step " << m_guidelineStepMeters
            << " invalid; guideline will not advance.\n";
        ::DebugTrace(oss.str());
        m_guidelineStepMeters = 0.0f;
    }

    m_heightBonusThresholdMeters = m_stageData.GetHeightBonusThreshold();
    if (!std::isfinite(m_heightBonusThresholdMeters) || m_heightBonusThresholdMeters <= 0.0f)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Height bonus threshold " << m_heightBonusThresholdMeters
            << " invalid; timer bonus disabled.\n";
        ::DebugTrace(oss.str());
        m_heightBonusThresholdMeters = 0.0f;
    }

    m_heightBonusSeconds = m_stageData.GetHeightBonusSeconds();
    if (!std::isfinite(m_heightBonusSeconds))
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Height bonus seconds " << m_heightBonusSeconds
            << " invalid; timer bonus disabled.\n";
        ::DebugTrace(oss.str());
        m_heightBonusSeconds = 0.0f;
    }
    else if (m_heightBonusSeconds <= 0.0f)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Height bonus seconds <= 0; timer bonus disabled.\n";
        ::DebugTrace(oss.str());
        m_heightBonusSeconds = 0.0f;
    }

    const std::string backgroundTexture = m_stageData.GetBackgroundTexturePath();
    if (!backgroundTexture.empty())
    {
        BackgroundRendererConfig backgroundConfig{};
        backgroundConfig.texturePath = backgroundTexture;
        m_backgroundRenderer = std::make_unique<BackgroundRenderer>();
        if (!m_backgroundRenderer->Initialize(backgroundConfig))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Background renderer failed for texture '" << backgroundTexture << "'.\n";
            ::DebugTrace(oss.str());
            m_backgroundRenderer.reset();
        }
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "Background texture path empty; background renderer skipped.\n");
    }

    const PhysicsTuningSettings tuning = LoadPhysicsTuningSettings();
    Systems::PhysicsTuningParameters physicsParameters{};
    physicsParameters.gravity = tuning.gravity;
    physicsParameters.subStepCount = tuning.subStepCount;
    physicsParameters.defaultLinearDamping = tuning.linearDamping;
    physicsParameters.defaultAngularDamping = tuning.angularDamping;
    physicsParameters.defaultFriction = tuning.defaultFriction;
    physicsParameters.defaultRestitution = tuning.defaultRestitution;
    physicsParameters.restitutionThreshold = tuning.restitutionThreshold;
    physicsParameters.hitEventThreshold = tuning.hitEventThreshold;
    physicsParameters.maxStepDt = tuning.maxStepDt;
    physicsParameters.maxAccumulatedSteps = tuning.maxAccumulatedSteps;
    physicsParameters.continuousCollisionForSweets = tuning.continuousCollisionForSweets;
    physicsParameters.continuousCollisionForSmallDynamics = tuning.continuousCollisionForSmallDynamics;
    physicsParameters.smallDynamicContinuousThreshold = tuning.smallDynamicContinuousThreshold;
    physicsParameters.continuousCollisionForPlayerProjectiles = tuning.continuousCollisionForPlayerProjectiles;
    m_physicsMachine = std::make_unique<Systems::PhysicsMachine>(physicsParameters);
    m_contactListener = std::make_shared<Systems::Box2DContactListener>();
    m_drawSystem = std::make_unique<Systems::DrawSystem>();
    if (m_drawSystem)
    {
        m_drawSystem->SetStagePlaneZ(m_stageData.GetPlaneZ());
    }
    m_sweetsManager = std::make_unique<SweetsSpace::SweetsManager>();
    if (m_contactListener)
    {
        m_contactListener->SetBeginContactCallback([this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b) {
            if (m_sweetsManager)
            {
                m_sweetsManager->OnContactBegin(&a, &b);
            }
        });

        m_contactListener->SetEndContactCallback([this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b) {
            if (m_sweetsManager)
            {
                m_sweetsManager->OnContactEnd(&a, &b);
            }
        });
    }
    if (m_physicsMachine && m_contactListener)
    {
        m_physicsMachine->SetContactListener(m_contactListener);
        m_contactListener->EnableMeshContactLogging(true);
#if defined(_DEBUG)
        m_contactListener->EnableDroppedEventLogging(true);
#endif
    }

    if (m_sweetsManager)
    {
        m_sweetsManager->SetDrawSystem(m_drawSystem.get());
        m_sweetsManager->SetPhysicsMachine(m_physicsMachine.get());
        m_sweetsManager->SetUpdateScheduler(&GetUpdateScheduler());
        m_sweetsManager->SetSpawnOrigins(
            m_stageData.GetSpawnPointsPrimary(),
            m_stageData.GetSpawnPointsSecondary());
        const bool spawnConfigLoaded = m_sweetsManager->LoadPhaseSpawnConfig(kSweetsSpawnConfigPath);
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Sweets spawn config " << (spawnConfigLoaded ? "loaded" : "defaulted")
                << " from '" << kSweetsSpawnConfigPath << "'.\n";
            ::DebugTrace(oss.str());
        }
        {
            const auto presets = m_sweetsManager->EnumerateFactoryPresets();
            const auto colliderShapeToString = [](SweetsSpace::ColliderShape shape)
            {
                switch (shape)
                {
                case SweetsSpace::ColliderShape::Box:
                    return "Box";
                case SweetsSpace::ColliderShape::Capsule:
                    return "Capsule";
                default:
                    return "Unknown";
                }
            };
            std::ostringstream oss;
            oss << kLogPrefix << "[SpawnFactory] presets=" << presets.size() << " (key, scale, collider)\n";
            for (const auto& preset : presets)
            {
                oss << kLogPrefix << "  kind='" << preset.first << "' scale=("
                    << preset.second.scale.x << ','
                    << preset.second.scale.y << ','
                    << preset.second.scale.z << ") collider="
                    << colliderShapeToString(preset.second.colliderPreset.shape)
                    << " pad=(" << preset.second.colliderPreset.padding.x << ','
                    << preset.second.colliderPreset.padding.y << ','
                    << preset.second.colliderPreset.padding.z << ")"
                    << " spawnWeight=" << preset.second.spawnWeight << "\n";
            }
            ::DebugTrace(oss.str());
        }
        m_sweetsManager->SetPhaseChangeCallback([this](int newPhase)
        {
            ForwardSweetsPhaseChange(newPhase);
        });
    }

    m_hudRenderer = std::make_unique<HudRenderer>();
    if (m_hudRenderer && !m_hudRenderer->Initialize(kHudLayoutConfig))
    {
        DebugTrace(std::string(kLogPrefix) + "HUD renderer failed to initialize; overlay disabled.\n");
        m_hudRenderer.reset();
    }
    else if (m_hudRenderer)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "HUD renderer initialized (texture="
            << kHudLayoutConfig.textureWidth << 'x' << kHudLayoutConfig.textureHeight
            << ", margin=" << kHudLayoutConfig.marginPixels
            << ", warningThreshold=" << kHudLayoutConfig.warningThresholdSeconds << "s).\n";
        ::DebugTrace(oss.str());
    }

    RenderTarget* pRTV = GetDefaultRTV();
    DepthStencil* pDSV = GetDefaultDSV();
    SetRenderTargets(1, &pRTV, pDSV);
    SetDepthTest(true);
    m_cameraAnchor = std::make_shared<Entities::Entity>("PlayerAnchor");
    if (!m_pObjects.empty())
    {
        auto& anchorTransform = m_cameraAnchor->GetTransform();
        anchorTransform.position = m_pObjects.front()->GetPosition();
        anchorTransform.rotation = m_pObjects.front()->GetfRads();
        anchorTransform.scale = m_pObjects.front()->GetScale();
    }

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
    }

    const auto* fairyDescriptor = manifest.Find("Fairy");
    if (!fairyDescriptor && !m_playerModel)
    {
        DebugTrace(std::string(kLogPrefix) + "[ModelLoad] Fairy descriptor missing; using placeholder.\n");
    }

    const std::string primaryModelPath = fairyDescriptor ? fairyDescriptor->modelPath : std::string(kFallbackPlayerModelPath);
    playerColliderKey = fairyDescriptor ? fairyDescriptor->key : std::string("Fairy");
    std::string resolvedModelPath = primaryModelPath;
    m_playerModel = cache.Get(primaryModelPath);
    if (!m_playerModel && primaryModelPath != kFallbackPlayerModelPath)
    {
        DebugTrace(std::string(kLogPrefix) + "[ModelLoad] Primary fairy model missing; trying placeholder.\n");
        resolvedModelPath = kFallbackPlayerModelPath;
        m_playerModel = cache.Get(resolvedModelPath);
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
    // ModelCacheの設定後、ステージレイアウトをロード
    LoadStageLayout("Assets/StageLayout.csv");

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

    PlayerColliderBuildResult colliderBuild = BuildPlayerCollider(
        resolvedColliderKey,
        trans,
        resolvedModelPath,
        m_playerColliderTuning,
        m_playerColliderDefinitions,
        m_forceConfigPlayerCollider,
        playerBodyType);
    const auto colliderHasShapes = [](const PlayerColliderBuildResult& build)
    {
        return build.collider && !build.collider->Empty();
    };
    const auto shapeConfigIsValid = [](const PlayerColliderBuildResult& build)
    {
        return build.shapeConfig && build.shapeConfig->type != Physics::Box2DShapeConfig::Type::None;
    };

    bool registryFallbackUsed = false;
    bool attemptedRegistryFallback = false;
    if ((!colliderHasShapes(colliderBuild) || !shapeConfigIsValid(colliderBuild)) && !playerConfigAvailable)
    {
        attemptedRegistryFallback = true;

        {
            std::ostringstream oss;
            oss << kLogPrefix << "[PlayerCollider] Player collider config unavailable; attempting registry fallback for key='"
                << resolvedColliderKey << "'.\n";
            DebugTrace(oss.str());
        }

        PlayerColliderBuildResult registryBuild = BuildPlayerColliderFromRegistry(
            resolvedColliderKey,
            trans,
            m_playerColliderTuning,
            m_playerPickupBoxWidth,
            m_playerPickupBoxHeight,
            playerBodyType);

        if (colliderHasShapes(registryBuild) && shapeConfigIsValid(registryBuild))
        {
            colliderBuild = registryBuild;
            registryFallbackUsed = true;
        }
        else
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[PlayerCollider] Registry fallback failed for key='" << resolvedColliderKey
                << "'; collider will remain unavailable.\n";
            DebugTrace(oss.str());
        }
    }
    else if ((!colliderHasShapes(colliderBuild) || !shapeConfigIsValid(colliderBuild)) && playerConfigAvailable)
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
    const std::string appliedSource = registryFallbackUsed
        ? "RegistryFallback"
        : colliderBuild.source;

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
            << " appliedSource=" << appliedSource
            << " key='" << resolvedColliderKey << "' forceConfig=" << std::boolalpha << m_forceConfigPlayerCollider
            << " configLoaded=" << m_playerColliderConfigLoaded
            << " configAvailable=" << hasConfigDefinition
            << " registryHit=" << colliderBuild.registryHit
            << " registryShapes=" << colliderBuild.registryHasShapes
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
        m_physicsMachine.get(),
        m_sweetsManager.get(),
        Collision::CollisionLayer::Player,
        playerIsKinematic,
        Core::UpdatePhase::Gameplay,
        0,
        "player");

    if (player)
    {
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
            RegisterPhysicsBody(*player);
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
                << (attemptedRegistryFallback ? " (registry fallback unavailable)" : "")
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
    // 数字テクスチャをロード
    m_numberTexture = new Texture();
    m_numberTexture->Create("Assets/Texture/number.png"); 

    // タイマー表示を初期化
    m_timerDisplay.Init(m_numberTexture);
    m_timerDisplay.SetPosition(700.0f, 50.0f);  // 画面右上
    m_timerDisplay.SetDigitSize(50.0f, 70.0f);   // 数字のサイズ

}

SceneGame::~SceneGame()
{
    if (m_hudRenderer)
    {
        m_hudRenderer->Shutdown();
        m_hudRenderer.reset();
    }

    UnregisterAllPhysicsBodies();
    if (m_inputRegistered)
    {
        if (auto* input = GetInput())
        {
            Unregister(*input);
        }
        m_inputRegistered = false;
    }

    if (m_physicsRegistered && m_physicsMachine)
    {
        GetUpdateScheduler().Unregister(*m_physicsMachine);
        m_physicsRegistered = false;
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

        if (m_numberTexture)
        {
            delete m_numberTexture;
            m_numberTexture = nullptr;
        }
        if (m_pManager)
        {
            m_pManager->StopAllSounds();
        }
}

void SceneGame::ConfigureUpdates()
{
        if (m_physicsMachine && !m_physicsRegistered)
        {
                GetUpdateScheduler().Register(*m_physicsMachine);
                m_physicsRegistered = true;

                std::ostringstream oss;
                oss << kLogPrefix << "ConfigureUpdates registered PhysicsMachine (phase="
                    << static_cast<int>(m_physicsMachine->GetPhase())
                    << ", priority=" << m_physicsMachine->GetPriority() << ").\n";
                ::DebugTrace(oss.str());
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

                const bool tracked = m_registeredPhysicsBodies.find(player.get()) != m_registeredPhysicsBodies.end();
                if (!tracked)
                {
                        RegisterPhysicsBody(*player);
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
                        RegisterPhysicsBody(*player);

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
    ValidatePlayerPhysicsRegistration();
    UpdateCountdown(ctx);

    if (m_guidelineTriggerCooldownSeconds > 0.0f)
    {
        m_guidelineTriggerCooldownSeconds = std::max(0.0f, m_guidelineTriggerCooldownSeconds - ctx.dt);
    }

    // スイーツの状態更新
    if (m_sweetsManager)
    {
        m_sweetsManager->OnUpdate(ctx);
    }

    const auto& stacked = m_sweetsManager->GetByState(SweetsSpace::SweetsState::Stacked);
    ::DebugTrace("[DEBUG] stacked.size=" + std::to_string(stacked.size()) + "\n");
    for (const auto& sweets : stacked)
    {
        if (!sweets) continue;
        const AABB bounds = sweets->GetAABB();
        ::DebugTrace("[DEBUG] sweetsTop=" + std::to_string(bounds.max.y) + "\n");
    }

    //高さボーナス判定
    UpdateTowerHeightTelemetry();

    
    
    float viewportWidth = static_cast<float>(SCREEN_WIDTH);
    float viewportHeight = static_cast<float>(SCREEN_HEIGHT);
    bool viewportFromDefault = true;
    bool boundsIssueThisFrame = false;
    if (RenderTarget* rtv = GetDefaultRTV())
    {
        viewportWidth = static_cast<float>(rtv->GetWidth());
        viewportHeight = static_cast<float>(rtv->GetHeight());
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

        if (changed)
        {
            std::ostringstream oss;
            oss << "[SceneGame] [PlayerBounds] camX=" << camPos.x
                << " d=" << d
                << " fovX=" << DirectX::XMConvertToDegrees(fovX)
                << " halfWidth=" << halfWidth
                << " => minX=" << minX
                << " maxX=" << maxX << "\n";
            ::DebugTrace(oss.str());
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

        if (m_cameraAnchor && !m_pObjects.empty())
        {
            auto& anchorTransform = m_cameraAnchor->GetTransform();
            anchorTransform.position = m_pObjects.front()->GetPosition();
            anchorTransform.rotation = m_pObjects.front()->GetfRads();
            anchorTransform.scale = m_pObjects.front()->GetScale();
        }

        float triggerHeight = 0.0f;
        if (m_guidelineTriggerCooldownSeconds <= 0.0f
            && CheckGuidelineCrossing(triggerHeight))
        {
            HandleGuidelineAdvance(triggerHeight);
        }
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
}

bool SceneGame::CheckGuidelineCrossing(float& outTriggerHeight)
{
    outTriggerHeight = 0.0f;

    if (!m_sweetsManager)
    {
        return false;
    }

    if (!(m_guidelineHeightMeters > 0.0f))
    {
        return false;
    }

    const SweetsSpace::SweetsState states[] = {
        SweetsSpace::SweetsState::Active,
        SweetsSpace::SweetsState::Stacked,
        SweetsSpace::SweetsState::Ground,
        SweetsSpace::SweetsState::Carried,
    };

    for (SweetsSpace::SweetsState state : states)
    {
        if (state == SweetsSpace::SweetsState::Carried)
        {
            continue;
        }

        auto& list = m_sweetsManager->GetByState(state);
        for (const auto& sweets : list)
        {
            if (!sweets || sweets->IsPendingDestroy())
            {
                continue;
            }

            sweets->SyncColliderBounds();
            const AABB bounds = sweets->GetAABB();
            if (bounds.max.y >= m_guidelineHeightMeters)
            {
                outTriggerHeight = bounds.max.y;
                return true;
            }
        }
    }

    return false;
}

void SceneGame::OnGuidelineCrossed()
{
    if (!(m_guidelineHeightMeters > 0.0f) || !(m_guidelineStepMeters > 0.0f))
    {
        m_guidelineTriggerCooldownSeconds = kGuidelineGlobalCooldownSeconds;
        return;
    }

    const float previousHeight = m_guidelineHeightMeters;
    m_guidelineHeightMeters += m_guidelineStepMeters;
    m_guidelineInvalidLogged = false;
    m_guidelineClampLogged = false;

    std::ostringstream oss;
    oss << kLogPrefix << "[Guideline] Advanced threshold from " << previousHeight
        << " to " << m_guidelineHeightMeters << " step=" << m_guidelineStepMeters << "\n";
    ::DebugTrace(oss.str());
    m_guidelineTriggerCooldownSeconds = kGuidelineGlobalCooldownSeconds;
}

void SceneGame::ApplyGuidelineBonus(float triggerHeight)
{
    if (!(kGuidelineTimeBonusSeconds > 0.0f))
    {
        return;
    }

    PendingTimeBonus bonus{};
    bonus.seconds = kGuidelineTimeBonusSeconds;
    bonus.triggerHeight = triggerHeight;
    m_pendingTimeBonuses.push_back(bonus);

    std::ostringstream oss;
    oss << kLogPrefix << "[Countdown] guidelineBonus +=" << std::fixed << std::setprecision(2)
        << bonus.seconds << "s triggerHeight=" << triggerHeight
        << "m queue=" << m_pendingTimeBonuses.size() << "\n";
    ::DebugTrace(oss.str());
}

void SceneGame::HandleGuidelineAdvance(float triggerHeight)
{
    const bool advanced = m_phaseManager.AdvancePhase();
    if (advanced)
    {
        BroadcastPhaseAdvance(m_phaseManager.GetCurrentPhase());
    }

    ApplyGuidelineBonus(triggerHeight);
    OnGuidelineCrossed();
}

#if defined(_DEBUG)
void SceneGame::HandleColliderDebugCommands(Core::UpdateContext& ctx)
{
        if (!player)
        {
                return;
        }

        Core::Input& input = ctx.input;
        if (input.IsTrigger(VK_F4))
        {
                g_DrawColliders = !g_DrawColliders;
                std::ostringstream oss;
                oss << kLogPrefix << "[ColliderDebug] r.DebugColliders="
                        << (g_DrawColliders ? 1 : 0) << " (F4).\n";
                DebugTrace(oss.str());
        }

        if (input.IsTrigger(VK_F5))
        {
                g_DrawColliderBounds = !g_DrawColliderBounds;
                std::ostringstream oss;
                oss << kLogPrefix << "[ColliderDebug] r.DebugColliderBounds="
                        << (g_DrawColliderBounds ? 1 : 0) << " (F5).\n";
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

bool SceneGame::LoadStageLayout(const std::string& csvPath)
{
    // CSVファイルを開く
    std::ifstream file(csvPath);
    if (!file.is_open())
    {
        DebugTrace(std::string(kLogPrefix) + "StageLayout.csvを開けませんでした\n");
        return false;
    }

    DebugTrace(std::string(kLogPrefix) + "CSVファイルを開きました: " + csvPath + "\n");

    const bool registryLoaded = Physics::ColliderRegistry::Instance().LoadFromFile(kColliderRegistryPath);
    if (!registryLoaded)
    {
        DebugTrace(std::string(kLogPrefix) + "Stage collider registry missing; falling back to cached definitions if any.\n");
    }

    const StageColliderConfig stageColliderConfig = LoadStageColliderConfig(kStageColliderConfigPath);

    Render::ModelCache& cache = Render::ModelCache::Instance();
    std::string line;

    // ファイルを1行ずつ読み込む
    while (std::getline(file, line))
    {
        // コメント行と空行をスキップ
        if (line.empty() || line[0] == '#')
            continue;

        // カンマ区切りでパース
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> values;

        while (std::getline(ss, token, ','))
        {
            values.push_back(token);
        }

        // 最低15個の値が必要（spin情報を含む）
        if (values.size() < 15)
            continue;

        // ステージオブジェクトを作成
        StageObject obj;
        obj.modelPath = values[0];

        // 位置を設定
        obj.transform.position.x = std::stof(values[1]);
        obj.transform.position.y = std::stof(values[2]);
        obj.transform.position.z = std::stof(values[3]);

        // 回転を設定（度数）
        obj.transform.rotation.x = std::stof(values[4]);
        obj.transform.rotation.y = std::stof(values[5]);
        obj.transform.rotation.z = std::stof(values[6]);

        // スケールを設定
        obj.transform.scale.x = std::stof(values[7]);
        obj.transform.scale.y = std::stof(values[8]);
        obj.transform.scale.z = std::stof(values[9]);

        // 回転アニメーション設定
        std::string spinValue = values[10];
        // 小文字に変換
        std::transform(spinValue.begin(), spinValue.end(), spinValue.begin(), ::tolower);
        
        obj.spinEnabled = (spinValue == "true" || spinValue == "1" || spinValue == "yes" || spinValue == "on");
        obj.spinAxis.x = std::stof(values[11]);
        obj.spinAxis.y = std::stof(values[12]);
        obj.spinAxis.z = std::stof(values[13]);
        obj.spinSpeed = std::stof(values[14]);
        obj.currentRotation = 0.0f;
        if (values.size() > 15)
        {
            obj.colliderPresetId = NormalizeColliderKey(values[15]);
        }

        // モデルをロード
        DebugTrace(std::string(kLogPrefix) + "モデルをロード中\n");
        obj.model = cache.Get(obj.modelPath);
        if (!obj.model)
        {
            DebugTrace(std::string(kLogPrefix) + "モデルのロードに失敗: " + obj.modelPath + "\n");
            continue;
        }
        else
        {
            DebugTrace(std::string(kLogPrefix) + "モデルをロード成功\n");
        }

        if (m_physicsMachine)
        {
            const auto preset = ResolveStageColliderPreset(stageColliderConfig, obj.colliderPresetId, obj.modelPath);
            if (preset)
            {
                obj.colliderPresetId = preset->presetId;

                auto registryDefinition = Physics::ColliderRegistry::Instance().Find(preset->registryId);
                if (!registryDefinition)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[StageColliders] registryId='" << preset->registryId
                        << "' missing for model '" << obj.modelPath << "' (preset='" << preset->presetId << "').\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    Physics::BoxColliderComponent colliderComponent{};
                    colliderComponent.colliderId = preset->registryId;
                    colliderComponent.shapeDef = registryDefinition->shapeDef;
                    colliderComponent.shapes = registryDefinition->shapes;
                    colliderComponent.colliderOffset = preset->offset;

                    Physics::Box2DShapeConfig shapeConfig{};
                    shapeConfig.type = InferShapeType(colliderComponent.shapes);
                    shapeConfig.bodyType = preset->bodyType;
                    shapeConfig.fixedRotation = preset->fixedRotation;
                    shapeConfig.allowRotation = preset->allowRotation;
                    shapeConfig.angularDamping = preset->allowRotation ? 0.2f : 0.0f;
                    shapeConfig.density = std::max(0.0f, registryDefinition->shapeDef.density);
                    shapeConfig.friction = std::max(0.0f, registryDefinition->shapeDef.material.friction);
                    shapeConfig.restitution = std::clamp(registryDefinition->shapeDef.material.restitution, 0.0f, 1.0f);

                    auto entity = std::make_shared<Entities::PhysicalEntity>(
                        obj.transform,
                        shapeConfig,
                        preset->layer,
                        obj.model,
                        nullptr,
                        Core::UpdatePhase::PrePhysics,
                        0,
                        preset->presetId,
                        false);

                    entity->SetBoxColliderComponent(colliderComponent);
                    entity->SetAllowMeshPhysics(false);
                    obj.physicsEntity = entity;

                    RegisterPhysicsBody(*entity);

                    std::ostringstream oss;
                    oss << kLogPrefix << "[StageColliders] Registered preset='" << preset->presetId
                        << "' for model '" << obj.modelPath << "' (layer=" << static_cast<int>(preset->layer)
                        << ")\n";
                    DebugTrace(oss.str());
                }
            }
            else
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[StageColliders] No preset mapped for model '" << obj.modelPath
                    << "'; collider skipped.\n";
                DebugTrace(oss.str());
            }
        }

        // リストに追加
        m_stageObjects.push_back(obj);
    }

    DebugTrace(std::string(kLogPrefix) + "ステージオブジェクトを" + std::to_string(m_stageObjects.size()) + "個ロードしました\n");
    return true;
}

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
            DebugTrace(std::string(kLogPrefix) + "描画開始: " + std::to_string(m_stageObjects.size()) + "個のオブジェクト\n");

            for (const auto& obj : m_stageObjects)
            {
                if (obj.model)
                {
                    DebugTrace(std::string(kLogPrefix) + "描画中: " + obj.modelPath + "\n");

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

                    m_render->Draw(*obj.model, renderTransform, activeCamera.get());
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

        if (m_hudRenderer)
        {
                HudDisplayData hudData{};
                  hudData.remainingSeconds = std::max(0.0f, m_remainingCountdownSeconds);
                  hudData.currentHeight = m_currentTowerHeight;
                  hudData.peakHeight = m_peakTowerHeight;
                  hudData.score = ComputeScore();
                  hudData.sweetsCount = GetSweetsCount();
                  hudData.timeUp = (m_stageState == StageState::TimeUp)
                      || (m_stageState == StageState::Cleared)
                      || (m_stageState == StageState::ShowingResult);
                  hudData.moveSpeedMultiplier = m_playerSpeedMultiplier;
                  if (player)
                  {
                          hudData.moveSpeedUnitsPerSecond = player->GetMoveSpeed();
                  }

                  float guidelineClip = 0.0f;
                  if (TryGetGuidelineClipY(*activeCamera, guidelineClip))
                  {
                          hudData.guidelineVisible = true;
                        hudData.guidelineClipY = guidelineClip;
                }

                m_hudRenderer->Draw(hudData);
        }

        m_timerDisplay.Draw(m_remainingCountdownSeconds);

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

void SceneGame::RegisterPhysicsBody(Entities::PhysicalEntity& entity)
{
    if (!m_physicsMachine)
        return;

    const bool isPlayerEntity = (player && player.get() == &entity);
    const auto& boxColliderComponent = entity.GetBoxColliderComponent();
    const bool hasBoxColliderComponent = boxColliderComponent && !boxColliderComponent->Empty();
    const auto& shapeConfig = entity.GetBox2DShapeConfig();
    const bool hasShapeConfig = shapeConfig && shapeConfig->type != Physics::Box2DShapeConfig::Type::None;
    const bool hasMeshData = entity.HasBox2DMesh() || !entity.GetBox2DBoundary().empty();
    if (!hasBoxColliderComponent && !hasShapeConfig && !hasMeshData)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Cannot register physics for tag='" << entity.GetTag()
            << "': collider component, shape config, and mesh data are all missing.\n";
        DebugTrace(oss.str());
        return;
    }

    const auto insertResult = m_registeredPhysicsBodies.insert(&entity);
    if (!insertResult.second)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Skipping duplicate physics registration for tag='" << entity.GetTag() << "'\n";
        DebugTrace(oss.str());
        return;
    }

    entity.AttachPhysicsMachine(m_physicsMachine.get());

    {
        bool hasBody = entity.HasBox2DBody();
        int fixtureCount = boxColliderComponent ? boxColliderComponent->createdFixtureCount : 0;
        if (!hasBody || fixtureCount <= 0)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Physics body incomplete for tag='" << entity.GetTag()
                << "' (hasBody=" << std::boolalpha << hasBody
                << ", fixtures=" << fixtureCount << "); attempting immediate rebuild.\n";
            ::DebugTrace(oss.str());

            entity.DetachPhysicsMachine();
            entity.AttachPhysicsMachine(m_physicsMachine.get());

            const bool rebuiltHasBody = entity.HasBox2DBody();
            const int rebuiltFixtures = boxColliderComponent ? boxColliderComponent->createdFixtureCount : 0;
            if (!rebuiltHasBody || rebuiltFixtures <= 0)
            {
                std::ostringstream errorOss;
                errorOss << kLogPrefix << "Failed to create physics body for tag='" << entity.GetTag()
                    << "' even after rebuild attempt (hasBody=" << std::boolalpha << rebuiltHasBody
                    << ", fixtures=" << rebuiltFixtures << ").\n";
                ::DebugTrace(errorOss.str());

                entity.DetachPhysicsMachine();
                m_registeredPhysicsBodies.erase(&entity);
                return;
            }

            hasBody = rebuiltHasBody;
            fixtureCount = rebuiltFixtures;

            std::ostringstream recovered;
            recovered << kLogPrefix << "Recovered physics body for tag='" << entity.GetTag()
                << "' (fixtures=" << rebuiltFixtures << ").\n";
            ::DebugTrace(recovered.str());
        }

        float gravityScale = hasBody ? entity.GetGravityScale() : 0.0f;
        if (isPlayerEntity && hasBody && std::fabs(gravityScale - 0.0f) > std::numeric_limits<float>::epsilon())
        {
            entity.SetGravityScale(0.0f);
            gravityScale = entity.GetGravityScale();
            DebugTrace(std::string(kLogPrefix) + "Player gravity disabled to prevent unintended falling.\n");
        }

        std::ostringstream oss;
        oss << kLogPrefix << "Physics registration complete for tag='" << entity.GetTag()
            << "' hasBody=" << std::boolalpha << hasBody
            << " fixtures=" << fixtureCount
            << " gravityScale=" << gravityScale
            << ((hasBody && fixtureCount > 0) ? "" : " (incomplete)") << "\n";
        ::DebugTrace(oss.str());
    }
}

void SceneGame::UnregisterPhysicsBody(Entities::PhysicalEntity* entity)
{
    if (!entity)
    {
        DebugTrace(std::string(kLogPrefix) + "UnregisterPhysicsBody received null entity pointer.\n");
        return;
    }

    const bool fixtureBuildFailed = entity->HasFixtureBuildFailure();

    const auto tracked = m_registeredPhysicsBodies.find(entity);
    if (tracked == m_registeredPhysicsBodies.end())
    {
        if (!fixtureBuildFailed)
        {
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << kLogPrefix << "UnregisterPhysicsBody called for untracked entity=" << entity << "\n";
            ::DebugTrace(oss.str());
#endif
        }
    }
    else
    {
        m_registeredPhysicsBodies.erase(tracked);
    }

    if (m_physicsMachine)
    {
        entity->DetachPhysicsMachine();
    }
    else
    {
        if (!fixtureBuildFailed)
        {
#if defined(_DEBUG)
            DebugTrace(std::string(kLogPrefix) + "Physics machine missing during UnregisterPhysicsBody.\n");
#endif
        }
    }
}

void SceneGame::ValidatePlayerPhysicsRegistration()
{
    if (!player || !m_physicsMachine)
    {
        return;
    }

    const auto& physicsBodies = m_physicsMachine->GetBodies();
    const bool registeredWithPhysics = std::find(physicsBodies.begin(), physicsBodies.end(), player.get()) != physicsBodies.end();
    if (!registeredWithPhysics)
    {
        RegisterPhysicsBody(*player);
        std::ostringstream oss;
        oss << kLogPrefix << "Player body missing from PhysicsMachine list; re-registering (kinematic="
            << std::boolalpha << player->IsKinematic()
            << ", bodyType=" << (player->GetPhysical().IsDynamic() ? "dynamic" : "static") << ").\n";
        ::DebugTrace(oss.str());
        m_loggedPlayerBodyRecovery = true;
    }
    else if (m_loggedPlayerBodyRecovery)
    {
        DebugTrace(std::string(kLogPrefix) + "Player body present in PhysicsMachine after prior recovery.\n");
        m_loggedPlayerBodyRecovery = false;
    }
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
    oss << kLogPrefix << "[Stage] cleared peakTop=" << std::fixed << std::setprecision(2)
        << m_peakTowerHeight << "m score=" << ComputeScore() << "\n";
    ::DebugTrace(oss.str());
}

void SceneGame::OnStageFail()
{
    ToggleGameplaySystems(false);

    std::ostringstream oss;
    oss << kLogPrefix << "[Countdown] timeUp peakTop=" << std::fixed << std::setprecision(2)
        << m_peakTowerHeight << "m score=" << ComputeScore() << "\n";
    ::DebugTrace(oss.str());
}

void SceneGame::UnregisterAllPhysicsBodies()
{
    if (m_registeredPhysicsBodies.empty())
    {
        return;
    }

    std::vector<Entities::PhysicalEntity*> bodies(m_registeredPhysicsBodies.begin(), m_registeredPhysicsBodies.end());
    for (Entities::PhysicalEntity* body : bodies)
    {
        UnregisterPhysicsBody(body);
    }
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

    if (m_cameraAnchor && !m_pObjects.empty())
    {
        auto& anchorTransform = m_cameraAnchor->GetTransform();
        anchorTransform.position = m_pObjects.front()->GetPosition();
        anchorTransform.rotation = m_pObjects.front()->GetfRads();
        anchorTransform.scale = m_pObjects.front()->GetScale();
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

void SceneGame::UpdateCountdown(Core::UpdateContext& ctx)
{
    if (m_stageState != StageState::Playing)
    {
        return;
    }

    if (!m_pendingTimeBonuses.empty())
    {
        while (!m_pendingTimeBonuses.empty())
        {
            const PendingTimeBonus bonus = m_pendingTimeBonuses.front();
            m_pendingTimeBonuses.pop_front();
            if (!(bonus.seconds > 0.0f))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[Countdown] Ignored non-positive bonus event seconds="
                    << bonus.seconds << " queue=" << m_pendingTimeBonuses.size() << "\n";
                ::DebugTrace(oss.str());
                continue;
            }

            m_remainingCountdownSeconds = std::max(0.0f, m_remainingCountdownSeconds);
            m_remainingCountdownSeconds += bonus.seconds;
            m_totalPlaySeconds += bonus.seconds;

            std::ostringstream oss;
            oss << kLogPrefix << "[Countdown] bonusApplied +=" << std::fixed << std::setprecision(2)
                << bonus.seconds << "s height=" << std::fixed << std::setprecision(2)
                << bonus.triggerHeight << "m remaining=" << std::fixed << std::setprecision(2)
                << m_remainingCountdownSeconds << "s queue=" << m_pendingTimeBonuses.size() << "\n";
            ::DebugTrace(oss.str());
        }
    }

    const float delta = m_countdownTimer.Tick(ctx.dt);
    if (!(delta > 0.0f))
    {
        return;
    }

    const float previousRemaining = m_remainingCountdownSeconds;
    m_remainingCountdownSeconds = std::max(0.0f, m_remainingCountdownSeconds - delta);

    const int previousSeconds = static_cast<int>(std::ceil(previousRemaining));
    const int currentSeconds = static_cast<int>(std::ceil(m_remainingCountdownSeconds));
    if (currentSeconds < previousSeconds)
    {
        const int intervalSeconds = std::max(1, static_cast<int>(kCountdownLogIntervalSeconds));
        const bool shouldLogInterval = (currentSeconds % intervalSeconds == 0);
        const bool shouldLogWarning = currentSeconds <= 3;
        if ((shouldLogInterval || shouldLogWarning) && currentSeconds != m_lastAnnouncedCountdownSeconds)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Countdown] remaining=" << std::fixed << std::setprecision(2)
                << m_remainingCountdownSeconds << "s\n";
            ::DebugTrace(oss.str());
            m_lastAnnouncedCountdownSeconds = currentSeconds;
        }
    }

    if (m_remainingCountdownSeconds <= 0.0f && m_stageState == StageState::Playing)
    {
        SetStageState(StageState::TimeUp);
    }
}

void SceneGame::UpdateTowerHeightTelemetry()
{
    if (!m_sweetsManager)
    {
        ::DebugTrace("[ERROR] m_sweetsManager is nullptr!\n");
        return;
    }

    const SweetsSpace::SweetsState states[] = {
        SweetsSpace::SweetsState::Active,
        SweetsSpace::SweetsState::Stacked,
        SweetsSpace::SweetsState::Ground,
    };

    float highestTop = 0.0f;

    for (SweetsSpace::SweetsState state : states)
    {
        auto& list = m_sweetsManager->GetByState(state);
        for (const auto& sweets : list)
        {
            if (!sweets || sweets->IsPendingDestroy())
            {
                continue;
            }

            sweets->SyncColliderBounds();
            const AABB bounds = sweets->GetAABB();
            highestTop = std::max(highestTop, bounds.max.y);
        }
    }

    m_currentTowerHeight = highestTop;
    m_maxTowerHeight = std::max(m_maxTowerHeight, highestTop);

    if (highestTop > m_peakTowerHeight)
    {
        m_peakTowerHeight = highestTop;
        std::ostringstream oss;
        oss << kLogPrefix << "[Tower] newPeakTop=" << std::fixed << std::setprecision(2)
            << m_peakTowerHeight << "m remaining=" << std::fixed << std::setprecision(2)
            << m_remainingCountdownSeconds << "s\n";
        ::DebugTrace(oss.str());
    }
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
        oss << kLogPrefix << "[Result] Confirm accepted peakTop=" << std::fixed << std::setprecision(2)
            << m_peakTowerHeight << "m score=" << ComputeScore() << "\n";
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

bool SceneGame::TryGetGuidelineClipY(const ::CameraSpace::ICamera& camera, float& outClipY) const
{
    if (m_guidelineHeightMeters <= 0.0f)
    {
        if (!m_guidelineInvalidLogged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Guideline] Disabled because height=" << m_guidelineHeightMeters << "\n";
            ::DebugTrace(oss.str());
            m_guidelineInvalidLogged = true;
        }
        return false;
    }

    const auto* baseCamera = dynamic_cast<const ::CameraSpace::BaseCamera*>(&camera);
    if (!baseCamera)
    {
        if (!m_guidelineInvalidLogged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Guideline] Unsupported camera type for guideline computation.\n";
            ::DebugTrace(oss.str());
            m_guidelineInvalidLogged = true;
        }
        return false;
    }

    const float cameraWorldY = baseCamera->GetPosition().y;
    if (!std::isfinite(cameraWorldY))
    {
        if (!m_guidelineInvalidLogged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Guideline] Non-finite camera Y; guideline disabled.\n";
            ::DebugTrace(oss.str());
            m_guidelineInvalidLogged = true;
        }
        return false;
    }

    const float deltaY = m_guidelineHeightMeters - cameraWorldY;
    const float viewHeightScale = kGuidelineViewHeightScale;
    if (!std::isfinite(viewHeightScale) || viewHeightScale <= std::numeric_limits<float>::epsilon())
    {
        if (!m_guidelineInvalidLogged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Guideline] Invalid viewHeightScale=" << viewHeightScale << "\n";
            ::DebugTrace(oss.str());
            m_guidelineInvalidLogged = true;
        }
        return false;
    }

    float halfThickness = 0.0f;
    if (RenderTarget* rtv = GetDefaultRTV())
    {
        const float viewportHeight = static_cast<float>(rtv->GetHeight());
        if (std::isfinite(viewportHeight) && viewportHeight > 0.0f)
        {
            const float thicknessClip = (kHudLayoutConfig.guidelineThicknessPixels / viewportHeight) * 2.0f;
            halfThickness = std::max(0.0f, thicknessClip * 0.5f);
        }
    }

    const float rawClipY = deltaY / viewHeightScale;
    if (!std::isfinite(rawClipY))
    {
        if (!m_guidelineInvalidLogged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Guideline] Non-finite rawClipY from deltaY=" << deltaY
                << " viewHeightScale=" << viewHeightScale << "\n";
            ::DebugTrace(oss.str());
            m_guidelineInvalidLogged = true;
        }
        return false;
    }

    const float minClip = -1.0f + halfThickness;
    const float maxClip = 1.0f - halfThickness;
    outClipY = std::clamp(rawClipY, minClip, maxClip);
    if ((rawClipY < minClip) || (rawClipY > maxClip))
    {
        if (!m_guidelineClampLogged && std::isfinite(halfThickness))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Guideline] Clamped clipY from " << rawClipY
                << " to " << outClipY << " halfThickness=" << halfThickness
                << " threshold=" << m_guidelineHeightMeters << "\n";
            ::DebugTrace(oss.str());
            m_guidelineClampLogged = true;
        }
    }
    else
    {
        m_guidelineClampLogged = false;
    }

    m_guidelineInvalidLogged = false;
    return true;
}

void SceneGame::ToggleGameplaySystems(bool enabled)
{
    if (m_physicsMachine)
    {
        m_physicsMachine->Enable(enabled);
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


int SceneGame::ComputeScore() const
{
    const float rawPhaseMultiplier = m_phaseManager.GetRewardMultiplier();
    const float phaseMultiplier = (std::isfinite(rawPhaseMultiplier) && rawPhaseMultiplier > 0.0f)
        ? rawPhaseMultiplier
        : 1.0f;
    const float totalMultiplier = kBaseScorePerMeter * phaseMultiplier;
    const int score = static_cast<int>(std::lround(m_peakTowerHeight * totalMultiplier));

    const bool multiplierChanged = !std::isfinite(m_lastScoreLogMultiplier)
        || std::fabs(m_lastScoreLogMultiplier - phaseMultiplier) > std::numeric_limits<float>::epsilon();
    if (score != m_lastScoreLogValue || multiplierChanged)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[Score] peak=" << std::fixed << std::setprecision(2) << m_peakTowerHeight
            << "m phase=" << m_phaseManager.GetCurrentPhase()
            << " phaseMultiplier=" << phaseMultiplier
            << " basePerMeter=" << kBaseScorePerMeter
            << " totalMultiplier=" << totalMultiplier
            << " score=" << score << "\n";
        ::DebugTrace(oss.str());
        m_lastScoreLogValue = score;
        m_lastScoreLogMultiplier = phaseMultiplier;
    }

    return score;
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
