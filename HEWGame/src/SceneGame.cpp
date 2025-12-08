#include "Collision2D.h"
#include "Transform.h"
#include "SceneGame.h"
#include "Collision2D.h"
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
#include "SweetsManager.h"
#include "SweetsPrototypeManifest.h"
#include "BackgroundRenderer.h"
#include "Sweets/BaseSweets.h"
#include "Entities/Player.h"
#include "Collision/ConvexHullBuilder.h"
#include "ColliderConfigLoader.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <initializer_list>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <iterator>
#include <DirectXMath.h>
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
        constexpr const char* kStageConfigPath = "Assets/Config/stage_default.cfg";
        constexpr const char* kSweetsSpawnConfigPath = "Assets/Config/sweets_spawn.cfg";
        constexpr const char* kPlayerColliderConfigPath = "Assets/Config/colliders_player.cfg";
        constexpr const char* kPlayerColliderTuningPath = "Assets/Config/player_collider.csv";
        constexpr const char* kPlayerTransformConfigPath = "Assets/Config/player_transform.cfg";
        constexpr float kDefaultPlaySeconds = 30.0f;
        constexpr float kCountdownLogIntervalSeconds = 5.0f;
        constexpr float kBaseScorePerMeter = 100.0f;
        constexpr float kPlayerSpeedBoostStep = 1.1f;
        constexpr float kMaxPlayerSpeedMultiplier = 3.0f;
        constexpr float kFallbackPlayerMoveSpeed = 15.0f;
        constexpr const char* kFallbackPlayerModelPath = "Assets/Model/SweetsPrototype/FairyPlaceholder.obj";
        constexpr float kDefaultGeneratedThicknessPadding = 1.1f;
        constexpr float kMinGeneratedThickness = 0.05f;
        constexpr float3 kDefaultPlayerColliderPadding{ 0.02f, 0.05f, 0.02f };
        constexpr float3 kDefaultPlayerColliderOffset{ 0.0f, 0.0f, 0.0f };
        constexpr float3 kDefaultPlayerFallbackPadding{ 0.02f, 0.05f, 0.02f };
        constexpr float3 kDefaultPlayerFallbackOffset{ 0.0f, 0.05f, 0.0f };
        constexpr float3 kDefaultPlayerFallbackSize{ 0.6f, 0.9f, 0.12f };
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

                    // aiProcess_Triangulate guarantees triangles
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

                const auto hullResult = Collision::BuildConvexHullFromMesh(desc);
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

        std::shared_ptr<Collision::ICollider> BuildGeneratedPlayerCollider(
            const PlayerColliderGeometry& geometry,
            const SceneGame::PlayerColliderTuning& tuning,
            const Transform& transform)
        {
            if (!geometry.bounds.valid)
            {
                return nullptr;
            }

            const float clampedThickness = std::max(geometry.paddedThickness * transform.scale.z, kMinGeneratedThickness);
            const float2 offset2D{ tuning.offset.x, tuning.offset.y };
            const float2 centroid2D{ geometry.centroidOffset.x, geometry.centroidOffset.y };
            auto hull = BuildPaddedHull(geometry, tuning, transform.scale);

            std::vector<float2> hullPointsRelativeToPivot = hull;

            auto collider = std::make_shared<Collision::ConvexHullCollider2D>(hullPointsRelativeToPivot, offset2D, clampedThickness, Collision::CollisionLayer::Player, centroid2D);
            collider->UpdateBounds(transform);
            return collider;
        }

        struct PhysicsTuningSettings
        {
            float gravity = 11.5f;
            int subStepCount = 8;
            float damping = 0.97f;
            float penetrationTolerance = 0.005f;
            float restitution = 0.06f;
            float frictionStatic = 0.85f;
            float frictionDynamic = 0.52f;
            float maxSlopeCos = 0.96f;
            float maxRestitutionVelocity = 5.0f;
            float verticalSnapCos = 0.97f;
            float groundMinCos = 0.92f;
            float baumgarteFactor = 0.18f;
            float maxPenetrationCorrection = 0.1f;
            float minLinearSpeed = 0.05f;
            float minVerticalSpeed = 0.03f;
            float minTangentialSpeed = 0.03f;
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
                    << ", damping=" << settings.damping
                    << ", penetrationTolerance=" << settings.penetrationTolerance
                    << ", restitution=" << settings.restitution
                    << ", frictionStatic=" << settings.frictionStatic
                    << ", frictionDynamic=" << settings.frictionDynamic
                    << ", maxSlopeCos=" << settings.maxSlopeCos
                    << ", maxRestitutionVelocity=" << settings.maxRestitutionVelocity
                    << ", verticalSnapCos=" << settings.verticalSnapCos
                    << ", groundMinCos=" << settings.groundMinCos
                    << ", baumgarteFactor=" << settings.baumgarteFactor
                    << ", maxPenetrationCorrection=" << settings.maxPenetrationCorrection
                    << ", minLinearSpeed=" << settings.minLinearSpeed
                    << ", minVerticalSpeed=" << settings.minVerticalSpeed
                    << ", minTangentialSpeed=" << settings.minTangentialSpeed
                    << ").\n";
                ::DebugTrace(oss.str());
                return settings;
            }

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
                    else if (key == "damping")
                    {
                        settings.damping = std::stof(value);
                    }
                    else if (key == "penetrationTolerance")
                    {
                        settings.penetrationTolerance = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "restitution")
                    {
                        settings.restitution = std::clamp(std::stof(value), 0.0f, 1.0f);
                    }
                    else if (key == "frictionStatic")
                    {
                        settings.frictionStatic = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "frictionDynamic")
                    {
                        settings.frictionDynamic = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "maxSlopeCos")
                    {
                        settings.maxSlopeCos = std::clamp(std::stof(value), 0.0f, 1.0f);
                    }
                    else if (key == "maxRestitutionVelocity")
                    {
                        settings.maxRestitutionVelocity = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "verticalSnapCos")
                    {
                        settings.verticalSnapCos = std::clamp(std::stof(value), 0.0f, 1.0f);
                    }
                    else if (key == "groundMinCos")
                    {
                        settings.groundMinCos = std::clamp(std::stof(value), 0.0f, 1.0f);
                    }
                    else if (key == "baumgarteFactor")
                    {
                        settings.baumgarteFactor = std::clamp(std::stof(value), 0.0f, 1.0f);
                    }
                    else if (key == "maxPenetrationCorrection")
                    {
                        settings.maxPenetrationCorrection = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "minLinearSpeed")
                    {
                        settings.minLinearSpeed = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "minVerticalSpeed")
                    {
                        settings.minVerticalSpeed = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "minTangentialSpeed")
                    {
                        settings.minTangentialSpeed = std::max(0.0f, std::stof(value));
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
                << ", damping=" << settings.damping
                << ", penetrationTolerance=" << settings.penetrationTolerance
                << ", restitution=" << settings.restitution
                << ", frictionStatic=" << settings.frictionStatic
                << ", frictionDynamic=" << settings.frictionDynamic
                << ", maxSlopeCos=" << settings.maxSlopeCos
                << ", maxRestitutionVelocity=" << settings.maxRestitutionVelocity
                << ", verticalSnapCos=" << settings.verticalSnapCos
                << ", groundMinCos=" << settings.groundMinCos
                << ", baumgarteFactor=" << settings.baumgarteFactor
                << ", maxPenetrationCorrection=" << settings.maxPenetrationCorrection
                << ", minLinearSpeed=" << settings.minLinearSpeed
                << ", minVerticalSpeed=" << settings.minVerticalSpeed
                << ", minTangentialSpeed=" << settings.minTangentialSpeed
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
            float3{ 0.0f, 3.5f, -25.0f },
            45.0f,
            DirectX::XMConvertToRadians(0.0f),
            DirectX::XMConvertToRadians(-10.0f)
        };

        constexpr float kPlatformScreenBottomFraction = 0.08f;
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

        bool TryComputeViewportBoundsOnPlane(
            const CameraSpace::PerspectiveCamera& camera,
            float viewportWidth,
            float viewportHeight,
            float planeZ,
            float& outMinX,
            float& outMaxX,
            float& outMinY,
            float& outMaxY)
        {
            if (!(std::isfinite(viewportWidth) && viewportWidth > 0.0f
                && std::isfinite(viewportHeight) && viewportHeight > 0.0f))
            {
                return false;
            }

            const DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&camera.GetView());
            const DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&camera.GetProjection());

            const std::array<std::pair<float, float>, 4> corners
            {
                std::pair<float, float>{0.0f, 0.0f},
                std::pair<float, float>{viewportWidth, 0.0f},
                std::pair<float, float>{viewportWidth, viewportHeight},
                std::pair<float, float>{0.0f, viewportHeight}
            };

            bool hasBounds = false;
            float minX = 0.0f;
            float maxX = 0.0f;
            float minY = 0.0f;
            float maxY = 0.0f;
            constexpr float kDirectionEpsilon = 1e-5f;

            for (const auto& corner : corners)
            {
                const DirectX::XMVECTOR nearPoint = DirectX::XMVector3Unproject(
                    DirectX::XMVectorSet(corner.first, corner.second, 0.0f, 1.0f),
                    0.0f,
                    0.0f,
                    viewportWidth,
                    viewportHeight,
                    0.0f,
                    1.0f,
                    proj,
                    view,
                    DirectX::XMMatrixIdentity());
                const DirectX::XMVECTOR farPoint = DirectX::XMVector3Unproject(
                    DirectX::XMVectorSet(corner.first, corner.second, 1.0f, 1.0f),
                    0.0f,
                    0.0f,
                    viewportWidth,
                    viewportHeight,
                    0.0f,
                    1.0f,
                    proj,
                    view,
                    DirectX::XMMatrixIdentity());

                const DirectX::XMVECTOR direction = DirectX::XMVectorSubtract(farPoint, nearPoint);
                const float dirZ = DirectX::XMVectorGetZ(direction);
                if (std::fabs(dirZ) < kDirectionEpsilon)
                {
                    continue;
                }

                const float nearZ = DirectX::XMVectorGetZ(nearPoint);
                const float t = (planeZ - nearZ) / dirZ;
                if (t < 0.0f || t > 1.0f)
                {
                    continue;
                }

                const DirectX::XMVECTOR hitPoint = DirectX::XMVectorAdd(nearPoint, DirectX::XMVectorScale(direction, t));
                const float hitX = DirectX::XMVectorGetX(hitPoint);
                const float hitY = DirectX::XMVectorGetY(hitPoint);

                if (!hasBounds)
                {
                    minX = maxX = hitX;
                    minY = maxY = hitY;
                    hasBounds = true;
                }
                else
                {
                    minX = std::min(minX, hitX);
                    maxX = std::max(maxX, hitX);
                    minY = std::min(minY, hitY);
                    maxY = std::max(maxY, hitY);
                }
            }

            if (!hasBounds)
            {
                return false;
            }

            outMinX = minX;
            outMaxX = maxX;
            outMinY = minY;
            outMaxY = maxY;
            return true;
        }

        Transform ComputePlatformTransformForCamera(
            const CameraSpace::PerspectiveCamera& camera,
            float viewportWidth,
            float viewportHeight,
            const Stage::PlatformDescriptor& descriptor,
            float screenFraction,
            float stageCenterX,
            std::size_t platformIndex)
        {
            Transform result{};
            result.position = float3{ stageCenterX + descriptor.center.x, descriptor.center.y, descriptor.center.z };
            result.rotation = float3{ 0.0f, 0.0f, 0.0f };
            float targetWidth = descriptor.scale.x;
            float targetHeight = descriptor.scale.y;
            const float targetDepth = std::max(0.01f, descriptor.depth);

            float viewportMinX = 0.0f;
            float viewportMaxX = 0.0f;
            float viewportMinY = 0.0f;
            float viewportMaxY = 0.0f;
            const bool boundsValid = TryComputeViewportBoundsOnPlane(
                camera,
                viewportWidth,
                viewportHeight,
                descriptor.center.z,
                viewportMinX,
                viewportMaxX,
                viewportMinY,
                viewportMaxY);
            if (boundsValid)
            {
                const float worldWidth = viewportMaxX - viewportMinX;
                const float worldHeight = viewportMaxY - viewportMinY;
                const float widthFraction = std::clamp(descriptor.widthFraction, 0.01f, 1.0f);
                const float heightFraction = std::clamp(descriptor.heightFraction, 0.01f, 1.0f);
                targetWidth = std::max(worldWidth * widthFraction, 0.01f);
                targetHeight = std::max(worldHeight * heightFraction, 0.01f);
            }
            else
            {
                DebugTrace(std::string(kLogPrefix) + "[PlatformAlign] Failed to derive viewport bounds; falling back to config platformScale.\n");
            }

            result.scale = float3{ targetWidth, targetHeight, targetDepth };

            const float sanitizedFraction = std::clamp(screenFraction, 0.0f, 1.0f);
            const float configuredTopY = result.position.y + (result.scale.y * 0.5f);
            float derivedTopY = configuredTopY;
            float derivedPlaneZ = result.position.z;
            bool derivedValid = false;

            if (std::isfinite(viewportWidth) && viewportWidth > 0.0f
                && std::isfinite(viewportHeight) && viewportHeight > 0.0f)
            {
                const DirectX::XMFLOAT4X4& viewMatrix = camera.GetView();
                const DirectX::XMFLOAT4X4& projMatrix = camera.GetProjection();
                const DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&viewMatrix);
                const DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&projMatrix);
                const float pixelX = viewportWidth * 0.5f;
                const float pixelY = viewportHeight * (1.0f - sanitizedFraction);
                const float clampedPixelY = std::clamp(pixelY, 0.0f, viewportHeight);
                const DirectX::XMVECTOR nearPoint = DirectX::XMVector3Unproject(
                    DirectX::XMVectorSet(pixelX, clampedPixelY, 0.0f, 1.0f),
                    0.0f,
                    0.0f,
                    viewportWidth,
                    viewportHeight,
                    0.0f,
                    1.0f,
                    proj,
                    view,
                    DirectX::XMMatrixIdentity());
                const DirectX::XMVECTOR farPoint = DirectX::XMVector3Unproject(
                    DirectX::XMVectorSet(pixelX, clampedPixelY, 1.0f, 1.0f),
                    0.0f,
                    0.0f,
                    viewportWidth,
                    viewportHeight,
                    0.0f,
                    1.0f,
                    proj,
                    view,
                    DirectX::XMMatrixIdentity());
                DirectX::XMVECTOR directionVec = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(farPoint, nearPoint));
                DirectX::XMFLOAT3 direction{};
                DirectX::XMStoreFloat3(&direction, directionVec);
                const float3 cameraPosition = camera.GetPosition();
                constexpr float kDirectionEpsilon = 1e-4f;
                if (std::fabs(direction.x) > kDirectionEpsilon)
                {
                    const float t = (stageCenterX - cameraPosition.x) / direction.x;
                    if (t > 0.0f)
                    {
                        derivedValid = true;
                        derivedTopY = cameraPosition.y + direction.y * t;
                        derivedPlaneZ = cameraPosition.z + direction.z * t;
                    }
                }
            }

            if (derivedValid)
            {
                const float deltaY = derivedTopY - configuredTopY;
                const float deltaZ = derivedPlaneZ - result.position.z;
                result.position.y += deltaY;
                result.position.z += deltaZ;
            }

            std::ostringstream oss;
            oss << kLogPrefix << "[PlatformAlign] fraction=" << sanitizedFraction
                << " viewport=" << viewportWidth << 'x' << viewportHeight
                << " platformIndex=" << platformIndex
                << " boundsValid=" << std::boolalpha << boundsValid
                << " scale=(" << result.scale.x << ',' << result.scale.y << ',' << result.scale.z << ')'
                << " topY=" << derivedTopY
                << " worldZ=" << derivedPlaneZ
                << " applied=" << std::boolalpha << derivedValid
                << " centerY=" << result.position.y
                << " planeZ=" << result.position.z << "\n";
            ::DebugTrace(oss.str());

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

    PlayerColliderGeometry playerGeometry{};
    std::string playerColliderKey = "Player";
    m_playerColliderMismatchLogged = false;

    const bool stageLoaded = m_stageData.LoadFromFile(kStageConfigPath);
    m_phaseManager.Initialize(m_stageData.GetPhaseThresholds(), m_stageData.GetPhaseRewardMultipliers());
    m_playerBaseHorizontalSpeed = m_stageData.GetPlayerHorizontalSpeedWorld();
    m_playerBaseVerticalSpeed = m_stageData.GetPlayerVerticalSpeedWorld();
    m_playerPickupBoxWidth = m_stageData.GetPickupBoxWidthWorld();
    m_playerPickupBoxHeight = m_stageData.GetPickupBoxHeightWorld();
    m_playerPickupForwardOffset = m_stageData.GetCellSizeWorld() * 0.5f;
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

    m_playerBaseHorizontalSpeed = sanitizePositive(m_playerBaseHorizontalSpeed, kFallbackPlayerMoveSpeed, "Player horizontal speed");
    m_playerBaseVerticalSpeed = sanitizePositive(m_playerBaseVerticalSpeed, m_playerBaseHorizontalSpeed, "Player vertical speed");
    const float pickupFallback = std::max(m_stageData.GetCellSizeWorld(), 1.0f);
    m_playerPickupBoxWidth = sanitizePositive(m_playerPickupBoxWidth, pickupFallback, "Player pickup width");
    m_playerPickupBoxHeight = sanitizePositive(m_playerPickupBoxHeight, pickupFallback, "Player pickup height");
    m_playerPickupForwardOffset = std::max(0.0f, m_playerPickupForwardOffset);
    m_playerSpeedMultiplier = 1.0f;
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Stage data " << (stageLoaded ? "loaded" : "defaulted")
            << " planeZ=" << m_stageData.GetPlaneZ()
            << " thresholds=" << m_stageData.GetPhaseThresholds().size()
            << " multipliers=" << m_stageData.GetPhaseRewardMultipliers().size()
            << " spawnPoints=" << m_stageData.GetSpawnPoints().size()
            << " platforms=" << m_stageData.GetPlatformCount()
            << " playerSpeedXY=(" << m_playerBaseHorizontalSpeed << ',' << m_playerBaseVerticalSpeed << ')'
            << " pickupBox=(" << m_playerPickupBoxWidth << ',' << m_playerPickupBoxHeight << ')'
            << " pickupOffset=" << m_playerPickupForwardOffset
            << " guidelineHeight=" << m_stageData.GetGuidelineHeight()
            << " heightBonusThreshold=" << m_stageData.GetHeightBonusThreshold()
            << " heightBonusSeconds=" << m_stageData.GetHeightBonusSeconds()
            << " background='" << m_stageData.GetBackgroundTexturePath() << "'\n";
        for (std::size_t i = 0; i < m_stageData.GetPlatformCount(); ++i)
        {
            const auto& descriptor = m_stageData.GetPlatform(i);
            oss << kLogPrefix << " PlatformDesc[" << i << "] center=(" << descriptor.center.x << ',' << descriptor.center.y
                << ',' << descriptor.center.z << ") scale=(" << descriptor.scale.x << ',' << descriptor.scale.y << ','
                << descriptor.scale.z << ") depth=" << descriptor.depth
                << " widthFraction=" << descriptor.widthFraction
                << " heightFraction=" << descriptor.heightFraction << "\n";
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

    NormalizePlayerColliderDefinitions();

    m_guidelineHeightMeters = m_stageData.GetGuidelineHeight();
    if (!std::isfinite(m_guidelineHeightMeters) || m_guidelineHeightMeters <= 0.0f)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Guideline height " << m_guidelineHeightMeters
            << " invalid; overlay guideline disabled.\n";
        ::DebugTrace(oss.str());
        m_guidelineHeightMeters = 0.0f;
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

    m_heightBonusArmed = (m_heightBonusThresholdMeters > 0.0f && m_heightBonusSeconds > 0.0f);

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
    m_physicsMachine = std::make_unique<Systems::PhysicsMachine>(tuning.gravity, tuning.subStepCount);
    m_contactListener = std::make_shared<Systems::Box2DContactListener>();
    m_drawSystem = std::make_unique<Systems::DrawSystem>();
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
    }

    if (m_sweetsManager)
    {
        m_sweetsManager->SetDrawSystem(m_drawSystem.get());
        m_sweetsManager->SetPhysicsMachine(m_physicsMachine.get());
        m_sweetsManager->SetUpdateScheduler(&GetUpdateScheduler());
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
            oss << kLogPrefix << "[SpawnFactory] presets=" << presets.size() << " (key, collider)\n";
            for (const auto& preset : presets)
            {
                oss << kLogPrefix << "  kind='" << preset.first << "' collider="
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
    InitializeStageAnchors();

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

    Render::ModelCache& cache = Render::ModelCache::Instance();
    ID3D11Device* device = GetDevice();
    if (!device)
    {
        DebugTrace(std::string(kLogPrefix) + "Failed to configure ModelCache: device was null.\n");
    }
    else
    {
        cache.SetLoader(m_loader, device, TextureCache::Instance());
    }

    const auto* fairyDescriptor = manifest.Find(SweetsSpace::SweetsType::Fairy);
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

    playerGeometry = AnalyzePlayerModelGeometry(resolvedModelPath, m_playerColliderTuning.thicknessPadding);

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

    // 描画テスト用
    //m_model = cache.Get("Assets/Model/LowPolyNature/Models/OiiaioooooiaiFin.fbx");もう要らないかも

    m_render = std::make_unique<ModelRender>();
    if (m_drawSystem && m_render)
    {
        m_drawSystem->SetRenderer(m_render.get());
    }
    float platformViewportWidth = static_cast<float>(SCREEN_WIDTH);
    float platformViewportHeight = static_cast<float>(SCREEN_HEIGHT);
    if (RenderTarget* rtv = GetDefaultRTV())
    {
        platformViewportWidth = static_cast<float>(rtv->GetWidth());
        platformViewportHeight = static_cast<float>(rtv->GetHeight());
    }

    const auto& platformDescriptors = m_stageData.GetPlatforms();
    if (platformDescriptors.empty())
    {
        DebugTrace(std::string(kLogPrefix) + "No platform descriptors available; skipping foundation creation.\n");
    }
    else
    {
        m_foundationPlatforms.resize(platformDescriptors.size());
        m_platformUpdateRegistered.assign(platformDescriptors.size(), false);
        m_platformDrawRegistered.assign(platformDescriptors.size(), false);

        for (std::size_t i = 0; i < platformDescriptors.size(); ++i)
        {
            const auto& descriptor = platformDescriptors[i];
            Transform platformTransform = ComputePlatformTransformForCamera(
                *m_gameCamera,
                platformViewportWidth,
                platformViewportHeight,
                descriptor,
                kPlatformScreenBottomFraction,
                kStageCenterX,
                i);

            std::ostringstream tagStream;
            tagStream << "StageFoundation" << i;
            Physics::BoxColliderComponent platformCollider;
            platformCollider.shapeDef.density = 0.0f;
            platformCollider.shapeDef.material.friction = 0.8f;
            const float halfWidth = std::max(0.01f, platformTransform.scale.x * 0.5f);
            const float halfHeight = std::max(0.01f, platformTransform.scale.y * 0.5f);
            platformCollider.AddBox(halfWidth, halfHeight, b2Vec2_zero);

            auto platform = std::make_unique<Stage::FoundationPlatform>(
                tagStream.str(),
                platformTransform,
                nullptr,
                platformCollider,
                m_physicsMachine.get(),
                descriptor.color,
                Core::UpdatePhase::PrePhysics,
                -100 - static_cast<int>(i));

            if (!platform)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Failed to allocate foundation platform at index " << i << " (tag='"
                    << tagStream.str() << "').\n";
                ::DebugTrace(oss.str());
                continue;
            }

            m_foundationPlatforms[i] = std::move(platform);
        }

        RefreshPlatformAnchor(true);

        for (std::size_t i = 0; i < m_foundationPlatforms.size(); ++i)
        {
            auto& platform = m_foundationPlatforms[i];
            if (!platform)
            {
                continue;
            }

            RegisterPhysicsBody(*platform);

            const auto& scale = platform->GetTransform().scale;
            const auto& pos = platform->GetTransform().position;
            std::ostringstream oss;
            oss << kLogPrefix << "Stage foundation[" << i << "] initialized at ("
                << pos.x << ',' << pos.y << ',' << pos.z << ") scale=("
                << scale.x << ',' << scale.y << ',' << scale.z << ") color=("
                << platform->GetDebugColor().x << ','
                << platform->GetDebugColor().y << ','
                << platform->GetDebugColor().z << ") tag='" << platform->GetTag() << "'.\n";
            ::DebugTrace(oss.str());

            if (m_drawSystem)
            {
                m_drawSystem->RegisterRenderable(*platform);
                m_platformDrawRegistered[i] = true;
                std::ostringstream drawOss;
                drawOss << kLogPrefix << "Stage foundation[" << i << "] registered with DrawSystem.\n";
                DebugTrace(drawOss.str());
            }
        }

        auto selectPlatform = [&](std::size_t preferredIndex) -> Stage::FoundationPlatform*
        {
            if (preferredIndex < m_foundationPlatforms.size() && m_foundationPlatforms[preferredIndex])
            {
                return m_foundationPlatforms[preferredIndex].get();
            }
            for (auto& platform : m_foundationPlatforms)
            {
                if (platform)
                {
                    return platform.get();
                }
            }
            return nullptr;
        };

        m_platePlatform = selectPlatform(platformDescriptors.size() > 1 ? 1 : 0);
        if (m_platePlatform)
        {
            const auto& pos = m_platePlatform->GetTransform().position;
            const auto& scale = m_platePlatform->GetTransform().scale;
            std::ostringstream oss;
            oss << kLogPrefix << "Plate platform selected tag='" << m_platePlatform->GetTag() << "' pos=("
                << pos.x << ',' << pos.y << ',' << pos.z << ") scale=("
                << scale.x << ',' << scale.y << ',' << scale.z << ").\n";
            DebugTrace(oss.str());
        }
        else
        {
            DebugTrace(std::string(kLogPrefix) + "No plate platform available; player and sweets will use default spawn heights.\n");
        }
    }

    if (m_sweetsManager)
    {
        std::vector<float3> spawnOrigins = m_stageData.GetSpawnPoints();
        if (m_platePlatform)
        {
            const auto& plateTransform = m_platePlatform->GetTransform();
            const float plateTopY = plateTransform.position.y + (plateTransform.scale.y * 0.5f);
            for (auto& origin : spawnOrigins)
            {
                origin.y = plateTopY;
                origin.z = plateTransform.position.z;
            }

            std::ostringstream oss;
            oss << kLogPrefix << "[SpawnOrigin] aligned " << spawnOrigins.size() << " origins to plate '"
                << m_platePlatform->GetTag() << "' topY=" << plateTopY
                << " planeZ=" << plateTransform.position.z << "\n";
            DebugTrace(oss.str());
        }
        m_sweetsManager->SetSpawnOrigins(spawnOrigins);
    }

    const float platePlaneZ = m_platePlatform ? m_platePlatform->GetTransform().position.z : m_stageData.GetPlaneZ();
    std::optional<Physics::BoxColliderComponent> boxColliderComponent;
    std::optional<Physics::Box2DShapeConfig> playerShapeConfig;
    float3 playerColliderHalfExtents{ 0.0f, 0.0f, 0.0f };
    float3 playerColliderOffset{ 0.0f, 0.0f, 0.0f };
    Transform trans = m_playerSpawnTransform.transform;
    trans.position.z += platePlaneZ;
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[PlayerSpawn] transform(model+collider) position=(" << trans.position.x << ',' << trans.position.y << ',' << trans.position.z << ')'
            << " rotationRad=(" << trans.rotation.x << ',' << trans.rotation.y << ',' << trans.rotation.z << ')'
            << " scale=(" << trans.scale.x << ',' << trans.scale.y << ',' << trans.scale.z << ')'
            << " planeZ=" << platePlaneZ
            << " configLoaded=" << std::boolalpha << m_playerSpawnTransform.loadedFromFile
            << " rotationSource=" << (m_playerSpawnTransform.rotationFromDegrees ? "degrees" : "radians_or_default") << "\n";
        DebugTrace(oss.str());
    }

    const bool geometryValid = playerGeometry.bounds.valid;

    const auto buildBoxCollider = [&](const float3& size, const float3& offset)
    {
        const float halfWidth = std::max(0.01f, size.x * 0.5f);
        const float halfHeight = std::max(0.01f, size.y * 0.5f);
        const float halfDepth = std::max(0.01f, size.z * 0.5f);

        Physics::BoxColliderComponent component;
        component.shapeDef.material.friction    = 0.6f;
        component.shapeDef.material.restitution = 0.05f;
        component.AddBox(halfWidth, halfHeight, b2Vec2{ offset.x, offset.y });

        Physics::Box2DShapeConfig shapeConfig{};
        shapeConfig.type = Physics::Box2DShapeConfig::Type::Polygon;
        shapeConfig.bodyType = b2_kinematicBody;
        shapeConfig.density = 0.5f;
        shapeConfig.friction = 0.35f;
        shapeConfig.restitution = 0.05f;
        shapeConfig.vertices =
        {
            b2Vec2{ offset.x - halfWidth, offset.y - halfHeight },
            b2Vec2{ offset.x + halfWidth, offset.y - halfHeight },
            b2Vec2{ offset.x + halfWidth, offset.y + halfHeight },
            b2Vec2{ offset.x - halfWidth, offset.y + halfHeight },
        };

        boxColliderComponent = component;
        playerShapeConfig = shapeConfig;
        playerColliderHalfExtents = float3{ halfWidth, halfHeight, halfDepth };
        playerColliderOffset = offset;
    };

    if (geometryValid)
    {
        float3 colliderSize = ScaleVector(float3{ playerGeometry.bounds.max.x - playerGeometry.bounds.min.x,
            playerGeometry.bounds.max.y - playerGeometry.bounds.min.y,
            playerGeometry.bounds.max.z - playerGeometry.bounds.min.z }, trans.scale);

        float3 colliderOffset = ScaleVector(float3{ (playerGeometry.bounds.min.x + playerGeometry.bounds.max.x) * 0.5f,
            (playerGeometry.bounds.min.y + playerGeometry.bounds.max.y) * 0.5f,
            (playerGeometry.bounds.min.z + playerGeometry.bounds.max.z) * 0.5f }, trans.scale);

        colliderSize.x += m_playerColliderTuning.padding.x * 2.0f;
        colliderSize.y += m_playerColliderTuning.padding.y * 2.0f;
        colliderSize.z = std::max(colliderSize.z * m_playerColliderTuning.thicknessPadding, kMinGeneratedThickness);

        colliderOffset.x += m_playerColliderTuning.offset.x;
        colliderOffset.y += m_playerColliderTuning.offset.y;
        colliderOffset.z += m_playerColliderTuning.offset.z;

        buildBoxCollider(colliderSize, colliderOffset);
    }
    else
    {
        float3 colliderSize = m_playerColliderTuning.fallbackSize;
        float3 colliderOffset{ 0.0f, m_playerColliderTuning.fallbackSize.y * 0.5f, 0.0f };

        colliderSize.x += m_playerColliderTuning.fallbackPadding.x * 2.0f;
        colliderSize.y += m_playerColliderTuning.fallbackPadding.y * 2.0f;
        colliderSize.z = std::max(colliderSize.z + m_playerColliderTuning.fallbackPadding.z * 2.0f, kMinGeneratedThickness);

        colliderOffset.x += m_playerColliderTuning.fallbackOffset.x;
        colliderOffset.y += m_playerColliderTuning.fallbackOffset.y;
        colliderOffset.z += m_playerColliderTuning.fallbackOffset.z;

        buildBoxCollider(colliderSize, colliderOffset);
    }

    player = std::make_unique<Entities::Player>(
        trans,
        m_playerModel,
        m_physicsMachine.get(),
        m_sweetsManager.get(),
        Collision::CollisionLayer::Player,
        true,
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
        const auto& fallbackPlatform = m_stageData.GetPlatform(0);
        float platformTopY = fallbackPlatform.center.y + (fallbackPlatform.scale.y * 0.5f);
        if (m_platePlatform)
        {
            const auto& platformXform = m_platePlatform->GetTransform();
            platformTopY = platformXform.position.y + (platformXform.scale.y * 0.5f);
        }
        player->SetMinimumY(platformTopY);
        player->SetBaseMoveSpeed(m_playerBaseHorizontalSpeed, m_playerBaseVerticalSpeed);
        player->SetSpeedMultiplier(m_playerSpeedMultiplier);
        player->SetPickupArea(m_playerPickupBoxWidth, m_playerPickupBoxHeight, m_playerPickupForwardOffset);
        RegisterPhysicsBody(*player);
        {
            std::ostringstream oss;
            const bool isKinematic = player->IsKinematic();
            oss << kLogPrefix << "Player physics body registered (kinematic="
                << std::boolalpha << isKinematic
                << ", bodyType=" << (player->GetPhysical().IsDynamic() ? "dynamic" : "static") << ").\n";
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
    if (m_hudRenderer)
    {
        m_hudRenderer->Shutdown();
        m_hudRenderer.reset();
    }

    for (std::size_t i = 0; i < m_foundationPlatforms.size(); ++i)
    {
        if (i < m_platformUpdateRegistered.size() && m_platformUpdateRegistered[i] && m_foundationPlatforms[i])
        {
            Unregister(*m_foundationPlatforms[i]);
            m_platformUpdateRegistered[i] = false;
        }

        if (i < m_platformDrawRegistered.size() && m_platformDrawRegistered[i] && m_drawSystem && m_foundationPlatforms[i])
        {
            m_drawSystem->UnregisterRenderable(*m_foundationPlatforms[i]);
            m_platformDrawRegistered[i] = false;
        }
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
            GetUpdateScheduler().Register(*m_physicsMachine);
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
                std::fill(m_platformDrawRegistered.begin(), m_platformDrawRegistered.end(), false);
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
}

void SceneGame::ConfigureUpdates()
{
        if (auto* input = GetInput())
        {
                Register(*input);
                m_inputRegistered = true;
        }

        RegisterSceneUpdateNode(Core::UpdatePhase::Gameplay, 0);

        if (m_physicsMachine && !m_physicsRegistered)
        {
                if (auto system = m_physicsMachine->GetBox2DSystem())
                {
                        m_physicsRegistered = true;
                }
        }

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

        for (std::size_t i = 0; i < m_foundationPlatforms.size(); ++i)
        {
                auto* platform = m_foundationPlatforms[i].get();
                if (!platform)
                {
                        continue;
                }

                if (i >= m_platformUpdateRegistered.size())
                {
                        m_platformUpdateRegistered.resize(m_foundationPlatforms.size(), false);
                }
                if (!m_platformUpdateRegistered[i])
                {
                        Register(*platform);
                        m_platformUpdateRegistered[i] = true;
                }

                if (m_drawSystem)
                {
                        if (i >= m_platformDrawRegistered.size())
                        {
                                m_platformDrawRegistered.resize(m_foundationPlatforms.size(), false);
                        }
                        if (!m_platformDrawRegistered[i])
                        {
                                m_drawSystem->RegisterRenderable(*platform);
                                m_platformDrawRegistered[i] = true;
                        }
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
        }

        SetStageState(StageState::Playing);
}

void SceneGame::Update(Core::UpdateContext& ctx)
{
#if defined(_DEBUG)
    HandleColliderDebugCommands(ctx);
#endif
    ValidatePlayerPhysicsRegistration();
    RefreshPlatformAnchor(false);
    UpdateCountdown(ctx);
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

        // World bounds on X
        const float minX = camPos.x - halfWidth;
        const float maxX = camPos.x + halfWidth;

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
        fRads.z += 0.001f * RAD;

        if (m_cameraAnchor && !m_pObjects.empty())
        {
            auto& anchorTransform = m_cameraAnchor->GetTransform();
            anchorTransform.position = m_pObjects.front()->GetPosition();
            anchorTransform.rotation = m_pObjects.front()->GetfRads();
            anchorTransform.scale = m_pObjects.front()->GetScale();
        }

        const float stackedHeight = m_maxTowerHeight;
        if (m_phaseManager.UpdateHeight(stackedHeight))
        {
            BroadcastPhaseAdvance(m_phaseManager.GetCurrentPhase());
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

        Collision::ICollider* collider = player->GetCollider();
        if (!collider)
        {
                DebugTrace(std::string(kLogPrefix) + "[ColliderDebug] Player collider missing; cannot log extents.\n");
                return;
        }

        const AABB bounds = collider->GetAABB();
        std::ostringstream oss;
        oss << kLogPrefix << "[ColliderDebug] Nudged player by (" << delta.x << ',' << delta.y << ',' << delta.z << ")"
            << " newPos=(" << transform.position.x << ',' << transform.position.y << ',' << transform.position.z << ")"
            << " aabbMin=(" << bounds.min.x << ',' << bounds.min.y << ',' << bounds.min.z << ")"
            << " aabbMax=(" << bounds.max.x << ',' << bounds.max.y << ',' << bounds.max.z << ")\n";
        ::DebugTrace(oss.str());
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

        if (m_hudRenderer)
        {
                HudDisplayData hudData{};
                  hudData.remainingSeconds = std::max(0.0f, m_remainingCountdownSeconds);
                  hudData.currentHeight = m_currentTowerHeight;
                  hudData.peakHeight = m_peakTowerHeight;
                  hudData.score = ComputeScore();
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
// BaseCamera::RecalculateViewは保護されたメソッドです。GetView はトランスフォームが変更された際に遅延評価で再計算
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

    RefreshPlatformAnchor(true);
}

void SceneGame::RefreshPlatformAnchor(bool forceRecompute)
{
    if (m_foundationPlatforms.empty() || !m_gameCamera)
    {
        return;
    }

    float viewportWidth = static_cast<float>(SCREEN_WIDTH);
    float viewportHeight = static_cast<float>(SCREEN_HEIGHT);
    if (RenderTarget* rtv = GetDefaultRTV())
    {
        viewportWidth = static_cast<float>(rtv->GetWidth());
        viewportHeight = static_cast<float>(rtv->GetHeight());
    }
    else if (forceRecompute && !m_platformAnchorInitialized)
    {
        DebugTrace(std::string(kLogPrefix) + "[PlatformAlign] DefaultRTV missing; falling back to constant screen size.\n");
    }

    const float3 cameraPos = m_gameCamera->GetPosition();
    const float3 cameraRot = m_gameCamera->GetRotation();
    const float cameraFov = m_gameCamera->GetFieldOfViewDegrees();
    const float cameraAspect = m_gameCamera->GetAspectRatio();
    const float3 rigOffset = m_cameraRig ? m_cameraRig->GetOffset() : float3{ 0.0f, 0.0f, 0.0f };

    const bool cameraChanged = forceRecompute
        || !m_platformAnchorInitialized
        || HasMeaningfulDifference(m_cachedViewportWidth, viewportWidth)
        || HasMeaningfulDifference(m_cachedViewportHeight, viewportHeight)
        || HasMeaningfulDifference(m_cachedCameraPosition, cameraPos)
        || HasMeaningfulDifference(m_cachedCameraRotation, cameraRot)
        || HasMeaningfulDifference(m_cachedCameraRigOffset, rigOffset)
        || HasMeaningfulDifference(m_cachedCameraFovDegrees, cameraFov)
        || HasMeaningfulDifference(m_cachedCameraAspect, cameraAspect);

    if (!cameraChanged)
    {
        return;
    }

    const auto& platformDescriptors = m_stageData.GetPlatforms();
    const std::size_t platformCount = std::min(m_foundationPlatforms.size(), platformDescriptors.size());
    if (platformCount == 0)
    {
        return;
    }

    for (std::size_t i = 0; i < platformCount; ++i)
    {
        auto& platform = m_foundationPlatforms[i];
        if (!platform)
        {
            continue;
        }

        const auto& descriptor = platformDescriptors[i];
        Transform updated = ComputePlatformTransformForCamera(
            *m_gameCamera,
            viewportWidth,
            viewportHeight,
            descriptor,
            kPlatformScreenBottomFraction,
            kStageCenterX,
            i);
        platform->SetAnchorTransform(updated);
    }

    m_platformAnchorInitialized = true;

    m_cachedViewportWidth = viewportWidth;
    m_cachedViewportHeight = viewportHeight;
    m_cachedCameraPosition = cameraPos;
    m_cachedCameraRotation = cameraRot;
    m_cachedCameraRigOffset = rigOffset;
    m_cachedCameraFovDegrees = cameraFov;
    m_cachedCameraAspect = cameraAspect;
}

void SceneGame::RegisterPhysicsBody(Entities::PhysicalEntity& entity)
{
    if (!m_physicsMachine)
        return;

    const auto& boxColliderComponent = entity.GetBoxColliderComponent();
    const bool hasBoxColliderComponent = boxColliderComponent && !boxColliderComponent->Empty();
    const auto& shapeConfig = entity.GetBox2DShapeConfig();
    const bool hasShapeConfig = shapeConfig && shapeConfig->type != Physics::Box2DShapeConfig::Type::None;
    const bool hasMeshData = entity.HasBox2DMesh() || !entity.GetBox2DBoundary().empty();
    if (!hasBoxColliderComponent && !hasShapeConfig && !hasMeshData)
    {
        DebugTrace("[SceneGame] Skipped physics registration: entity has no Box2D collider component, shape config, or mesh.\n");
        return;
    }

    const bool meshOnly = hasMeshData && !hasBoxColliderComponent && !hasShapeConfig;

    const auto insertResult = m_registeredPhysicsBodies.insert(&entity);
    if (!insertResult.second)
        return;

    if (meshOnly)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Registering mesh-only physics body tag='" << entity.GetTag()
            << "' polygons=" << entity.GetBox2DPolygons().size()
            << " boundaryPoints=" << entity.GetBox2DBoundary().size() << "\\n";
        DebugTrace(oss.str());
    }

    entity.AttachPhysicsMachine(m_physicsMachine.get());

    {
        const bool hasBody = entity.HasBox2DBody();
        std::ostringstream oss;
        oss << kLogPrefix << "Physics registration complete for tag='" << entity.GetTag()
            << "' hasBody=" << std::boolalpha << hasBody << (hasBody ? "" : " (unexpected)") << "\n";
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

    const auto tracked = m_registeredPhysicsBodies.find(entity);
    if (tracked == m_registeredPhysicsBodies.end())
    {
        std::ostringstream oss;
        oss << kLogPrefix << "UnregisterPhysicsBody called for untracked entity=" << entity << "\n";
        ::DebugTrace(oss.str());
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
        DebugTrace(std::string(kLogPrefix) + "Physics machine missing during UnregisterPhysicsBody.\n");
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

void SceneGame::InitializeStageAnchors()
{
    m_pObjects.clear();
    const auto& spawnPoints = m_stageData.GetSpawnPoints();
    if (spawnPoints.empty())
    {
        const float3 fallback{ 0.0f, 0.0f, m_stageData.GetPlaneZ() };
        m_pObjects.push_back(std::make_shared<TestObject>("OiiaioooooiaiFin", fallback, float3{ 1.0f, 1.0f, 1.0f }, float3{ 0.0f, 0.0f, 0.0f }));
        std::ostringstream oss;
        oss << kLogPrefix << "InitializeStageAnchors fallback anchor at z=" << fallback.z << "\n";
        ::DebugTrace(oss.str());
        return;
    }

    for (const auto& spawn : spawnPoints)
    {
        m_pObjects.push_back(std::make_shared<TestObject>("OiiaioooooiaiFin", spawn, float3{ 1.0f, 1.0f, 1.0f }, float3{ 0.0f, 0.0f, 0.0f }));
    }

    std::ostringstream oss;
    oss << kLogPrefix << "InitializeStageAnchors loaded " << m_pObjects.size() << " stage anchors.\n";
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

        player->SetPickupArea(m_playerPickupBoxWidth, m_playerPickupBoxHeight, m_playerPickupForwardOffset);
    }

    InitializeStageAnchors();
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
    m_heightBonusArmed = (m_heightBonusThresholdMeters > 0.0f && m_heightBonusSeconds > 0.0f);
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
    float instantHeight = 0.0f;
    if (m_sweetsManager)
    {
        instantHeight = m_sweetsManager->GetHighestStackedHeight();
    }

    m_currentTowerHeight = instantHeight;
    m_maxTowerHeight = std::max(m_maxTowerHeight, instantHeight);

    if (instantHeight > m_peakTowerHeight)
    {
        m_peakTowerHeight = instantHeight;
        std::ostringstream oss;
        oss << kLogPrefix << "[Tower] newPeakTop=" << std::fixed << std::setprecision(2)
            << m_peakTowerHeight << "m remaining=" << std::fixed << std::setprecision(2)
            << m_remainingCountdownSeconds << "s\n";
        ::DebugTrace(oss.str());
    }

    if (m_heightBonusThresholdMeters > 0.0f && m_heightBonusSeconds > 0.0f)
    {
        if (instantHeight >= m_heightBonusThresholdMeters)
        {
            if (m_heightBonusArmed)
            {
                m_heightBonusArmed = false;
                m_pendingTimeBonuses.push_back(PendingTimeBonus{ m_heightBonusSeconds, instantHeight });
                std::ostringstream oss;
                oss << kLogPrefix << "[TowerBonus] thresholdCrossed height=" << std::fixed << std::setprecision(2)
                    << instantHeight << "m threshold=" << std::fixed << std::setprecision(2)
                    << m_heightBonusThresholdMeters << "m bonus=" << std::fixed << std::setprecision(2)
                    << m_heightBonusSeconds << "s queue=" << m_pendingTimeBonuses.size() << "\n";
                ::DebugTrace(oss.str());
            }
        }
        else if (!m_heightBonusArmed)
        {
            m_heightBonusArmed = true;
            std::ostringstream oss;
            oss << kLogPrefix << "[TowerBonus] rearmed height=" << std::fixed << std::setprecision(2)
                << instantHeight << "m threshold=" << std::fixed << std::setprecision(2)
                << m_heightBonusThresholdMeters << "m\n";
            ::DebugTrace(oss.str());
        }
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

    DirectX::XMFLOAT4X4 viewMatrix = camera.GetView();
    DirectX::XMFLOAT4X4 projMatrix = camera.GetProjection();
    const DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&viewMatrix);
    const DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&projMatrix);
    const DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);
    const DirectX::XMVECTOR worldPos = DirectX::XMVectorSet(0.0f, m_guidelineHeightMeters, m_stageData.GetPlaneZ(), 1.0f);
    const DirectX::XMVECTOR clipPos = DirectX::XMVector4Transform(worldPos, viewProj);
    const float clipW = DirectX::XMVectorGetW(clipPos);
    if (!std::isfinite(clipW) || std::fabs(clipW) <= std::numeric_limits<float>::epsilon())
    {
        if (!m_guidelineInvalidLogged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Guideline] Invalid clip W=" << clipW
                << " threshold=" << m_guidelineHeightMeters
                << " planeZ=" << m_stageData.GetPlaneZ() << "\n";
            ::DebugTrace(oss.str());
            m_guidelineInvalidLogged = true;
        }
        return false;
    }

    const float clipY = DirectX::XMVectorGetY(clipPos) / clipW;
    if (!std::isfinite(clipY))
    {
        if (!m_guidelineInvalidLogged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Guideline] Non-finite clipY derived from threshold="
                << m_guidelineHeightMeters << " clipW=" << clipW << "\n";
            ::DebugTrace(oss.str());
            m_guidelineInvalidLogged = true;
        }
        return false;
    }

    outClipY = std::clamp(clipY, -1.0f, 1.0f);
    if ((clipY < -1.0f || clipY > 1.0f))
    {
        if (!m_guidelineClampLogged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Guideline] Clamped clipY from " << clipY
                << " to " << outClipY << " threshold=" << m_guidelineHeightMeters
                << " planeZ=" << m_stageData.GetPlaneZ() << "\n";
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
