#include "SweetsManager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <unordered_set>
#include <utility>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif
#include <d3d11.h>
#include <vector>
#include "AABBCollider.h"
#include "ColliderConfigLoader.h"
#include "Collision/ConvexHullBuilder.h"
#include "Core/UpdateScheduler.h"
#include "MeshBuffer.h"
#include "ModelCache.h"
#include "Sweets/BaseSweets.h"
#include "SweetsPrototypeManifest.h"
#include "Systems/DrawSystem.h"
#include "Systems/PhysicsMachine.h"
#include <DirectXMath.h>

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

    constexpr const char* kLogPrefix = "[SweetsManager] ";
    constexpr float kRestVelocityThresholdSq = 0.05f * 0.05f;
    constexpr float kGroundHeightThreshold = 0.05f;
    constexpr float kDefaultSpawnIntervalSeconds = 2.5f;
    constexpr float kDefaultBurstIntervalSeconds = 0.5f;
    constexpr int kDefaultSimultaneousSpawnCount = 1;
    constexpr int kDefaultSimultaneousLiveTarget = kDefaultSimultaneousSpawnCount;
    constexpr int kDefaultLiveSweetsCap = 24;
    constexpr float kMinimumBurstIntervalSeconds = 0.05f;
    constexpr float kMinimumSpawnIntervalSeconds = 0.1f;
    constexpr std::size_t kMaxSpawnQueueEntries = 16;
    constexpr float kDefaultSpawnHeightOffset = 8.0f;
    constexpr const char* kColliderConfigPath = "Assets/Config/colliders_sweets.cfg";
    constexpr const char* kFactoryPresetCsvPath = "Assets/Config/sweets_factory.csv";
    constexpr float kPlatformWidth = 12.0f;
    constexpr float kHorizontalOutOfBoundsMargin = 3.5f;
    constexpr float kHorizontalBoundsEpsilon = 1e-4f;
    constexpr float kDefaultGeneratedThicknessPadding = 1.1f;

    struct FallbackMaterialColors
    {
        DirectX::XMFLOAT4 diffuse{};
        DirectX::XMFLOAT4 ambient{};
    };

    FallbackMaterialColors GetFallbackColorsFor(SweetsSpace::SweetsType type)
    {
        // Ambient alpha is kept at 1.0 to disable texture sampling for texture-less meshes.
        switch (type)
        {
        case SweetsSpace::SweetsType::Pudding:
            return { DirectX::XMFLOAT4{ 0.89f, 0.72f, 0.38f, 1.0f }, DirectX::XMFLOAT4{ 0.65f, 0.52f, 0.28f, 1.0f } };
        case SweetsSpace::SweetsType::Sherbet:
            return { DirectX::XMFLOAT4{ 0.96f, 0.78f, 0.86f, 1.0f }, DirectX::XMFLOAT4{ 0.78f, 0.62f, 0.69f, 1.0f } };
        case SweetsSpace::SweetsType::Cake:
        default:
            return { DirectX::XMFLOAT4{ 1.0f, 0.94f, 0.85f, 1.0f }, DirectX::XMFLOAT4{ 0.78f, 0.72f, 0.62f, 1.0f } };
        }
    }

    void ApplyFallbackMaterials(ModelDX& model, SweetsSpace::SweetsType type)
    {
        const FallbackMaterialColors colors = GetFallbackColorsFor(type);
        std::unordered_set<unsigned int> visitedMaterials;

        for (int i = 0; i < static_cast<int>(model.GetMeshCount()); ++i)
        {
            auto mesh = model.GetMesh(i).lock();
            if (!mesh)
            {
                continue;
            }

            const unsigned int materialId = mesh->GetMaterialID();
            if (!visitedMaterials.insert(materialId).second)
            {
                continue;
            }

            auto material = model.GetMaterial(static_cast<int>(materialId)).lock();
            if (!material)
            {
                continue;
            }

            if (material->GetTexture())
            {
                continue;
            }

            material->SetDiffuse(colors.diffuse);
            material->SetAmbient(colors.ambient);

#if defined(_DEBUG)
            std::ostringstream oss;
            oss << kLogPrefix << "Applied fallback material for materialId=" << materialId
                << " diffuse=(" << colors.diffuse.x << ',' << colors.diffuse.y << ',' << colors.diffuse.z
                << ") ambient=(" << colors.ambient.x << ',' << colors.ambient.y << ',' << colors.ambient.z
                << ") sweetsType=" << SweetsSpace::ToString(type) << "\n";
            DebugTrace(oss.str());
#endif
        }
    }

    using SweetsSpace::ColliderPreset;
    using SweetsSpace::ColliderShape;
    using SweetsSpace::SweetsPrototypeManifest;
    using SweetsSpace::SweetsType;

    struct ModelBounds
    {
        float3 min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
        float3 max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
        bool valid{ false };
    };

    struct MeshHullSummary
    {
        ModelBounds bounds{};
        std::vector<float2> hull{};
        std::vector<std::vector<float2>> polygons{};
        float rawThickness{ 0.0f };
        float thickness{ 1.0f };
        float paddingFactor{ 1.0f };
    };

    float SanitizeThicknessPadding(float value)
    {
        if (!std::isfinite(value))
        {
            return 1.0f;
        }

        return std::max(value, 1.0f);
    }

    bool IsColliderDefinitionEmpty(const Collision::ColliderDefinition& definition)
    {
        switch (definition.type)
        {
        case Collision::ColliderKind::Compound:
            return definition.children.empty();
        case Collision::ColliderKind::Hull:
            return definition.points.empty();
        case Collision::ColliderKind::Circle:
            return definition.radius <= std::numeric_limits<float>::epsilon();
        default:
            return false;
        }
    }

    void UpdateBatchUsage(SweetsSpace::DeleteBatchContext* batch, SweetsSpace::UseOutcome outcome)
    {
        if (!batch)
        {
            return;
        }

        if (batch->usesRemaining > 0)
        {
            --batch->usesRemaining;
        }

        if (outcome == SweetsSpace::UseOutcome::Hit)
        {
            ++batch->usesRemaining;
        }
    }

    float ComputeSpeedSq(const Physics::Physical& physical)
    {
        const auto& v = physical.velocity;
        return (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
    }

    bool HasMeaningfulDifference(float previous, float current, float epsilon = kHorizontalBoundsEpsilon)
    {
        if (!std::isfinite(previous) || !std::isfinite(current))
        {
            return true;
        }

        return std::fabs(previous - current) > epsilon;
    }

    float ComputeFallbackHorizontalLimit(float margin)
    {
        const float sanitizedMargin = std::max(0.0f, margin);
        return (kPlatformWidth * 0.5f) + sanitizedMargin;
    }

    SweetsSpace::SweetsState EvaluateState(SweetsSpace::ActiveSweets& sweets, SweetsSpace::SweetsState currentState)
    {
        if (currentState == SweetsSpace::SweetsState::Carried)
        {
            return SweetsSpace::SweetsState::Carried;
        }

        const float speedSq = ComputeSpeedSq(sweets.GetPhysical());
        const float height = sweets.GetTransform().position.y;

        if (height <= kGroundHeightThreshold && speedSq <= kRestVelocityThresholdSq)
        {
            return SweetsSpace::SweetsState::Stacked;
        }

        if (speedSq <= kRestVelocityThresholdSq)
        {
            return SweetsSpace::SweetsState::Stacked;
        }

        return SweetsSpace::SweetsState::Active;
    }

    bool IsHorizontallyOutOfBounds(
        const SweetsSpace::ActiveSweets& sweets,
        const std::optional<std::pair<float, float>>& worldBounds,
        float margin,
        float fallbackLimit,
        bool* usedFallback)
    {
        float minX = 0.0f;
        float maxX = 0.0f;
        const Collision::ICollider* collider = sweets.GetCollider();
        if (collider)
        {
            const AABB bounds = collider->GetAABB();
            minX = bounds.min.x;
            maxX = bounds.max.x;
        }
        else
        {
            const Transform& transform = sweets.GetTransform();
            const float halfWidth = std::max(0.0f, transform.scale.x * 0.5f);
            minX = transform.position.x - halfWidth;
            maxX = transform.position.x + halfWidth;
        }

        const float sanitizedMargin = std::max(0.0f, margin);
        float allowedMin = -fallbackLimit;
        float allowedMax = fallbackLimit;
        if (usedFallback)
        {
            *usedFallback = !worldBounds.has_value();
        }

        if (worldBounds)
        {
            allowedMin = worldBounds->first - sanitizedMargin;
            allowedMax = worldBounds->second + sanitizedMargin;
        }

        return minX < allowedMin || maxX > allowedMax;
    }

    std::string TrimToken(const std::string& value)
    {
        auto begin = value.begin();
        auto end = value.end();
        while (begin != end && std::isspace(static_cast<unsigned char>(*begin)))
        {
            ++begin;
        }

        while (end != begin)
        {
            auto prev = end;
            --prev;
            if (!std::isspace(static_cast<unsigned char>(*prev)))
            {
                break;
            }
            end = prev;
        }

        return std::string(begin, end);
    }

    std::vector<SweetsSpace::WeightedSweetsType> ParseWeights(const std::string& raw, bool* outError)
    {
        std::vector<SweetsSpace::WeightedSweetsType> weights;
        auto& manifest = SweetsPrototypeManifest::Instance();
        std::string token;
        std::istringstream stream(raw);

        while (std::getline(stream, token, ','))
        {
            token = TrimToken(token);
            if (token.empty())
            {
                continue;
            }

            float weight = 1.0f;
            std::string kind = token;
            const auto colon = token.find(':');
            if (colon != std::string::npos)
            {
                kind = TrimToken(token.substr(0, colon));
                std::string weightToken = TrimToken(token.substr(colon + 1));
                if (!weightToken.empty())
                {
                    try
                    {
                        weight = std::stof(weightToken);
                    }
                    catch (const std::exception& ex)
                    {
                        if (outError)
                        {
                            *outError = true;
                        }

                        std::ostringstream oss;
                        oss << kLogPrefix << "Failed to parse weight token '" << weightToken
                            << "' from '" << raw << "': " << ex.what() << "\n";
                        DebugTrace(oss.str());
                        return {};
                    }
                }
            }

            if (!(weight > 0.0f))
            {
                continue;
            }

            const auto resolvedType = manifest.Resolve(kind);
            if (!resolvedType)
            {
                if (outError)
                {
                    *outError = true;
                }

                std::ostringstream oss;
                oss << kLogPrefix << "Unknown sweets key '" << kind
                    << "' in weight definition '" << raw << "'.\n";
                DebugTrace(oss.str());
                continue;
            }

            weights.push_back({ *resolvedType, weight });
        }

        if (weights.empty() && outError)
        {
            *outError = true;
        }

        return weights;
    }

    std::string NormalizeKey(const std::string& key)
    {
        std::string normalized;
        normalized.reserve(key.size());
        for (unsigned char ch : key)
        {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        }
        return normalized;
    }

    bool TryParseFloat(const std::string& token, float* out)
    {
        try
        {
            *out = std::stof(token);
            return std::isfinite(*out);
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    using FactoryPreset = SweetsSpace::SweetsManager::FactoryPreset;

    std::optional<FactoryPreset> BuildDefaultFactoryPreset(SweetsSpace::SweetsType type)
    {
        FactoryPreset preset{};
        preset.spawnWeight = 0.0f;
        preset.weight = 1.0f;
        preset.damping = 0.98f;
        preset.minScale = float3{ 1.0f, 1.0f, 1.0f };
        preset.maxScale = float3{ 1.0f, 1.0f, 1.0f };
        preset.colliderPreset = ColliderPreset{};

        switch (type)
        {
        case SweetsSpace::SweetsType::Cake:
            preset.spawnWeight = 1.0f;
            preset.weight = 1.35f;
            preset.damping = 0.96f;
            preset.minScale = float3{ 0.85f, 0.85f, 0.85f };
            preset.maxScale = float3{ 1.35f, 1.35f, 1.35f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Box, float3{ 0.02f, 0.02f, 0.02f } };
            return preset;
        case SweetsSpace::SweetsType::Pudding:
            preset.spawnWeight = 1.0f;
            preset.weight = 1.0f;
            preset.damping = 0.98f;
            preset.minScale = float3{ 0.9f, 0.9f, 0.9f };
            preset.maxScale = float3{ 1.2f, 1.2f, 1.2f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Box, float3{ 0.01f, 0.04f, 0.01f } };
            return preset;
        case SweetsSpace::SweetsType::Sherbet:
            preset.spawnWeight = 1.0f;
            preset.weight = 0.85f;
            preset.damping = 0.99f;
            preset.minScale = float3{ 0.75f, 0.75f, 0.75f };
            preset.maxScale = float3{ 1.4f, 1.4f, 1.4f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Capsule, float3{ 0.01f, 0.01f, 0.01f } };
            return preset;
        default:
            return std::nullopt;
        }
    }

    bool ParseFactoryRow(const std::vector<std::string>& tokens, FactoryPreset& outPreset, size_t lineNumber)
    {
        if (tokens.size() < 13)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Factory preset row on line " << lineNumber
                << " had " << tokens.size() << " tokens; expected 13.\n";
            DebugTrace(oss.str());
            return false;
        }

        bool parseError = false;
        parseError |= !TryParseFloat(tokens[1], &outPreset.spawnWeight);
        parseError |= !TryParseFloat(tokens[2], &outPreset.weight);
        parseError |= !TryParseFloat(tokens[3], &outPreset.damping);
        parseError |= !TryParseFloat(tokens[4], &outPreset.minScale.x);
        parseError |= !TryParseFloat(tokens[5], &outPreset.minScale.y);
        parseError |= !TryParseFloat(tokens[6], &outPreset.minScale.z);
        parseError |= !TryParseFloat(tokens[7], &outPreset.maxScale.x);
        parseError |= !TryParseFloat(tokens[8], &outPreset.maxScale.y);
        parseError |= !TryParseFloat(tokens[9], &outPreset.maxScale.z);
        parseError |= !TryParseFloat(tokens[11], &outPreset.colliderPreset.padding.x);
        parseError |= !TryParseFloat(tokens[12], &outPreset.colliderPreset.padding.y);

        if (tokens.size() > 13)
        {
            parseError |= !TryParseFloat(tokens[13], &outPreset.colliderPreset.padding.z);
        }
        else
        {
            outPreset.colliderPreset.padding.z = 0.0f;
        }

        const std::string colliderShape = NormalizeKey(tokens[10]);
        if (colliderShape == "capsule")
        {
            outPreset.colliderPreset.shape = ColliderShape::Capsule;
        }
        else if (colliderShape == "box" || colliderShape.empty())
        {
            outPreset.colliderPreset.shape = ColliderShape::Box;
        }
        else
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Unknown collider preset '" << tokens[10]
                << "' on line " << lineNumber << "; defaulting to Box.\n";
            DebugTrace(oss.str());
            outPreset.colliderPreset.shape = ColliderShape::Box;
        }

        outPreset.spawnWeight = std::max(0.0f, outPreset.spawnWeight);
        outPreset.weight = std::max(0.0f, outPreset.weight);
        outPreset.damping = std::clamp(outPreset.damping, 0.0f, 0.999f);
        return !parseError;
    }

    std::map<std::string, FactoryPreset> LoadFactoryPresets(bool* parseError)
    {
        std::map<std::string, FactoryPreset> overrides;
        std::ifstream file(kFactoryPresetCsvPath);
        if (!file.is_open())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Factory preset CSV missing at '" << kFactoryPresetCsvPath
                << "'; falling back to legacy defaults.\n";
            DebugTrace(oss.str());
            return overrides;
        }

        std::string line;
        size_t lineNumber = 0;
        while (std::getline(file, line))
        {
            ++lineNumber;
            std::string trimmed = TrimToken(line);
            if (trimmed.empty() || trimmed[0] == '#')
            {
                continue;
            }

            std::vector<std::string> tokens;
            std::stringstream ss(trimmed);
            std::string column;
            while (std::getline(ss, column, ','))
            {
                tokens.push_back(TrimToken(column));
            }

            if (tokens.empty())
            {
                continue;
            }

            const std::string normalizedHeader = NormalizeKey(tokens[0]);
            if (normalizedHeader == "key")
            {
                continue;
            }

            FactoryPreset preset{};
            if (!ParseFactoryRow(tokens, preset, lineNumber))
            {
                if (parseError)
                {
                    *parseError = true;
                }
                continue;
            }

            const std::string normalizedKey = NormalizeKey(tokens[0]);
            overrides[normalizedKey] = preset;

            std::ostringstream oss;
            oss << kLogPrefix << "Loaded factory preset for key '" << tokens[0]
                << "' spawnWeight=" << preset.spawnWeight
                << " weight=" << preset.weight
                << " damping=" << preset.damping
                << " minScale=(" << preset.minScale.x << ',' << preset.minScale.y << ',' << preset.minScale.z << ')'
                << " maxScale=(" << preset.maxScale.x << ',' << preset.maxScale.y << ',' << preset.maxScale.z << ')'
                << " collider=" << ToString(preset.colliderPreset.shape)
                << " padding=(" << preset.colliderPreset.padding.x << ',' << preset.colliderPreset.padding.y << ','
                << preset.colliderPreset.padding.z << ")\n";
            DebugTrace(oss.str());
        }

        return overrides;
    }

    const char* ToString(ColliderShape shape)
    {
        switch (shape)
        {
        case ColliderShape::Box:
            return "Box";
        case ColliderShape::Capsule:
            return "Capsule";
        default:
            return "Unknown";
        }
    }

    float3 ScaleVector(const float3& value, const float3& scale)
    {
        return float3{ value.x * scale.x, value.y * scale.y, value.z * scale.z };
    }

    MeshHullSummary AnalyzeModelGeometry(const std::string& path, float thicknessPadding)
    {
        MeshHullSummary summary{};
        std::ifstream file(path);
        if (!file)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Failed to open model for geometry analysis path='" << path << "'\n";
            DebugTrace(oss.str());
            return summary;
        }

        struct PositionOnly
        {
            float x;
            float y;
            float z;
        };

        std::vector<PositionOnly> positions;
        std::vector<UINT> indices;
        std::string line;
        const float sanitizedPadding = SanitizeThicknessPadding(thicknessPadding);
        summary.paddingFactor = sanitizedPadding;

        while (std::getline(file, line))
        {
            if (line.size() < 2 || line[0] != 'v' || line[1] != ' ')
            {
                if (line.size() < 2 || line[0] != 'f' || line[1] != ' ')
                {
                    continue;
                }
            }

            if (line[0] == 'v')
            {
                std::istringstream parser(line.substr(2));
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                if (!(parser >> x >> y >> z))
                {
                    continue;
                }

                positions.push_back(PositionOnly{ x, y, z });

                summary.bounds.min.x = std::min(summary.bounds.min.x, x);
                summary.bounds.min.y = std::min(summary.bounds.min.y, y);
                summary.bounds.min.z = std::min(summary.bounds.min.z, z);

                summary.bounds.max.x = std::max(summary.bounds.max.x, x);
                summary.bounds.max.y = std::max(summary.bounds.max.y, y);
                summary.bounds.max.z = std::max(summary.bounds.max.z, z);
                summary.bounds.valid = true;
            }
            else if (line[0] == 'f')
            {
                std::istringstream parser(line.substr(2));
                std::string token;
                std::vector<UINT> faceIndices;
                while (parser >> token)
                {
                    const auto slash = token.find('/');
                    const std::string indexToken = token.substr(0, slash);
                    try
                    {
                        int index = std::stoi(indexToken);
                        if (index < 0)
                        {
                            index = static_cast<int>(positions.size()) + index + 1;
                        }
                        const UINT zeroBased = static_cast<UINT>(std::max(index - 1, 0));
                        faceIndices.push_back(zeroBased);
                    }
                    catch (...)
                    {
                        continue;
                    }
                }

                if (faceIndices.size() >= 3)
                {
                    for (size_t i = 1; i + 1 < faceIndices.size(); ++i)
                    {
                        indices.push_back(faceIndices[0]);
                        indices.push_back(faceIndices[i]);
                        indices.push_back(faceIndices[i + 1]);
                    }
                }
            }
        }

        if (!summary.bounds.valid)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "No vertex positions parsed from model path='" << path << "'; using fallback bounds.\n";
            DebugTrace(oss.str());
        }
        else
        {
            summary.rawThickness = std::max(summary.bounds.max.z - summary.bounds.min.z, 0.0f);
            summary.paddingFactor = sanitizedPadding;
            summary.thickness = summary.rawThickness * summary.paddingFactor;
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

            const Collision::Mesh2DExtraction meshExtraction = Collision::ExtractProjectedPolygons(desc);
            summary.hull = Collision::BuildConvexHullFromMesh(desc);
            summary.polygons = meshExtraction.polygons;
            std::ostringstream oss;
            oss << kLogPrefix << "Analyzed geometry for '" << path << "' positions=" << positions.size()
                << " hullPoints=" << summary.hull.size() << " polygonParts=" << summary.polygons.size()
                << " rawThickness=" << summary.rawThickness
                << " paddedThickness=" << summary.thickness
                << " paddingFactor=" << summary.paddingFactor << "\n";
            DebugTrace(oss.str());
        }
        else
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Geometry analysis found no positions for '" << path << "'.\n";
            DebugTrace(oss.str());
        }

        return summary;
    }

    float3 ComputeHalfExtents(const ModelBounds& bounds, const float3& scale, const ColliderPreset& preset)
    {
        float3 scaledMin = ScaleVector(bounds.min, scale);
        float3 scaledMax = ScaleVector(bounds.max, scale);
        float3 halfExtents{};

        switch (preset.shape)
        {
        case ColliderShape::Capsule:
        {
            const float radiusX = (scaledMax.x - scaledMin.x) * 0.5f;
            const float radiusZ = (scaledMax.z - scaledMin.z) * 0.5f;
            const float radius = std::max(radiusX, radiusZ);
            const float height = scaledMax.y - scaledMin.y;
            halfExtents = float3{ radius, height * 0.5f, radius };
            break;
        }
        case ColliderShape::Box:
        default:
            halfExtents = float3{
                (scaledMax.x - scaledMin.x) * 0.5f,
                (scaledMax.y - scaledMin.y) * 0.5f,
                (scaledMax.z - scaledMin.z) * 0.5f,
            };
            break;
        }

        halfExtents.x += preset.padding.x;
        halfExtents.y += preset.padding.y;
        halfExtents.z += preset.padding.z;
        return halfExtents;
    }

    float3 ComputeColliderOffset(const ModelBounds& bounds, const float3& scale)
    {
        const float3 scaledMin = ScaleVector(bounds.min, scale);
        const float3 scaledMax = ScaleVector(bounds.max, scale);
        return float3{
            (scaledMax.x + scaledMin.x) * 0.5f,
            (scaledMax.y + scaledMin.y) * 0.5f,
            (scaledMax.z + scaledMin.z) * 0.5f,
        };
    }

    float3 ComputeExpandFromHalfExtents(const float3& halfExtents, const float3& transformScale)
    {
        float3 expand{ 1.0f, 1.0f, 1.0f };

        if (transformScale.x != 0.0f)
        {
            expand.x = (halfExtents.x * 2.0f) / transformScale.x;
        }
        if (transformScale.y != 0.0f)
        {
            expand.y = (halfExtents.y * 2.0f) / transformScale.y;
        }
        if (transformScale.z != 0.0f)
        {
            expand.z = (halfExtents.z * 2.0f) / transformScale.z;
        }

        return expand;
    }

    std::shared_ptr<Collision::CompoundCollider2D> BuildCompoundFromPolygons(
        const std::vector<std::vector<float2>>& polygons,
        float thickness,
        Collision::CollisionLayer layer)
    {
        std::vector<std::shared_ptr<Collision::ICollider>> parts;
        parts.reserve(polygons.size());

        for (const auto& poly : polygons)
        {
            if (poly.size() < 3)
            {
                continue;
            }

            parts.push_back(std::make_shared<Collision::ConvexHullCollider2D>(poly, float2{ 0.0f, 0.0f }, thickness, layer));
        }

        if (parts.empty())
        {
            return nullptr;
        }

        return std::make_shared<Collision::CompoundCollider2D>(parts, thickness, layer);
    }
}

namespace SweetsSpace
{
    const char* ToString(DeleteReason reason)
    {
        switch (reason)
        {
        case DeleteReason::Hit:
            return "Hit";
        case DeleteReason::Miss:
            return "Miss";
        default:
            return "<unknown>";
        }
    }

    const char* ToString(UseOutcome outcome)
    {
        switch (outcome)
        {
        case UseOutcome::Hit:
            return "Hit";
        case UseOutcome::Miss:
            return "Miss";
        default:
            return "<unknown>";
        }
    }

    const char* StateToString(SweetsState state)
    {
        switch (state)
        {
        case SweetsState::Active:
            return "Active";
        case SweetsState::Stacked:
            return "Stacked";
        case SweetsState::Ground:
            return "Ground";
        case SweetsState::Carried:
            return "Carried";
        default:
            return "Unknown";
        }
    }

    class LateUpdateNode : public Core::UpdateNode
    {
    public:
        explicit LateUpdateNode(SweetsSpace::SweetsManager& owner)
            : Core::UpdateNode(Core::UpdatePhase::Late, 0)
            , m_owner(owner)
        {
        }

        void OnUpdate(Core::UpdateContext& ctx) override
        {
            m_owner.OnLateUpdate(ctx);
        }

    private:
        SweetsSpace::SweetsManager& m_owner;
    };

    SweetsManager::SweetsManager()
        : m_drawSystem(nullptr)
        , m_physicsMachine(nullptr)
        , m_scheduler(nullptr)
        , m_lateUpdateNode(std::make_unique<LateUpdateNode>(*this))
        , m_phaseChangeCallback()
        , m_currentPhase(0)
        , m_phaseConfigs()
        , m_activePhaseConfigIndex(-1)
        , m_spawnTimer(kDefaultSpawnIntervalSeconds)
        , m_burstTimer(0.0f)
        , m_spawnQueue()
        , m_spawnOrigins()
        , m_spawnHeightOffset(kDefaultSpawnHeightOffset)
        , m_activeBurstInterval(kDefaultBurstIntervalSeconds)
        , m_activeBurstPlanned(0)
        , m_currentBurstRemaining(0)
        , m_simultaneousLiveTarget(kDefaultSimultaneousLiveTarget)
        , m_stackedInBatch(0)
        , m_batchInitialSpawnDone(false)
        , m_batchStackTarget(kDefaultSimultaneousSpawnCount)
        , m_batchCoolingDown(false)
        , m_random(std::random_device{}())
        , m_pendingTopUpAfterDrop(false)
        , m_enableRandomRotation(false)
        , m_randomRotationMinDegrees{ 0.0f, 0.0f, 0.0f }
        , m_randomRotationMaxDegrees{ 0.0f, 360.0f, 0.0f }
        , m_enableRandomScale(true)
        , m_liveSweetsCap(kDefaultLiveSweetsCap)
        , m_warnedMissingDrawSystem(false)
        , m_warnedMissingPhysicsMachine(false)
        , m_warnedMissingScheduler(false)
        , m_warnedMissingPhaseCallback(false)
        , m_warnedMissingSpawnOrigins(false)
        , m_warnedMissingHeightCollider(false)
        , m_warnedMissingBoundsCollider(false)
        , m_warnedMissingColliderDefinitionKinds()
        , m_warnedMissingGeneratedHull(false)
        , m_hasHorizontalBounds(false)
        , m_minHorizontalBound(0.0f)
        , m_maxHorizontalBound(0.0f)
        , m_horizontalBoundsMargin(kHorizontalOutOfBoundsMargin)
        , m_generatedColliderThicknessPadding(SanitizeThicknessPadding(kDefaultGeneratedThicknessPadding))
        , m_forceGeneratedColliders(false)
        , m_colliderConfigOverridesEnabled(false)
        , m_warnedMissingHorizontalBounds(false)
        , m_spawnPausedDueToLiveSweets(false)
        , m_lastLiveSweetsCount(0)
    {
        m_byState.emplace(SweetsState::Active, SweetsList{});
        m_byState.emplace(SweetsState::Stacked, SweetsList{});
        m_byState.emplace(SweetsState::Ground, SweetsList{});
        m_byState.emplace(SweetsState::Carried, SweetsList{});

        if (!Collision::LoadColliderDefinitions(kColliderConfigPath, m_colliderDefinitions))
        {
            DebugTrace(std::string(kLogPrefix) + "Failed to load collider definitions; using AABB fallbacks.\n");
        }

        Render::ModelCache& cache = Render::ModelCache::Instance();
        const auto& manifest = SweetsPrototypeManifest::Instance();

        bool factoryParseError = false;
        const auto factoryOverrides = LoadFactoryPresets(&factoryParseError);
        const ModelBounds fallbackBounds{ float3{ -0.5f, -0.5f, -0.5f }, float3{ 0.5f, 0.5f, 0.5f }, true };
        const auto prototypes = manifest.Enumerate();
        if (prototypes.empty())
        {
            DebugTrace(std::string(kLogPrefix) + "No sweets prototypes registered; spawner map will remain empty.\n");
        }

        for (const auto& prototype : prototypes)
        {
            const auto* descriptor = manifest.Find(prototype.type);
            if (!descriptor)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Prototype lookup failed for type '" << ToString(prototype.type)
                    << "'; skipping.\n";
                DebugTrace(oss.str());
                continue;
            }

            const std::string kind = descriptor->key;
            const std::string normalizedKind = NormalizeKey(kind);
            const std::string modelPath = descriptor->modelPath;

            const auto overrideIt = factoryOverrides.find(normalizedKind);
            const bool hasOverride = overrideIt != factoryOverrides.end();
            std::optional<FactoryPreset> defaultPreset = BuildDefaultFactoryPreset(prototype.type);

            if (!hasOverride && !defaultPreset)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "No factory preset available for kind '" << kind
                    << "' (type=" << ToString(prototype.type) << "); skipping registration.\n";
                DebugTrace(warn.str());
                continue;
            }

            FactoryPreset preset = hasOverride ? overrideIt->second : *defaultPreset;
            m_factoryPresets[kind] = preset;

            std::ostringstream presetLog;
            presetLog << kLogPrefix << "Resolved factory preset for kind '" << kind
                << "' source=" << (hasOverride ? "config" : "fallback")
                << " spawnWeight=" << preset.spawnWeight
                << " weight=" << preset.weight
                << " damping=" << preset.damping
                << " minScale=(" << preset.minScale.x << ',' << preset.minScale.y << ',' << preset.minScale.z << ')'
                << " maxScale=(" << preset.maxScale.x << ',' << preset.maxScale.y << ',' << preset.maxScale.z << ')'
                << " collider=" << ToString(preset.colliderPreset.shape)
                << " padding=(" << preset.colliderPreset.padding.x << ',' << preset.colliderPreset.padding.y << ','
                << preset.colliderPreset.padding.z << ")"
                << " overrideParseError=" << std::boolalpha << factoryParseError << "\n";
            DebugTrace(presetLog.str());

            MeshHullSummary geometry = AnalyzeModelGeometry(modelPath, m_generatedColliderThicknessPadding);
            const float baseFallbackThickness = fallbackBounds.max.z - fallbackBounds.min.z;
            const float fallbackThickness = baseFallbackThickness * m_generatedColliderThicknessPadding;
            if (geometry.hull.empty() && geometry.bounds.valid)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "No hull generated for kind '" << kind
                    << "'; collider definition or fallback will be used.\n";
                DebugTrace(warn.str());
            }

            ModelBounds bounds = geometry.bounds.valid ? geometry.bounds : fallbackBounds;
            if (!geometry.bounds.valid)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "Fallback collider bounds applied for kind '" << kind
                    << "' because model bounds were unavailable.\n";
                DebugTrace(warn.str());
            }
            const float resolvedRawThickness = geometry.bounds.valid ? geometry.rawThickness : baseFallbackThickness;
            const float paddedThickness = std::max(resolvedRawThickness * m_generatedColliderThicknessPadding, fallbackThickness);
            GeneratedCollider generatedCollider{ geometry.hull, geometry.polygons, paddedThickness };
            m_generatedColliders[kind] = generatedCollider;

            const auto colliderDefinition = m_colliderDefinitions.find(kind);
            const bool colliderDefinitionFound = colliderDefinition != m_colliderDefinitions.end();
            const bool colliderDefinitionEmpty = colliderDefinitionFound
                && IsColliderDefinitionEmpty(colliderDefinition->second);
            if (!colliderDefinitionFound || colliderDefinitionEmpty)
            {
                std::ostringstream padLog;
                padLog << kLogPrefix << "Using padded thickness for kind '" << kind
                    << "' baseThickness=" << resolvedRawThickness
                    << " paddingFactor=" << m_generatedColliderThicknessPadding
                    << " paddedThickness=" << paddedThickness
                    << (colliderDefinitionFound ? " because collider config was empty.\n"
                        : " because collider config was missing.\n");
                DebugTrace(padLog.str());
            }

            if (!generatedCollider.hull.empty())
            {
                std::ostringstream hullLog;
                hullLog << kLogPrefix << "Cached generated hull for kind '" << kind << "' points="
                    << generatedCollider.hull.size() << " polyParts=" << generatedCollider.polygons.size()
                    << " thickness=" << generatedCollider.thickness << "\n";
                DebugTrace(hullLog.str());
            }

            if (!generatedCollider.polygons.empty())
            {
                std::ostringstream polyLog;
                polyLog << kLogPrefix << "Generated polygon decomposition for kind '" << kind
                    << "' parts=" << generatedCollider.polygons.size() << " vertices=";
                for (size_t i = 0; i < generatedCollider.polygons.size(); ++i)
                {
                    polyLog << (i == 0 ? '[' : ',') << generatedCollider.polygons[i].size();
                }
                polyLog << "] thickness=" << generatedCollider.thickness << "\n";
                DebugTrace(polyLog.str());
            }

            {
                const float3 baseHalfExtents = ComputeHalfExtents(bounds, float3{ 1.0f, 1.0f, 1.0f }, preset.colliderPreset);
                const float3 baseCenter = ComputeColliderOffset(bounds, float3{ 1.0f, 1.0f, 1.0f });
                std::ostringstream boundsLog;
                boundsLog << kLogPrefix << "Resolved collider template for kind '" << kind
                    << "' shape=" << ToString(preset.colliderPreset.shape)
                    << " halfExtents=(" << baseHalfExtents.x << ',' << baseHalfExtents.y << ',' << baseHalfExtents.z << ")"
                    << " center=(" << baseCenter.x << ',' << baseCenter.y << ',' << baseCenter.z << ")\n";
                DebugTrace(boundsLog.str());
            }

            std::ostringstream oss;
            oss << kLogPrefix << "Registering sweets kind '" << kind
                << "' with asset '" << modelPath << "'.\n";
            DebugTrace(oss.str());

            const SweetsType type = prototype.type;

            auto factory = [this, &cache, kind, modelPath, type, preset, bounds, generatedCollider](const Transform& transform)
                -> std::shared_ptr<ActiveSweets>
            {
                auto model = cache.Get(modelPath);
                if (!model)
                {
                    std::ostringstream fail;
                    fail << kLogPrefix << "Model load failed for sweets kind '" << kind
                        << "' path='" << modelPath << "'.\n";
                    DebugTrace(fail.str());
                    return nullptr;
                }

                ApplyFallbackMaterials(*model, type);

                Transform scaledTransform = transform;
                const auto sampleScale = [&](float minValue, float maxValue)
                {
                    if (!m_enableRandomScale)
                    {
                        return 1.0f;
                    }

                    const float lo = std::min(minValue, maxValue);
                    const float hi = std::max(minValue, maxValue);
                    if (lo == hi)
                    {
                        return lo;
                    }

                    std::uniform_real_distribution<float> dist(lo, hi);
                    return dist(m_random);
                };

                const float3 sampledScale{
                    sampleScale(preset.minScale.x, preset.maxScale.x),
                    sampleScale(preset.minScale.y, preset.maxScale.y),
                    sampleScale(preset.minScale.z, preset.maxScale.z)
                };

                scaledTransform.scale.x *= sampledScale.x;
                scaledTransform.scale.y *= sampledScale.y;
                scaledTransform.scale.z *= sampledScale.z;
                std::shared_ptr<Collision::ICollider> collider;
                const bool hasGeneratedHull = !generatedCollider.hull.empty();
                const bool hasGeneratedPolygons = !generatedCollider.polygons.empty();
                bool usedGeneratedHull = false;
                bool usedGeneratedCompound = false;
                bool colliderDefinitionFound = false;
                bool colliderDefinitionFailed = false;
                bool colliderDefinitionEmpty = false;
                bool colliderDefinitionHullMissingPoints = false;
                bool colliderDefinitionIsCompound = false;
                bool colliderDefinitionIsHull = false;
                bool colliderDefinitionIsCircle = false;
                const Collision::ColliderDefinition* colliderDefinition = nullptr;
                const bool warnedMissingColliderDefinition = m_warnedMissingColliderDefinitionKinds.find(kind) != m_warnedMissingColliderDefinitionKinds.end();
                std::string colliderTypeChosen = "unresolved";
                std::string colliderResolutionReason = "unresolved";

                const auto colliderDef = m_colliderDefinitions.find(kind);
                if (colliderDef != m_colliderDefinitions.end())
                {
                    colliderDefinitionFound = true;
                    colliderDefinition = &colliderDef->second;
                    colliderDefinitionIsCompound = colliderDefinition->type == Collision::ColliderKind::Compound;
                    colliderDefinitionIsHull = colliderDefinition->type == Collision::ColliderKind::Hull;
                    colliderDefinitionIsCircle = colliderDefinition->type == Collision::ColliderKind::Circle;

                    colliderDefinitionHullMissingPoints = colliderDefinitionIsHull && colliderDefinition->points.empty();
                    colliderDefinitionEmpty = IsColliderDefinitionEmpty(*colliderDefinition);
                }

                const float3 halfExtents = ComputeHalfExtents(bounds, scaledTransform.scale, preset.colliderPreset);
                const float3 colliderOffset = ComputeColliderOffset(bounds, scaledTransform.scale);
                const float3 colliderExpand = ComputeExpandFromHalfExtents(halfExtents, scaledTransform.scale);

                const bool configDefinitionAvailable = colliderDefinitionFound && !colliderDefinitionEmpty;
                const bool configOverrideRequested = configDefinitionAvailable
                    && m_colliderConfigOverridesEnabled && !m_forceGeneratedColliders;
                const bool preferGenerated = m_forceGeneratedColliders || !configOverrideRequested;

                {
                    std::ostringstream plan;
                    plan << kLogPrefix << "Collider resolve plan kind='" << kind << "'"
                        << " preferGenerated=" << std::boolalpha << preferGenerated
                        << " forceGenerated=" << m_forceGeneratedColliders
                        << " configOverride=" << m_colliderConfigOverridesEnabled
                        << " configFound=" << colliderDefinitionFound
                        << " configEmpty=" << colliderDefinitionEmpty
                        << " hasPolygons=" << hasGeneratedPolygons
                        << " hasHull=" << hasGeneratedHull
                        << "\n";
                    DebugTrace(plan.str());
                }

                const auto tryBuildGeneratedColliders = [&]()
                {
                    if (collider)
                    {
                        return;
                    }

                    if (hasGeneratedPolygons)
                    {
                        collider = BuildCompoundFromPolygons(generatedCollider.polygons, generatedCollider.thickness, Collision::CollisionLayer::Sweets);
                        usedGeneratedHull = collider != nullptr;
                        usedGeneratedCompound = collider != nullptr;
                        if (collider)
                        {
                            colliderTypeChosen = "generated compound";
                            if (colliderDefinitionHullMissingPoints)
                            {
                                colliderResolutionReason = "config hull empty -> generated polygons";
                            }
                            else if (colliderDefinitionFailed)
                            {
                                colliderResolutionReason = "build failed -> generated polygons";
                            }
                            else if (colliderDefinitionEmpty)
                            {
                                colliderResolutionReason = "config empty -> generated polygons";
                            }
                            else if (!colliderDefinitionFound)
                            {
                                colliderResolutionReason = "no config -> generated polygons";
                            }
                            else
                            {
                                colliderResolutionReason = configOverrideRequested
                                    ? "config override bypassed -> generated polygons"
                                    : "generated polygons used";
                            }

                            if (!colliderDefinitionFound || colliderDefinitionEmpty || colliderDefinitionFailed || !preferGenerated)
                            {
                                std::ostringstream warn;
                                warn << kLogPrefix << "Using generated polygon decomposition for '" << kind
                                    << "' parts=" << generatedCollider.polygons.size()
                                    << " thickness=" << generatedCollider.thickness
                                    << " paddingFactor=" << m_generatedColliderThicknessPadding << "\n";
                                DebugTrace(warn.str());
                                if ((!colliderDefinitionFound || colliderDefinitionEmpty) && !warnedMissingColliderDefinition)
                                {
                                    m_warnedMissingColliderDefinitionKinds.insert(kind);
                                }
                            }
                        }
                    }

                    if (!collider && hasGeneratedHull)
                    {
                        collider = std::make_shared<Collision::ConvexHullCollider2D>(generatedCollider.hull, float2{ 0.0f, 0.0f }, generatedCollider.thickness, Collision::CollisionLayer::Sweets);
                        usedGeneratedHull = true;
                        if (colliderDefinitionHullMissingPoints)
                        {
                            std::ostringstream warn;
                            warn << kLogPrefix << "Hull collider definition for '" << kind
                                << "' had no usable points; using auto-generated hull points=" << generatedCollider.hull.size()
                                << " scale=(" << sampledScale.x << ',' << sampledScale.y << ',' << sampledScale.z << ')'
                                << " margin=(" << colliderExpand.x << ',' << colliderExpand.y << ',' << colliderExpand.z << ')'
                                << " thickness=" << generatedCollider.thickness
                                << " paddingFactor=" << m_generatedColliderThicknessPadding << "\n";
                            DebugTrace(warn.str());
                        }
                        else if (colliderDefinitionFailed)
                        {
                            std::ostringstream warn;
                            warn << kLogPrefix << "Collider definition for '" << kind
                                << "' failed or was empty; using generated hull points=" << generatedCollider.hull.size()
                                << " thickness=" << generatedCollider.thickness << "\n";
                            DebugTrace(warn.str());
                        }
                        else if ((!colliderDefinitionFound || colliderDefinitionEmpty) && !warnedMissingColliderDefinition)
                        {
                            std::ostringstream warn;
                            warn << kLogPrefix << "Using auto-generated hull for '" << kind << "' ("
                                << (colliderDefinitionEmpty ? "empty collider config" : "no collider config found") << ")"
                                << " points=" << generatedCollider.hull.size()
                                << " scale=(" << sampledScale.x << ',' << sampledScale.y << ',' << sampledScale.z << ')'
                                << " margin=(" << colliderExpand.x << ',' << colliderExpand.y << ',' << colliderExpand.z << ')'
                                << " thickness=" << generatedCollider.thickness
                                << " paddingFactor=" << m_generatedColliderThicknessPadding << "\n";
                            DebugTrace(warn.str());
                            m_warnedMissingColliderDefinitionKinds.insert(kind);
                        }
                        else if (colliderDefinitionFound && !colliderDefinitionEmpty && !preferGenerated)
                        {
                            std::ostringstream warn;
                            warn << kLogPrefix << "Generated hull used despite available collider config for '" << kind
                                << "' because generated colliders are preferred.\n";
                            DebugTrace(warn.str());
                        }

                        if (colliderDefinitionHullMissingPoints)
                        {
                            colliderResolutionReason = "config hull empty -> generated hull";
                        }
                        else if (colliderDefinitionFailed)
                        {
                            colliderResolutionReason = "build failed -> generated hull";
                        }
                        else if (colliderDefinitionEmpty)
                        {
                            colliderResolutionReason = "config empty -> generated hull";
                        }
                        else if (!colliderDefinitionFound)
                        {
                            colliderResolutionReason = "no config -> generated hull";
                        }
                        else
                        {
                            colliderResolutionReason = configOverrideRequested
                                ? "config override bypassed -> generated hull"
                                : "generated hull used";
                        }
                        colliderTypeChosen = "generated hull";
                    }
                };

                const auto tryBuildConfigColliders = [&]()
                {
                    if (!colliderDefinitionFound)
                    {
                        return;
                    }

                    if (colliderDefinitionIsCircle)
                    {
                        collider = std::make_shared<Collision::CircleCollider2D>(colliderDefinition->radius, float2{ 0.0f, 0.0f }, generatedCollider.thickness, Collision::CollisionLayer::Sweets);
                        if (!collider)
                        {
                            colliderDefinitionFailed = true;
                            return;
                        }

                        colliderTypeChosen = "circle from config";
                        colliderResolutionReason = "config circle used";
                        return;
                    }

                    if (colliderDefinitionIsHull)
                    {
                        const auto colliderHull = colliderDefinition->points;
                        if (colliderHull.empty())
                        {
                            colliderDefinitionHullMissingPoints = true;
                            return;
                        }

                        std::vector<float2> scaled;
                        scaled.reserve(colliderHull.size());
                        const auto clampMargin = [](float value)
                        {
                            if (!std::isfinite(value))
                            {
                                return 0.0f;
                            }
                            return std::clamp(value, -0.25f, 0.25f);
                        };
                        const float xMargin = clampMargin(colliderDefinition->margin.x);
                        const float yMargin = clampMargin(colliderDefinition->margin.y);
                        const float zMargin = clampMargin(colliderDefinition->margin.z);
                        for (const auto& point : colliderHull)
                        {
                            scaled.emplace_back(float2{ point.x * colliderDefinition->scale.x + xMargin,
                                point.y * colliderDefinition->scale.y + zMargin });
                        }

                        collider = std::make_shared<Collision::ConvexHullCollider2D>(scaled, float2{ colliderDefinition->offset.x + xMargin,
                            colliderDefinition->offset.z + zMargin }, generatedCollider.thickness + yMargin, Collision::CollisionLayer::Sweets);
                        if (!collider)
                        {
                            colliderDefinitionFailed = true;
                            return;
                        }

                        colliderTypeChosen = "hull from config";
                        colliderResolutionReason = "config hull used";
                        return;
                    }

                    if (colliderDefinitionIsCompound)
                    {
                        collider = Collision::BuildColliderFromDefinition(*colliderDefinition, Collision::CollisionLayer::Sweets);
                        if (!collider)
                        {
                            colliderDefinitionFailed = true;
                            return;
                        }

                        colliderTypeChosen = "compound from config";
                        colliderResolutionReason = "config compound used";
                        return;
                    }
                };

                if (preferGenerated)
                {
                    tryBuildGeneratedColliders();
                    tryBuildConfigColliders();
                }
                else
                {
                    tryBuildConfigColliders();
                    tryBuildGeneratedColliders();
                }

                if (colliderDefinitionFound && !colliderDefinitionEmpty && !collider)
                {
                    colliderDefinitionFailed = true;

                    std::ostringstream warn;
                    warn << kLogPrefix << "Collider definition for '" << kind
                        << "' could not be turned into a collider; falling back to generated or AABB.\n";
                    DebugTrace(warn.str());
                }

                const float3 velocity{ 0.0f, 0.0f, 0.0f };
                const float3 force{ 0.0f, 0.0f, 0.0f };
                const float scaleFactor = sampledScale.x * sampledScale.y * sampledScale.z;
                const float scaledWeight = preset.weight * scaleFactor;
                const float scaledDamping = std::min(preset.damping * scaleFactor, 0.999f);

                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Factory spawn kind='" << kind << "' scale=("
                        << sampledScale.x << ',' << sampledScale.y << ',' << sampledScale.z << ")"
                        << " enabledRandomScale=" << std::boolalpha << m_enableRandomScale
                        << " weight=" << scaledWeight << " (base=" << preset.weight << ")\n";
                    DebugTrace(oss.str());
                }

                {
                    std::string polygonVertexSummary;
                    if (hasGeneratedPolygons)
                    {
                        std::ostringstream contourStream;
                        contourStream << '[';
                        for (size_t i = 0; i < generatedCollider.polygons.size(); ++i)
                        {
                            contourStream << (i == 0 ? "" : ",") << generatedCollider.polygons[i].size();
                        }
                        contourStream << ']';
                        polygonVertexSummary = contourStream.str();
                    }

                    std::ostringstream oss;
                    oss << kLogPrefix << "Collider resolved kind='" << kind << "'"
                        << " type=" << colliderTypeChosen
                        << " reason=" << colliderResolutionReason
                        << " sampledScale=(" << sampledScale.x << ',' << sampledScale.y << ',' << sampledScale.z << ')'
                        << " appliedScale=(" << scaledTransform.scale.x << ',' << scaledTransform.scale.y << ',' << scaledTransform.scale.z << ")"
                        << " halfExtents=(" << halfExtents.x << ',' << halfExtents.y << ',' << halfExtents.z << ")"
                        << " offset=(" << colliderOffset.x << ',' << colliderOffset.y << ',' << colliderOffset.z << ")"
                        << " expand=(" << colliderExpand.x << ',' << colliderExpand.y << ',' << colliderExpand.z << ")"
                        << " defFound=" << std::boolalpha << colliderDefinitionFound
                        << " defEmpty=" << std::boolalpha << colliderDefinitionEmpty
                        << " buildFailed=" << std::boolalpha << colliderDefinitionFailed
                        << " forceGenerated=" << std::boolalpha << m_forceGeneratedColliders
                        << " configOverride=" << std::boolalpha << m_colliderConfigOverridesEnabled
                        << " usedGeneratedHull=" << std::boolalpha << usedGeneratedHull
                        << " usedGeneratedCompound=" << std::boolalpha << usedGeneratedCompound
                        << " hullAvailable=" << std::boolalpha << hasGeneratedHull
                        << " polygonAvailable=" << std::boolalpha << hasGeneratedPolygons
                        << " hullPoints=" << generatedCollider.hull.size()
                        << " polygonParts=" << generatedCollider.polygons.size()
                        << " polygonVertexCounts=" << (polygonVertexSummary.empty() ? "[]" : polygonVertexSummary)
                        << " paddingFactor=" << m_generatedColliderThicknessPadding
                        << "\n";
                    DebugTrace(oss.str());
                }

                return MakeSweets<ActiveSweets>(kind, model, scaledTransform, collider, Collision::CollisionLayer::Sweets, velocity, force, scaledWeight, scaledDamping,
                    Core::UpdatePhase::Physics, 0);
            };

            auto [_, inserted] = m_spawner.emplace(kind, std::move(factory));
            if (!inserted)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "Spawner for kind '" << kind
                    << "' already existed. New factory ignored.\n";
                DebugTrace(warn.str());
            }
        }

        ApplyDefaultPhaseConfigs();
        ApplyDefaultPhaseConfigs();

    }

    SweetsManager::~SweetsManager()
    {
        if (m_scheduler && m_lateUpdateNode)
        {
            m_scheduler->Unregister(*m_lateUpdateNode);
        }
    }

    void SweetsManager::OnUpdate(Core::UpdateContext& ctx)
    {
        RunSpawnScheduler(ctx.dt);

        struct PendingStateChange
        {
            std::shared_ptr<ActiveSweets> sweets;
            SweetsState state;
        };

        std::vector<PendingStateChange> stateChanges;
        std::vector<std::shared_ptr<ActiveSweets>> missedSweets;
        const auto horizontalBounds = GetHorizontalBounds();
        const float fallbackHorizontalLimit = ComputeFallbackHorizontalLimit(m_horizontalBoundsMargin);

        for (auto& entry : m_byState)
        {
            SweetsState bucketState = entry.first;
            auto& sweetsList = entry.second;

            auto it = sweetsList.begin();
            while (it != sweetsList.end())
            {
                std::shared_ptr<ActiveSweets> sweets = *it;

                if (!sweets)
                {
#if defined(_DEBUG)
                    std::ostringstream oss;
                    oss << kLogPrefix << "Purging null sweets entry from state "
                        << StateToString(bucketState) << "\n";
                    DebugTrace(oss.str());
#endif
                    it = sweetsList.erase(it);
                    continue;
                }

                if (sweets->IsPendingDestroy())
                {
                    ++it;
                    continue;
                }

                if (!sweets->IsEnabled())
                {
#if defined(_DEBUG)
                    std::ostringstream oss;
                    oss << kLogPrefix << "Purging sweets ptr=" << sweets.get()
                        << " from state " << StateToString(bucketState)
                        << " because update node was disabled.\n";
                    DebugTrace(oss.str());
#endif
                    m_lookup.erase(sweets);
                    it = sweetsList.erase(it);
                    continue;
                }

                const bool isCarried = (bucketState == SweetsState::Carried);

                if (!isCarried)
                {
                    bool usedFallbackBounds = false;
                    const bool outOfBounds = IsHorizontallyOutOfBounds(
                        *sweets,
                        horizontalBounds,
                        m_horizontalBoundsMargin,
                        fallbackHorizontalLimit,
                        &usedFallbackBounds);
                    if (!sweets->GetCollider() && !m_warnedMissingBoundsCollider)
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Horizontal bounds check using transform scale for sweets ptr="
                            << sweets.get() << " (missing collider).\n";
                        DebugTrace(oss.str());
                        m_warnedMissingBoundsCollider = true;
                    }

                    if (usedFallbackBounds && !m_warnedMissingHorizontalBounds)
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Horizontal bounds fallback active; using platform width limit="
                            << fallbackHorizontalLimit << " (half-width + margin).\n";
                        DebugTrace(oss.str());
                        m_warnedMissingHorizontalBounds = true;
                    }

                    if (outOfBounds)
                    {
                        missedSweets.push_back(sweets);
                        ++it;
                        continue;
                    }

                    sweets->OnUpdate(ctx);

                    const SweetsState desiredState = EvaluateState(*sweets, bucketState);
                    if (desiredState != bucketState)
                    {
                        stateChanges.push_back({ sweets, desiredState });
                    }
                }

                ++it;
            }
        }

        for (const auto& change : stateChanges)
        {
            auto lookup = m_lookup.find(change.sweets);
            const SweetsState previousState = (lookup != m_lookup.end()) ? lookup->second : SweetsState::Active;
            SetState(change.sweets, change.state);
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << kLogPrefix << "State change ptr=" << change.sweets.get()
                << " " << StateToString(previousState) << " -> "
                << StateToString(change.state) << "\n";
            DebugTrace(oss.str());
#endif
        }

        DeleteBatchContext missBatch{};
        missBatch.usesRemaining = static_cast<int>(missedSweets.size());
        for (const auto& sweets : missedSweets)
        {
            RemoveSweetsEntry(sweets, DeleteReason::Miss, "MissDespawn", true, &missBatch);
        }
    }

    void SweetsManager::OnLateUpdate(Core::UpdateContext& ctx)
    {
        (void)ctx;

        std::size_t removed = 0;
        std::size_t markedSafe = 0;
        std::size_t stagedThisFrame = 0;

        for (auto& entry : m_byState)
        {
            auto& sweetsList = entry.second;
            auto it = sweetsList.begin();
            while (it != sweetsList.end())
            {
                const auto& sweets = *it;
                if (!sweets)
                {
                    it = sweetsList.erase(it);
                    ++removed;
                    continue;
                }

                if (sweets->IsPendingDestroy())
                {
                    auto watched = m_pendingLateDeletion.find(sweets);
                    if (watched == m_pendingLateDeletion.end())
                    {
                        m_pendingLateDeletion.insert(sweets);
                        ++stagedThisFrame;
                        ++it;
                        continue;
                    }

                    if (!sweets->IsSafeToDelete())
                    {
                        sweets->MarkSafeToDelete();
                        ++markedSafe;
                        ++it;
                        continue;
                    }

                    m_lookup.erase(sweets);
                    m_pendingLateDeletion.erase(watched);
                    it = sweetsList.erase(it);
                    ++removed;
                    continue;
                }

                ++it;
            }
        }

#if defined(_DEBUG)
        if (removed > 0 || markedSafe > 0 || stagedThisFrame > 0)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Late prune staged " << stagedThisFrame
                << " pending entries, marked " << markedSafe
                << " ready for deletion, removed " << removed
                << " finalized sweets.\n";
            DebugTrace(oss.str());
        }
#endif
    }

    void SweetsManager::DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<CameraSpace::ICamera>& camera)
    {
        if (!camera)
        {
            return;
        }

        for (auto& entry : m_byState)
        {
            SweetsList& sweetsList = entry.second;
            for (auto& sweets : sweetsList)
            {
                if (sweets && !sweets->IsPendingDestroy())
                {
                    sweets->Draw(renderer, *camera);
                }
            }
        }
    }

    SweetsManager::SweetsList& SweetsManager::GetByState(SweetsState state)
    {
        return m_byState[state];
    }

    const SweetsManager::SweetsList& SweetsManager::GetByState(SweetsState state) const
    {
        static const SweetsList kEmpty;
        auto it = m_byState.find(state);
        if (it == m_byState.end())
        {
            return kEmpty;
        }
        return it->second;
    }

    float SweetsManager::GetHighestStackedHeight() const
    {
        float maxHeight = 0.0f;
        const auto& stacked = GetByState(SweetsState::Stacked);
        for (const auto& sweets : stacked)
        {
            if (!sweets || sweets->IsPendingDestroy())
            {
                continue;
            }

            float sweetsTop = sweets->GetTransform().position.y;
            const Collision::ICollider* collider = sweets->GetCollider();
            if (collider)
            {
                const AABB bounds = collider->GetAABB();
                sweetsTop = bounds.max.y;
            }
            else
            {
                const Transform& transform = sweets->GetTransform();
                const float halfHeight = std::max(0.0f, transform.scale.y * 0.5f);
                sweetsTop = transform.position.y + halfHeight;
#if defined(_DEBUG)
                if (!m_warnedMissingHeightCollider)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "GetHighestStackedHeight fallback to transform scale"
                        << " for sweets ptr=" << sweets.get() << " (missing collider).\n";
                    DebugTrace(oss.str());
                    m_warnedMissingHeightCollider = true;
                }
#endif
            }

            maxHeight = std::max(maxHeight, sweetsTop);
        }

        return maxHeight;
    }

    void SweetsManager::SetState(const std::shared_ptr<ActiveSweets>& sweets, SweetsState state)
    {
        if (!sweets)
        {
            return;
        }

        if (sweets->IsPendingDestroy())
        {
            return;
        }

        auto found = m_lookup.find(sweets);
        const SweetsState previousState = (found != m_lookup.end()) ? found->second : SweetsState::Active;
        if (found != m_lookup.end())
        {
            if (found->second == state)
            {
                return;
            }

            auto& currentList = GetByState(found->second);
            currentList.remove(sweets);
        }

        auto& targetList = GetByState(state);
        targetList.push_back(sweets);
        m_lookup[sweets] = state;

        if (state == SweetsState::Stacked && previousState != SweetsState::Stacked)
        {
            ++m_stackedInBatch;

            std::ostringstream oss;
            oss << kLogPrefix << "Stacked sweets progress=" << m_stackedInBatch
                << '/' << m_batchStackTarget << " tag='" << sweets->GetTag() << "'.\n";
            DebugTrace(oss.str());
        }
    }

    void SweetsManager::Erase(const std::shared_ptr<ActiveSweets>& sweets)
    {
        if (!sweets)
        {
            return;
        }

        auto found = m_lookup.find(sweets);
        if (found == m_lookup.end())
        {
            return;
        }

        if (sweets->IsPendingDestroy())
        {
            return;
        }

        sweets->MarkPendingDestroy();
    }

    void SweetsManager::ClearAllSweets()
    {
        ForceSpawnRefresh();

        std::size_t staged = 0;
        std::size_t alreadyPending = 0;

        for (auto& entry : m_byState)
        {
            SweetsList& list = entry.second;
            for (auto& sweets : list)
            {
                if (!sweets)
                {
                    continue;
                }

                if (sweets->IsPendingDestroy())
                {
                    ++alreadyPending;
                }
                else
                {
                    sweets->MarkPendingDestroy();
                    sweets->Enable(false);
                    ++staged;
                }

                UnregisterSweetsFromSystems(sweets, "ClearAllSweets");
            }
        }

        std::ostringstream oss;
        oss << kLogPrefix << "ClearAllSweets staged " << staged
            << " sweets for pending destroy (" << alreadyPending << " already pending) across "
            << m_byState.size() << " states.\n";
        DebugTrace(oss.str());
    }

    void SweetsManager::SetDrawSystem(Systems::DrawSystem* drawSystem)
    {
        if (m_drawSystem == drawSystem)
        {
            return;
        }

        m_drawSystem = drawSystem;
        m_warnedMissingDrawSystem = false;
    }

    void SweetsManager::SetPhysicsMachine(Systems::PhysicsMachine* physicsMachine)
    {
        if (m_physicsMachine == physicsMachine)
        {
            return;
        }

        m_physicsMachine = physicsMachine;
        m_warnedMissingPhysicsMachine = false;
    }

    void SweetsManager::SetUpdateScheduler(Core::UpdateScheduler* scheduler)
    {
        if (m_scheduler == scheduler)
        {
            return;
        }

        m_scheduler = scheduler;
        m_warnedMissingScheduler = false;
    }

    void SweetsManager::SetPhaseChangeCallback(PhaseChangeCallback callback)
    {
        m_phaseChangeCallback = std::move(callback);
        m_warnedMissingPhaseCallback = false;
    }

    void SweetsManager::NotifyPhaseChanged(int newPhase)
    {
        if (newPhase < 0)
        {
            newPhase = 0;
        }

        if (newPhase == m_currentPhase)
        {
            return;
        }

        m_currentPhase = newPhase;

        std::ostringstream oss;
        oss << kLogPrefix << "Phase changed to " << m_currentPhase << "\n";
        DebugTrace(oss.str());

        OnPhaseConfigurationChanged(m_currentPhase);

        if (m_phaseChangeCallback)
        {
            m_phaseChangeCallback(m_currentPhase);
        }
        else if (!m_warnedMissingPhaseCallback)
        {
            DebugTrace(std::string(kLogPrefix) + "Phase change callback missing; spawn layout unchanged.\n");
            m_warnedMissingPhaseCallback = true;
        }
    }

    int SweetsManager::GetCurrentPhase() const noexcept
    {
        return m_currentPhase;
    }

    Core::IUpdatable* SweetsManager::GetLateUpdateNode()
    {
        return m_lateUpdateNode.get();
    }

    const Core::IUpdatable* SweetsManager::GetLateUpdateNode() const
    {
        return m_lateUpdateNode.get();
    }

    std::shared_ptr<ActiveSweets> SweetsManager::Spawn(const std::string& kind, const Transform& transform)
    {
        auto it = m_spawner.find(kind);
        if (it == m_spawner.end())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawn requested for unknown kind '" << kind << "'. Registered kinds:";
            if (m_spawner.empty())
            {
                oss << " (none)";
            }
            else
            {
                bool first = true;
                for (const auto& entry : m_spawner)
                {
                    oss << (first ? ' ' : ',') << entry.first;
                    first = false;
                }
            }
            oss << "\n";
            DebugTrace(oss.str());
            return nullptr;
        }

        auto sweets = it->second(transform);
        if (!sweets)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawner for kind '" << kind << "' returned null instance.\n";
            DebugTrace(oss.str());
            return nullptr;
        }

        SetState(sweets, SweetsState::Active);
        RegisterWithSystems(sweets);

        return sweets;
    }

    std::shared_ptr<ActiveSweets> SweetsManager::FindNearestCarryable(const float3& position, float radius) const
    {
        const float radiusSq = radius * radius;
        std::shared_ptr<ActiveSweets> best;
        float bestDistanceSq = radiusSq;

        const auto scanState = [&](SweetsState state)
        {
            auto it = m_byState.find(state);
            if (it == m_byState.end())
            {
                return;
            }

            for (const auto& sweets : it->second)
            {
                if (!sweets || sweets->IsPendingDestroy())
                {
                    continue;
                }

                const auto& sweetsPos = sweets->GetTransform().position;
                const float dx = sweetsPos.x - position.x;
                const float dy = sweetsPos.y - position.y;
                const float dz = sweetsPos.z - position.z;
                const float distanceSq = (dx * dx) + (dy * dy) + (dz * dz);
                if (distanceSq <= bestDistanceSq)
                {
                    bestDistanceSq = distanceSq;
                    best = sweets;
                }
            }
        };

        scanState(SweetsState::Ground);
        scanState(SweetsState::Active);

        return best;
    }

    bool SweetsManager::TryMarkAsCarried(const std::shared_ptr<ActiveSweets>& sweets)
    {
        if (!sweets)
        {
            return false;
        }

        if (sweets->IsPendingDestroy())
        {
            return false;
        }

        auto it = m_lookup.find(sweets);
        const SweetsState current = (it != m_lookup.end()) ? it->second : SweetsState::Active;
        if (current != SweetsState::Active && current != SweetsState::Ground)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "TryMarkAsCarried rejected tag='" << sweets->GetTag()
                << "' currentState=" << StateToString(current) << "\n";
            DebugTrace(oss.str());
            return false;
        }

        if (m_physicsMachine)
        {
            m_physicsMachine->UnregisterBody(*sweets);
        }
        else if (!m_warnedMissingPhysicsMachine)
        {
            DebugTrace(std::string(kLogPrefix) + "TryMarkAsCarried missing physics machine; body may keep simulating.\n");
            m_warnedMissingPhysicsMachine = true;
        }

        SetState(sweets, SweetsState::Carried);

        std::ostringstream oss;
        oss << kLogPrefix << "TryMarkAsCarried success tag='" << sweets->GetTag() << "'\n";
        DebugTrace(oss.str());
        return true;
    }

    bool SweetsManager::ReleaseCarriedSweets(const std::shared_ptr<ActiveSweets>& sweets)
    {
        if (!sweets)
        {
            return false;
        }

        if (sweets->IsPendingDestroy())
        {
            return false;
        }

        auto it = m_lookup.find(sweets);
        if (it == m_lookup.end() || it->second != SweetsState::Carried)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "ReleaseCarriedSweets rejected tag='" << sweets->GetTag() << "' state="
                << (it == m_lookup.end() ? "<unknown>" : StateToString(it->second)) << "\n";
            DebugTrace(oss.str());
            return false;
        }

        SetState(sweets, SweetsState::Active);

        if (m_physicsMachine)
        {
            m_physicsMachine->RegisterBody(*sweets);
        }
        else if (!m_warnedMissingPhysicsMachine)
        {
            DebugTrace(std::string(kLogPrefix) + "ReleaseCarriedSweets missing physics machine; body will not simulate.\n");
            m_warnedMissingPhysicsMachine = true;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "ReleaseCarriedSweets success tag='" << sweets->GetTag() << "'\n";
        DebugTrace(oss.str());
        return true;
    }

    std::optional<std::pair<float, float>> SweetsManager::GetHorizontalBounds() const
    {
        if (!m_hasHorizontalBounds)
        {
            return std::nullopt;
        }

        return std::make_pair(m_minHorizontalBound, m_maxHorizontalBound);
    }

    void SweetsManager::RegisterWithSystems(const std::shared_ptr<ActiveSweets>& sweets)
    {
        if (!sweets)
        {
            return;
        }

        if (m_scheduler)
        {
            m_scheduler->Register(*sweets);
        }
        else if (!m_warnedMissingScheduler)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawned sweets '" << sweets->GetTag()
                << "' missing scheduler registration: scheduler not configured.\n";
            DebugTrace(oss.str());
            m_warnedMissingScheduler = true;
        }

        if (m_physicsMachine)
        {
            m_physicsMachine->RegisterBody(*sweets);
        }
        else if (!m_warnedMissingPhysicsMachine)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawned sweets '" << sweets->GetTag()
                << "' missing physics registration: physics machine not configured.\n";
            DebugTrace(oss.str());
            m_warnedMissingPhysicsMachine = true;
        }

        if (m_drawSystem)
        {
            m_drawSystem->RegisterRenderable(*sweets);
        }
        else if (!m_warnedMissingDrawSystem)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawned sweets '" << sweets->GetTag()
                << "' missing draw registration: draw system not configured.\n";
            DebugTrace(oss.str());
            m_warnedMissingDrawSystem = true;
        }
    }

    void SweetsManager::UnregisterSweetsFromSystems(const std::shared_ptr<ActiveSweets>& sweets, const char* context, bool logWarnings)
    {
        if (!sweets)
        {
            return;
        }

        if (m_scheduler)
        {
            m_scheduler->Unregister(*sweets);
        }
        else if (logWarnings && !m_warnedMissingScheduler)
        {
            std::ostringstream oss;
            oss << kLogPrefix << context << " could not unregister sweets '" << sweets->GetTag()
                << "': scheduler not configured.\n";
            DebugTrace(oss.str());
            m_warnedMissingScheduler = true;
        }

        if (m_physicsMachine)
        {
            m_physicsMachine->UnregisterBody(*sweets);
        }
        else if (logWarnings && !m_warnedMissingPhysicsMachine)
        {
            std::ostringstream oss;
            oss << kLogPrefix << context << " could not unregister physics for sweets '" << sweets->GetTag()
                << "': physics machine not configured.\n";
            DebugTrace(oss.str());
            m_warnedMissingPhysicsMachine = true;
        }

        if (m_drawSystem)
        {
            m_drawSystem->UnregisterRenderable(*sweets);
        }
        else if (logWarnings && !m_warnedMissingDrawSystem)
        {
            std::ostringstream oss;
            oss << kLogPrefix << context << " could not unregister draw entry for sweets '" << sweets->GetTag()
                << "': draw system not configured.\n";
            DebugTrace(oss.str());
            m_warnedMissingDrawSystem = true;
        }
    }

    void SweetsManager::RemoveSweetsEntry(const std::shared_ptr<ActiveSweets>& sweets,
        DeleteReason reason,
        const char* context,
        bool logWarnings,
        DeleteBatchContext* batchContext)
    {
        if (!sweets)
        {
            return;
        }

        auto found = m_lookup.find(sweets);
        if (found == m_lookup.end())
        {
            return;
        }

        if (sweets->IsPendingDestroy())
        {
            return;
        }

        const SweetsState previousState = found->second;
        sweets->MarkPendingDestroy();
        sweets->Enable(false);

        const UseOutcome outcome = (reason == DeleteReason::Hit) ? UseOutcome::Hit : UseOutcome::Miss;
        UpdateBatchUsage(batchContext, outcome);

        UnregisterSweetsFromSystems(sweets, context, logWarnings);

        const int remainingToStack = std::max(0, m_batchStackTarget - m_stackedInBatch);
        const int liveAfterRemoval = CountLiveSweets();
        const int desiredLiveTarget = ComputeDesiredLiveTarget(remainingToStack);
        if (!m_batchCoolingDown && desiredLiveTarget > 0 && liveAfterRemoval < desiredLiveTarget)
        {
            m_pendingTopUpAfterDrop = true;

            std::ostringstream oss;
            oss << kLogPrefix << context << " detected live sweets drop below target; scheduling delayed top-up. live="
                << liveAfterRemoval << " target=" << desiredLiveTarget << " remainingToStack=" << remainingToStack
                << " stacked=" << m_stackedInBatch << '/' << m_batchStackTarget
                << " queue=" << m_spawnQueue.size()
                << " burstInterval=" << std::fixed << std::setprecision(2) << m_activeBurstInterval << "\n";
            DebugTrace(oss.str());
        }

#if defined(_DEBUG)
        const auto& pos = sweets->GetTransform().position;
        std::ostringstream oss;
        oss << kLogPrefix << context << " removed sweets tag='" << sweets->GetTag()
            << "' state=" << StateToString(previousState)
            << " deleteReason=" << SweetsSpace::ToString(reason)
            << " outcome=" << SweetsSpace::ToString(outcome)
            << " batchUsesRemaining=" << (batchContext ? batchContext->usesRemaining : -1)
            << " pos=(" << pos.x << ',' << pos.y << ',' << pos.z << ")\n";
        DebugTrace(oss.str());
#endif
    }

    bool SweetsManager::LoadPhaseSpawnConfig(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Phase spawn config not found at '" << path << "'. Using defaults.\n";
            DebugTrace(oss.str());
            ApplyDefaultPhaseConfigs();
            return false;
        }

        struct PartialConfig
        {
            PhaseSpawnConfig config;
            bool hasInterval = false;
            bool hasBurstInterval = false;
            bool hasSimultaneous = false;
            bool hasSimultaneousTarget = false;
            bool hasWeights = false;
        };

        std::map<int, PartialConfig> parsed;
        int currentPhase = -1;
        bool parseError = false;
        std::string line;
        size_t lineNumber = 0;

        while (std::getline(file, line))
        {
            ++lineNumber;
            const auto commentPos = line.find_first_of("#;");
            if (commentPos != std::string::npos)
            {
                line = line.substr(0, commentPos);
            }

            line = TrimToken(line);
            if (line.empty())
            {
                continue;
            }

            const auto delimiterPos = line.find('=');
            if (delimiterPos == std::string::npos)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Spawn config line " << lineNumber << " missing '=' : " << line << "\n";
                DebugTrace(oss.str());
                parseError = true;
                continue;
            }

            const std::string key = TrimToken(line.substr(0, delimiterPos));
            const std::string value = TrimToken(line.substr(delimiterPos + 1));

            if (key == "phase")
            {
                try
                {
                    currentPhase = std::max(0, std::stoi(value));
                    parsed[currentPhase];
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse phase index on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                    currentPhase = -1;
                }
                continue;
            }

            if (currentPhase < 0)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Encountered key '" << key << "' on line " << lineNumber
                    << " before declaring a phase index.\n";
                DebugTrace(oss.str());
                parseError = true;
                continue;
            }

            auto& partial = parsed[currentPhase];
            if (key == "interval")
            {
                try
                {
                    partial.config.spawnIntervalSeconds = std::max(kMinimumSpawnIntervalSeconds, std::stof(value));
                    partial.hasInterval = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse interval on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "burstInterval")
            {
                try
                {
                    partial.config.burstIntervalSeconds = std::max(kMinimumBurstIntervalSeconds, std::stof(value));
                    partial.hasBurstInterval = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse burst interval on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "simultaneous")
            {
                try
                {
                    partial.config.simultaneousSpawnCount = std::max(1, std::stoi(value));
                    partial.hasSimultaneous = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse simultaneous count on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "simultaneousTarget")
            {
                try
                {
                    partial.config.simultaneousLiveTarget = std::max(1, std::stoi(value));
                    partial.hasSimultaneousTarget = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse simultaneous target on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "weights")
            {
                bool weightError = false;
                auto parsedWeights = ParseWeights(value, &weightError);
                if (weightError)
                {
                    parseError = true;
                    continue;
                }

                std::map<SweetsType, float> collapsed;
                for (const auto& entry : parsedWeights)
                {
                    const auto* descriptor = SweetsPrototypeManifest::Instance().Find(entry.kind);
                    if (!descriptor)
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Phase " << currentPhase << " references unknown sweets type '"
                            << ToString(entry.kind) << "'.\n";
                        DebugTrace(oss.str());
                        continue;
                    }

                    const std::string normalizedKind = NormalizeKey(descriptor->key);
                    if (m_spawner.find(normalizedKind) == m_spawner.end())
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Phase " << currentPhase << " references unknown sweets kind '"
                            << descriptor->key << "'.\n";
                        DebugTrace(oss.str());
                        continue;
                    }
                    collapsed[entry.kind] += entry.weight;
                }

                partial.config.weights.clear();
                for (const auto& entry : collapsed)
                {
                    if (entry.second > 0.0f)
                    {
                        partial.config.weights.push_back({ entry.first, entry.second });
                    }
                }

                if (partial.config.weights.empty())
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Phase " << currentPhase
                        << " weights resolved to zero valid entries; falling back to defaults.\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
                else
                {
                    partial.hasWeights = true;
                }
            }
            else
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Unknown spawn config key '" << key << "' on line " << lineNumber << "\n";
                DebugTrace(oss.str());
            }
        }

        m_phaseConfigs.clear();
        for (const auto& entry : parsed)
        {
            PhaseSpawnConfig config = entry.second.config;
            if (!entry.second.hasInterval)
            {
                config.spawnIntervalSeconds = kDefaultSpawnIntervalSeconds;
            }
            config.spawnIntervalSeconds = std::max(config.spawnIntervalSeconds, kMinimumSpawnIntervalSeconds);

            if (!entry.second.hasBurstInterval)
            {
                config.burstIntervalSeconds = kDefaultBurstIntervalSeconds;
            }
            config.burstIntervalSeconds = std::max(config.burstIntervalSeconds, kMinimumBurstIntervalSeconds);

            if (!entry.second.hasSimultaneous)
            {
                config.simultaneousSpawnCount = kDefaultSimultaneousSpawnCount;
            }
            config.simultaneousSpawnCount = std::max(1, config.simultaneousSpawnCount);

            if (!entry.second.hasSimultaneousTarget)
            {
                config.simultaneousLiveTarget = config.simultaneousSpawnCount;
            }
            config.simultaneousLiveTarget = std::max(1, config.simultaneousLiveTarget);

            if (!entry.second.hasWeights || config.weights.empty())
            {
                config.weights = BuildDefaultWeights();
            }

            if (config.weights.empty())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Skipping phase " << entry.first
                    << " because no valid weights were available even after defaults.\n";
                DebugTrace(oss.str());
                continue;
            }

            m_phaseConfigs.push_back(std::move(config));
        }

        if (m_phaseConfigs.empty())
        {
            DebugTrace(std::string(kLogPrefix) + "No valid phase configs parsed; applying defaults.\n");
            ApplyDefaultPhaseConfigs();
            return false;
        }

        m_activePhaseConfigIndex = -1;
        OnPhaseConfigurationChanged(m_currentPhase);

        std::ostringstream oss;
        oss << kLogPrefix << "Loaded " << m_phaseConfigs.size()
            << " spawn phases from '" << path << "' (parseError=" << std::boolalpha << parseError << ").\n";
        DebugTrace(oss.str());
        return !parseError;
    }

    void SweetsManager::SetHorizontalBounds(float minX, float maxX, std::optional<float> margin)
    {
        if (!(std::isfinite(minX) && std::isfinite(maxX)))
        {
            DebugTrace(std::string(kLogPrefix) + "Ignored horizontal bounds update with non-finite values.\n");
            return;
        }

        const float sanitizedMin = std::min(minX, maxX);
        const float sanitizedMax = std::max(minX, maxX);
        const float sanitizedMargin = margin.has_value()
            ? std::max(0.0f, *margin)
            : m_horizontalBoundsMargin;

        const bool changed = !m_hasHorizontalBounds
            || HasMeaningfulDifference(m_minHorizontalBound, sanitizedMin)
            || HasMeaningfulDifference(m_maxHorizontalBound, sanitizedMax)
            || HasMeaningfulDifference(m_horizontalBoundsMargin, sanitizedMargin);

        m_hasHorizontalBounds = true;
        m_minHorizontalBound = sanitizedMin;
        m_maxHorizontalBound = sanitizedMax;
        m_horizontalBoundsMargin = sanitizedMargin;
        m_warnedMissingHorizontalBounds = false;

        if (!changed)
        {
            return;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "Horizontal bounds updated to min=" << m_minHorizontalBound
            << " max=" << m_maxHorizontalBound
            << " margin=" << m_horizontalBoundsMargin << " (world units).\n";
        DebugTrace(oss.str());
    }

    void SweetsManager::SetGeneratedColliderThicknessPadding(float factor)
    {
        const float sanitized = SanitizeThicknessPadding(factor);
        const bool clamped = std::fabs(sanitized - factor) > 1e-4f;
        const bool changed = std::fabs(sanitized - m_generatedColliderThicknessPadding) > 1e-4f;
        m_generatedColliderThicknessPadding = sanitized;

        std::ostringstream oss;
        oss << kLogPrefix << "Generated collider thickness padding set to " << sanitized
            << (clamped ? " (clamped to minimum of 1.0)" : "")
            << "; cached colliders will retain previously generated thickness values until regeneration.\n";
        DebugTrace(oss.str());

        if (!changed)
        {
            DebugTrace(std::string(kLogPrefix) + "Thickness padding unchanged from prior value.\n");
        }
    }

    void SweetsManager::SetForceGeneratedColliders(bool force)
    {
        const bool changed = m_forceGeneratedColliders != force;
        m_forceGeneratedColliders = force;

        std::ostringstream oss;
        oss << kLogPrefix << "Force-generated colliders " << (force ? "enabled" : "disabled")
            << "; config collider definitions "
            << (force ? "will be ignored when generated geometry is available." : "may be used as overrides or fallbacks.")
            << "\n";
        DebugTrace(oss.str());

        if (!changed)
        {
            DebugTrace(std::string(kLogPrefix) + "Force-generated collider flag unchanged from prior value.\n");
        }
    }

    void SweetsManager::SetColliderConfigOverridesEnabled(bool enabled)
    {
        const bool changed = m_colliderConfigOverridesEnabled != enabled;
        m_colliderConfigOverridesEnabled = enabled;

        std::ostringstream oss;
        oss << kLogPrefix << "Collider config overrides " << (enabled ? "enabled" : "disabled")
            << "; config definitions will "
            << (enabled ? "replace generated colliders when available (unless force-generated is enabled)."
                : "only be used when generated colliders are unavailable or forced off.")
            << "\n";
        DebugTrace(oss.str());

        if (!changed)
        {
            DebugTrace(std::string(kLogPrefix) + "Collider config override flag unchanged from prior value.\n");
        }
    }

    void SweetsManager::SetSpawnOrigins(const std::vector<float3>& origins)
    {
        m_spawnOrigins = origins;
        m_warnedMissingSpawnOrigins = m_spawnOrigins.empty();
        const auto seed = std::random_device{}();
        m_random.seed(seed);


        std::ostringstream oss;
        oss << kLogPrefix << "Configured " << m_spawnOrigins.size()
            << " spawn origins (heightOffset=" << m_spawnHeightOffset
            << " seed=" << seed << ").\n";
        DebugTrace(oss.str());
    }

    void SweetsManager::SetRandomRotationEnabled(bool enabled)
    {
        m_enableRandomRotation = enabled;

        std::ostringstream oss;
        oss << kLogPrefix << "Random rotation " << (enabled ? "enabled" : "disabled")
            << " (minYawPitchRollDeg=" << m_randomRotationMinDegrees.x << ',' << m_randomRotationMinDegrees.y
            << ',' << m_randomRotationMinDegrees.z << " maxYawPitchRollDeg="
            << m_randomRotationMaxDegrees.x << ',' << m_randomRotationMaxDegrees.y << ','
            << m_randomRotationMaxDegrees.z << ").\n";
        DebugTrace(oss.str());
    }

    void SweetsManager::SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees)
    {
        m_randomRotationMinDegrees = minDegrees;
        m_randomRotationMaxDegrees = maxDegrees;

        std::ostringstream oss;
        oss << kLogPrefix << "Random rotation range updated to minYawPitchRollDeg="
            << minDegrees.x << ',' << minDegrees.y << ',' << minDegrees.z
            << " maxYawPitchRollDeg=" << maxDegrees.x << ',' << maxDegrees.y << ','
            << maxDegrees.z << " (enabled=" << std::boolalpha << m_enableRandomRotation << ").\n";
        DebugTrace(oss.str());
    }

    void SweetsManager::SetRandomScaleEnabled(bool enabled)
    {
        m_enableRandomScale = enabled;

        std::ostringstream oss;
        oss << kLogPrefix << "Random scale " << (enabled ? "enabled" : "disabled") << ".\n";
        DebugTrace(oss.str());
    }

    void SweetsManager::SetLiveSweetsCap(int cap)
    {
        const int sanitizedCap = std::max(0, cap);
        const bool changed = (sanitizedCap != m_liveSweetsCap);
        m_liveSweetsCap = sanitizedCap;
        const bool capActive = (m_liveSweetsCap > 0);

        std::ostringstream oss;
        oss << kLogPrefix << "Live sweets cap " << (sanitizedCap > 0 ? "set" : "disabled")
            << " (cap=" << sanitizedCap << ", queued=" << m_spawnQueue.size()
            << ", liveCount=" << m_lastLiveSweetsCount << ").\n";
        DebugTrace(oss.str());

        if (changed && m_spawnPausedDueToLiveSweets && (!capActive || m_lastLiveSweetsCount < m_liveSweetsCap))
        {
            m_spawnPausedDueToLiveSweets = false;
        }
    }

    void SweetsManager::ForceSpawnRefresh()
    {
        m_spawnTimer = 0.0f;
        m_burstTimer = 0.0f;
        m_activeBurstPlanned = 0;
        m_activeBurstInterval = kDefaultBurstIntervalSeconds;
        m_stackedInBatch = 0;
        m_batchInitialSpawnDone = false;
        m_batchCoolingDown = false;
        m_batchStackTarget = std::max(1, m_batchStackTarget);
        m_simultaneousLiveTarget = std::max(1, m_simultaneousLiveTarget);
        m_pendingTopUpAfterDrop = false;
        if (!m_spawnQueue.empty())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Clearing pending spawn queue (" << m_spawnQueue.size() << ").\n";
            DebugTrace(oss.str());
        }
        m_spawnQueue.clear();
        m_currentBurstRemaining = 0;
    }

    int SweetsManager::CountLiveSweets() const
    {
        const std::array<SweetsState, 3> liveStates = {
            SweetsState::Active,
            SweetsState::Ground,
            SweetsState::Carried,
        };

        int total = 0;
        for (const auto state : liveStates)
        {
            const auto it = m_byState.find(state);
            if (it != m_byState.end())
            {
                for (const auto& sweets : it->second)
                {
                    if (sweets && !sweets->IsPendingDestroy())
                    {
                        ++total;
                    }
                }
            }
        }

        return total;
    }

    int SweetsManager::ComputeDesiredLiveTarget(int remainingToStack) const
    {
        if (remainingToStack <= 0)
        {
            return 0;
        }

        return std::max(1, std::min(m_simultaneousLiveTarget, remainingToStack));
    }

    void SweetsManager::RunSpawnScheduler(float deltaSeconds)
    {
        const int previousLiveCount = m_lastLiveSweetsCount;
        const int liveSweets = CountLiveSweets();
        const bool liveCapActive = (m_liveSweetsCap > 0);
        const bool spawnBlockedByCap = liveCapActive && liveSweets >= m_liveSweetsCap;
        const bool liveCountChanged = (liveSweets != previousLiveCount);

        if (spawnBlockedByCap)
        {
            if (!m_spawnPausedDueToLiveSweets || liveCountChanged)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Spawn scheduler throttled by live sweets cap=" << m_liveSweetsCap
                    << "; live=" << liveSweets << " (Active/Ground/Carried) queued=" << m_spawnQueue.size()
                    << " burstRemaining=" << m_currentBurstRemaining << ".\n";
                DebugTrace(oss.str());
            }
        }
        else if (liveCountChanged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Live sweets count updated to " << liveSweets
                << " (cap=" << (liveCapActive ? std::to_string(m_liveSweetsCap) : std::string("unlimited"))
                << ", queued=" << m_spawnQueue.size() << ").\n";
            DebugTrace(oss.str());
        }

        const bool resumedFromLivePause = m_spawnPausedDueToLiveSweets && !spawnBlockedByCap;
        m_spawnPausedDueToLiveSweets = spawnBlockedByCap;
        m_lastLiveSweetsCount = liveSweets;

        if (m_phaseConfigs.empty())
        {
            return;
        }

        if (m_activePhaseConfigIndex < 0 || m_activePhaseConfigIndex >= static_cast<int>(m_phaseConfigs.size()))
        {
            OnPhaseConfigurationChanged(m_currentPhase);
        }

        if (!(deltaSeconds > 0.0f))
        {
            return;
        }
        const auto& config = m_phaseConfigs[std::clamp(m_activePhaseConfigIndex, 0, static_cast<int>(m_phaseConfigs.size()) - 1)];
        const float interval = std::max(config.spawnIntervalSeconds, kMinimumSpawnIntervalSeconds);
        m_activeBurstInterval = std::max(config.burstIntervalSeconds, kMinimumBurstIntervalSeconds);

        const int batchTarget = std::max(1, config.simultaneousSpawnCount);
        const bool targetChanged = (batchTarget != m_batchStackTarget);
        if (targetChanged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Updated batch target to " << batchTarget
                << " sweets per batch (was " << m_batchStackTarget << ") for phase " << m_activePhaseConfigIndex << ".\n";
            DebugTrace(oss.str());
        }

        const int simultaneousTarget = std::max(1, config.simultaneousLiveTarget);
        if (simultaneousTarget != m_simultaneousLiveTarget)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Updated simultaneous live target to " << simultaneousTarget
                << " (was " << m_simultaneousLiveTarget << ") for phase " << m_activePhaseConfigIndex << ".\n";
            DebugTrace(oss.str());
        }

        m_simultaneousLiveTarget = simultaneousTarget;
        m_batchStackTarget = batchTarget;

        if (!m_batchCoolingDown && m_stackedInBatch >= m_batchStackTarget)
        {
            m_batchCoolingDown = true;
            m_spawnTimer = 0.0f;
            m_batchInitialSpawnDone = false;
            m_activeBurstPlanned = 0;
            m_currentBurstRemaining = 0;
            m_pendingTopUpAfterDrop = false;
            if (!m_spawnQueue.empty())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Clearing spawn queue (" << m_spawnQueue.size()
                    << ") after reaching batch stack target=" << m_batchStackTarget << ".\n";
                DebugTrace(oss.str());
            }
            m_spawnQueue.clear();

            std::ostringstream oss;
            oss << kLogPrefix << "Batch stacked target reached; entering cooldown for "
                << std::fixed << std::setprecision(2) << interval << "s (stacked=" << m_stackedInBatch
                << ", live=" << liveSweets << ").\n";
            DebugTrace(oss.str());
        }

        if (m_batchCoolingDown)
        {
            m_spawnTimer += deltaSeconds;
            const float remainingCooldown = std::max(0.0f, interval - m_spawnTimer);
            if (remainingCooldown > 0.0f)
            {
                return;
            }

            m_batchCoolingDown = false;
            m_stackedInBatch = 0;
            m_batchInitialSpawnDone = false;
            m_spawnTimer = 0.0f;

            DebugTrace(std::string(kLogPrefix) + "Cooldown complete; starting next batch window.\n");
        }

        if (spawnBlockedByCap)
        {
            m_burstTimer = std::min(m_activeBurstInterval, m_burstTimer + deltaSeconds);
            std::ostringstream oss;
            oss << kLogPrefix << "Spawn scheduler waiting for live sweets to drop below cap="
                << m_liveSweetsCap << "; live=" << liveSweets << " queued=" << m_spawnQueue.size()
                << " stackedInBatch=" << m_stackedInBatch << '/' << m_batchStackTarget
                << " burstTimerUntilEmit=" << std::max(0.0f, m_activeBurstInterval - m_burstTimer) << "s.\n";
            DebugTrace(oss.str());
            return;
        }

        if (resumedFromLivePause)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawn scheduler resumed after cap pause. live=" << liveSweets
                << " queued=" << m_spawnQueue.size() << " burstRemaining=" << m_currentBurstRemaining
                << " stackedInBatch=" << m_stackedInBatch << '/' << m_batchStackTarget << ".\n";
            DebugTrace(oss.str());
        }

        const int pendingRequests = static_cast<int>(m_spawnQueue.size()) + m_currentBurstRemaining;
        const int projectedLive = liveSweets + pendingRequests;
        const int remainingToStack = std::max(0, m_batchStackTarget - m_stackedInBatch);
        const int desiredLiveTarget = ComputeDesiredLiveTarget(remainingToStack);

        if (desiredLiveTarget <= 0)
        {
            EmitSpawnRequests(deltaSeconds);
            m_lastLiveSweetsCount = CountLiveSweets();
            return;
        }

        const bool forceSeedNewBatch = (!m_batchInitialSpawnDone && m_stackedInBatch == 0);
        const bool liveAtTarget = liveSweets >= desiredLiveTarget;
        if (liveAtTarget && !forceSeedNewBatch)
        {
            if (!m_batchInitialSpawnDone)
            {
                m_batchInitialSpawnDone = true;
                std::ostringstream oss;
                oss << kLogPrefix << "Live sweets already meet simultaneous target; skipping burst scheduling (live="
                    << liveSweets << " target=" << desiredLiveTarget
                    << " stacked=" << m_stackedInBatch << '/' << m_batchStackTarget
                    << " queued=" << m_spawnQueue.size() << ").\n";
                DebugTrace(oss.str());
            }

            if (m_pendingTopUpAfterDrop)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Clearing pending top-up because live sweets recovered to target (live=" << liveSweets
                    << " target=" << desiredLiveTarget << ").\n";
                DebugTrace(oss.str());
                m_pendingTopUpAfterDrop = false;
            }

            EmitSpawnRequests(deltaSeconds);
            m_lastLiveSweetsCount = CountLiveSweets();
            return;
        }

        if (liveAtTarget && forceSeedNewBatch)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Forcing initial burst for new batch despite live target met (live=" << liveSweets
                << " target=" << desiredLiveTarget << " queued=" << m_spawnQueue.size()
                << " stacked=" << m_stackedInBatch << '/' << m_batchStackTarget << ").\n";
            DebugTrace(oss.str());

            if (m_pendingTopUpAfterDrop)
            {
                std::ostringstream pendingOss;
                pendingOss << kLogPrefix
                    << "Clearing pending top-up while forcing new batch seed (live target already satisfied).\n";
                DebugTrace(pendingOss.str());
                m_pendingTopUpAfterDrop = false;
            }
        }

        if (projectedLive >= desiredLiveTarget && !forceSeedNewBatch)
        {
            if (!m_batchInitialSpawnDone)
            {
                m_batchInitialSpawnDone = true;
                std::ostringstream oss;
                oss << kLogPrefix << "Projected sweets will meet target; deferring new burst (projected="
                    << projectedLive << " target=" << desiredLiveTarget
                    << " stacked=" << m_stackedInBatch << '/' << m_batchStackTarget << ").\n";
                DebugTrace(oss.str());
            }

            EmitSpawnRequests(deltaSeconds);
            m_lastLiveSweetsCount = CountLiveSweets();
            m_pendingTopUpAfterDrop = false;
            return;
        }

        const bool delayBurst = m_pendingTopUpAfterDrop;
        const int deficit = desiredLiveTarget - projectedLive;
        const int requestedCount = forceSeedNewBatch ? desiredLiveTarget : (delayBurst ? 1 : deficit);
        m_activeBurstPlanned = std::max(desiredLiveTarget, requestedCount);

        std::ostringstream oss;
        oss << kLogPrefix << "Scheduling burst to restore live target. live=" << liveSweets
            << " pending=" << pendingRequests << " target=" << desiredLiveTarget
            << " requested=" << requestedCount << " stacked=" << m_stackedInBatch << '/' << m_batchStackTarget
            << " delayedByDrop=" << std::boolalpha << delayBurst
            << " forceSeed=" << forceSeedNewBatch << ".\n";
        DebugTrace(oss.str());

        EnqueueSpawns(requestedCount);
        m_burstTimer = delayBurst ? 0.0f : m_activeBurstInterval;
        m_pendingTopUpAfterDrop = false;
        m_batchInitialSpawnDone = true;

        EmitSpawnRequests(deltaSeconds);
        m_lastLiveSweetsCount = CountLiveSweets();
    }

    void SweetsManager::EnqueueSpawns(int count)
    {
        if (count <= 0)
        {
            return;
        }

        const int burstTarget = (m_activeBurstPlanned > 0) ? m_activeBurstPlanned : count;
        const int availableQueueSlots = static_cast<int>(kMaxSpawnQueueEntries - m_spawnQueue.size());
        const int allowedCount = std::max(0, std::min({ count, burstTarget, availableQueueSlots }));
        std::ostringstream oss;
        oss << kLogPrefix << "Initializing burst (planned=" << burstTarget << ", requested=" << count
            << ", allowed=" << allowedCount << ", queueSlots=" << availableQueueSlots << ").\n";
        DebugTrace(oss.str());

        int enqueued = 0;
        for (int i = 0; i < allowedCount; ++i)
        {
            if (m_spawnQueue.size() >= kMaxSpawnQueueEntries)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Spawn queue is full (" << m_spawnQueue.size()
                    << "); dropping additional requests.\n";
                DebugTrace(oss.str());
                break;
            }

            const std::string kind = ChooseKindFromActiveConfig();
            if (kind.empty())
            {
                break;
            }

            m_spawnQueue.push_back(kind);
            ++enqueued;
        }

        if (enqueued != allowedCount)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawn enqueue shortfall: enqueued=" << enqueued
                << " expected=" << allowedCount << "; queueSize=" << m_spawnQueue.size() << ".\n";
            DebugTrace(oss.str());
        }

        m_currentBurstRemaining = enqueued;
    }

    void SweetsManager::EmitSpawnRequests(float deltaSeconds)
    {
        if (m_spawnQueue.empty())
        {
            return;
        }

        m_burstTimer += deltaSeconds;
        std::size_t spawnedCount = 0;
        const int queuedBefore = static_cast<int>(m_spawnQueue.size());
        while (!m_spawnQueue.empty() && m_burstTimer >= m_activeBurstInterval)
        {
            m_burstTimer -= m_activeBurstInterval;
            const std::string kind = m_spawnQueue.front();
            m_spawnQueue.pop_front();

            if (m_currentBurstRemaining > 0)
            {
                --m_currentBurstRemaining;
            }

            const std::vector<float3> emitOrigins = m_spawnOrigins.empty()
                ? std::vector<float3>{ NextSpawnOrigin() }
                : m_spawnOrigins;
            std::size_t successfulEmits = 0;
            for (std::size_t originIndex = 0; originIndex < emitOrigins.size(); ++originIndex)
            {
                const float3& origin = emitOrigins[originIndex];
                Transform transform = BuildSpawnTransform(origin);
                const bool spawned = static_cast<bool>(Spawn(kind, transform));
                if (spawned)
                {
                    ++spawnedCount;
                    ++successfulEmits;
                }
            }

            const int remainingInBurst = std::max(0, m_currentBurstRemaining);
            std::ostringstream oss;
            oss << kLogPrefix << "Burst emit kind='" << kind << "' remaining=" << remainingInBurst
                << '/' << m_activeBurstPlanned << " queue=" << m_spawnQueue.size()
                << " origins=" << emitOrigins.size() << " successful=" << successfulEmits
                << " nextEmitIn=" << std::fixed << std::setprecision(2)
                << std::max(0.0f, m_activeBurstInterval - m_burstTimer) << "s.\n";
            DebugTrace(oss.str());
        }

        if (m_currentBurstRemaining < 0)
        {
            m_currentBurstRemaining = 0;
        }

        if (spawnedCount > 0)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Emitted " << spawnedCount << " sweets from scheduler.\n";
            DebugTrace(oss.str());
        }

        if (queuedBefore > 0)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Batch progress: queued=" << queuedBefore
                << " remaining=" << m_currentBurstRemaining << ".\n";
            DebugTrace(oss.str());

            if (m_currentBurstRemaining <= 0 && m_spawnQueue.empty())
            {
                DebugTrace(std::string(kLogPrefix) + "Batch completed; scheduler may start a new batch after cooldown.\n");
            }
        }
    }

    std::string SweetsManager::ChooseKindFromActiveConfig()
    {
        if (m_phaseConfigs.empty())
        {
            return {};
        }

        const int clampedIndex = std::clamp(m_activePhaseConfigIndex, 0, static_cast<int>(m_phaseConfigs.size()) - 1);
        const auto& config = m_phaseConfigs[clampedIndex];
        if (config.weights.empty())
        {
            return {};
        }

        float totalWeight = 0.0f;
        for (const auto& entry : config.weights)
        {
            totalWeight += entry.weight;
        }

        if (!(totalWeight > 0.0f))
        {
            return {};
        }

        std::uniform_real_distribution<float> dist(0.0f, totalWeight);
        float roll = dist(m_random);
        std::optional<SweetsType> chosenType;
        for (const auto& entry : config.weights)
        {
            if (roll <= entry.weight)
            {
                chosenType = entry.kind;
                break;
            }
            roll -= entry.weight;
        }

        if (!chosenType)
        {
            chosenType = config.weights.back().kind;
        }

        const auto* descriptor = SweetsPrototypeManifest::Instance().Find(*chosenType);
        if (!descriptor)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Failed to resolve descriptor for sweets type '" << ToString(*chosenType)
                << "' when choosing spawn kind.\n";
            DebugTrace(oss.str());
            return {};
        }

        return NormalizeKey(descriptor->key);
    }

    void SweetsManager::OnPhaseConfigurationChanged(int newPhase)
    {
        if (m_phaseConfigs.empty())
        {
            return;
        }

        const int clamped = std::clamp(newPhase, 0, static_cast<int>(m_phaseConfigs.size()) - 1);
        if (m_activePhaseConfigIndex == clamped)
        {
            return;
        }

        m_activePhaseConfigIndex = clamped;
        ForceSpawnRefresh();
        const float interval = std::max(m_phaseConfigs[clamped].spawnIntervalSeconds, kMinimumSpawnIntervalSeconds);
        m_spawnTimer = std::max(m_spawnTimer, interval);
        m_activeBurstInterval = std::max(m_phaseConfigs[clamped].burstIntervalSeconds, kMinimumBurstIntervalSeconds);
        m_batchStackTarget = std::max(1, m_phaseConfigs[clamped].simultaneousSpawnCount);
        m_simultaneousLiveTarget = std::max(1, m_phaseConfigs[clamped].simultaneousLiveTarget);
        m_batchInitialSpawnDone = false;
        m_batchCoolingDown = false;
        m_stackedInBatch = 0;

        std::ostringstream oss;
        oss << kLogPrefix << "Phase change primed next batch with interval="
            << std::fixed << std::setprecision(2) << interval << " burstInterval=" << m_activeBurstInterval
            << " plannedSimultaneous=" << m_phaseConfigs[clamped].simultaneousSpawnCount
            << " liveTarget=" << m_simultaneousLiveTarget << ".\n";
        DebugTrace(oss.str());
        LogActiveSpawnParameters(m_phaseConfigs[clamped], clamped);
    }

    void SweetsManager::LogActiveSpawnParameters(const PhaseSpawnConfig& config, int phaseIndex) const
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[Spawn] phase=" << phaseIndex
            << " interval=" << std::fixed << std::setprecision(2) << config.spawnIntervalSeconds
            << " burstInterval=" << std::fixed << std::setprecision(2) << config.burstIntervalSeconds
            << " simultaneous=" << config.simultaneousSpawnCount
            << " liveTarget=" << config.simultaneousLiveTarget
            << " weights=";

        if (config.weights.empty())
        {
            oss << "(none)";
        }
        else
        {
            bool first = true;
            for (const auto& entry : config.weights)
            {
                oss << (first ? ' ' : ',') << ToString(entry.kind)
                    << ':' << std::fixed << std::setprecision(2) << entry.weight;
                first = false;
            }
        }

        oss << "\n";
        DebugTrace(oss.str());
    }

    void SweetsManager::ApplyDefaultPhaseConfigs()
    {
        m_phaseConfigs.clear();
        std::vector<WeightedSweetsType> baseWeights = BuildDefaultWeights();
        if (baseWeights.empty())
        {
            DebugTrace(std::string(kLogPrefix) + "Cannot build default spawn configs: no sweets registered.\n");
            return;
        }

        PhaseSpawnConfig phase0;
        phase0.spawnIntervalSeconds = kDefaultSpawnIntervalSeconds;
        phase0.burstIntervalSeconds = kDefaultBurstIntervalSeconds;
        phase0.simultaneousSpawnCount = kDefaultSimultaneousSpawnCount;
        phase0.simultaneousLiveTarget = kDefaultSimultaneousLiveTarget;
        phase0.weights = baseWeights;

        PhaseSpawnConfig phase1 = phase0;
        phase1.spawnIntervalSeconds = std::max(kMinimumSpawnIntervalSeconds, kDefaultSpawnIntervalSeconds * 0.75f);

        PhaseSpawnConfig phase2 = phase0;
        phase2.spawnIntervalSeconds = std::max(kMinimumSpawnIntervalSeconds, kDefaultSpawnIntervalSeconds * 0.5f);
        phase2.simultaneousSpawnCount = 2;
        phase2.simultaneousLiveTarget = 2;

        m_phaseConfigs.push_back(phase0);
        m_phaseConfigs.push_back(phase1);
        m_phaseConfigs.push_back(phase2);

        m_activePhaseConfigIndex = -1;
        ForceSpawnRefresh();

        std::ostringstream oss;
        oss << kLogPrefix << "Applied default spawn configs (" << m_phaseConfigs.size() << " phases).\n";
        DebugTrace(oss.str());
    }

    std::vector<WeightedSweetsType> SweetsManager::BuildDefaultWeights() const
    {
        std::vector<WeightedSweetsType> weights;
        weights.reserve(m_spawner.size());
        auto& manifest = SweetsPrototypeManifest::Instance();
        for (const auto& entry : m_spawner)
        {
            float spawnWeight = 1.0f;
            auto presetIt = m_factoryPresets.find(entry.first);
            if (presetIt != m_factoryPresets.end())
            {
                spawnWeight = presetIt->second.spawnWeight;
            }

            if (spawnWeight <= 0.0f)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "Skipping default spawn weight for kind '" << entry.first
                    << "' because spawnWeight=" << spawnWeight << "<= 0.\n";
                DebugTrace(warn.str());
                continue;
            }

            const auto resolvedType = manifest.Resolve(entry.first);
            if (!resolvedType)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "Skipping default spawn weight for unknown sweets type key '" << entry.first
                    << "'.\n";
                DebugTrace(warn.str());
                continue;
            }

            weights.push_back({ *resolvedType, spawnWeight });
        }
        return weights;
    }

    float3 SweetsManager::NextSpawnOrigin()
    {
        if (m_spawnOrigins.empty())
        {
            if (!m_warnedMissingSpawnOrigins)
            {
                DebugTrace(std::string(kLogPrefix) + "Spawn origin list is empty; using world origin.\n");
                m_warnedMissingSpawnOrigins = true;
            }
            return float3{ 0.0f, 0.0f, 0.0f };
        }

        std::uniform_int_distribution<std::size_t> dist(0, m_spawnOrigins.size() - 1);
        const std::size_t chosenIndex = dist(m_random);
        const float3 origin = m_spawnOrigins[chosenIndex];

        std::ostringstream oss;
        oss << kLogPrefix << "Selected spawn origin index=" << chosenIndex
            << " position=(" << origin.x << ',' << origin.y << ',' << origin.z
            << ") of " << m_spawnOrigins.size() << " configured origins.\n";
        DebugTrace(oss.str());

        return origin;
    }

    Transform SweetsManager::BuildSpawnTransform(const float3& origin) const
    {
        Transform transform{};
        transform.position = origin;
        transform.position.y += m_spawnHeightOffset;
        transform.rotation = float3{ 0.0f, 0.0f, 0.0f };
        transform.scale = float3{ 1.0f, 1.0f, 1.0f };

        if (m_enableRandomRotation)
        {
            const auto sampleRotation = [&](float minDeg, float maxDeg)
            {
                const float lo = std::min(minDeg, maxDeg);
                const float hi = std::max(minDeg, maxDeg);
                if (lo == hi)
                {
                    return lo;
                }

                std::uniform_real_distribution<float> dist(lo, hi);
                return dist(m_random);
            };

            const float sampledPitch = sampleRotation(m_randomRotationMinDegrees.x, m_randomRotationMaxDegrees.x);
            const float sampledYaw = sampleRotation(m_randomRotationMinDegrees.y, m_randomRotationMaxDegrees.y);
            const float sampledRoll = sampleRotation(m_randomRotationMinDegrees.z, m_randomRotationMaxDegrees.z);
            transform.rotation = float3{ sampledPitch, sampledYaw, sampledRoll };

            std::ostringstream oss;
            oss << kLogPrefix << "Spawn rotation sampled pitch/yaw/roll(deg)="
                << sampledPitch << ',' << sampledYaw << ',' << sampledRoll
                << " origin=" << origin.x << ',' << origin.y << ',' << origin.z << "\n";
            DebugTrace(oss.str());
        }

        return transform;
    }
}
