#include "SweetsManager.h"

#include <algorithm>
#include <array>
#include <cassert>
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
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <box2d/box2d.h>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif
#include <d3d11.h>
#include <vector>
#include "DirectX.h"
#include "AABB.h"
#include "ColliderConfigLoader.h"
#include "Collision2D.h"
#include "Collision/ConvexHullBuilder.h"
#include "Core/UpdateScheduler.h"
#include "MeshBuffer.h"
#include "ModelCache.h"
#include "ModelPivot.h"
#include "BoxColliderComponent.h"
#include "Sweets/BaseSweets.h"
#include "SweetsPrototypeManifest.h"
#include "Systems/DrawSystem.h"
#include "Systems/PhysicsMachine.h"
#include "Entities/PhysicalEntity.h"
#include "Physics/ShapeConstants.h"
#include "Diagnostics/Logger.h"
#include <DirectXMath.h>

namespace SweetsSpace
{
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
}

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
    constexpr float kDefaultSpawnIntervalSeconds = 2.5f;
    constexpr float kDefaultBurstIntervalSeconds = 0.5f;
    constexpr int kDefaultSimultaneousSpawnCount = 1;
    constexpr int kDefaultSimultaneousLiveTarget = kDefaultSimultaneousSpawnCount;
    constexpr int kDefaultLiveSweetsCap = 24;
    constexpr float kMinimumBurstIntervalSeconds = 0.05f;
    constexpr float kMinimumSpawnIntervalSeconds = 0.1f;
    constexpr bool kAutoIncludeMissingWeightKinds = true;
    constexpr std::size_t kMaxSpawnQueueEntries = 16;
    constexpr float kDefaultSpawnHeightOffset = 8.0f;
    constexpr const char* kColliderConfigPath = "Assets/Config/colliders_sweets.cfg";
    constexpr const char* kFactoryPresetCsvPath = "Assets/Config/sweets_factory.csv";
    constexpr const char* kPlaceholderModelPath = "Assets/Model/SweetsPrototype/FairyPlaceholder.obj";
    constexpr float kPlatformWidth = 12.0f;
    constexpr float kHorizontalOutOfBoundsMargin = 3.5f;
    constexpr float kHorizontalBoundsEpsilon = 1e-4f;
    constexpr float kDefaultGeneratedThicknessPadding = 1.1f;
    constexpr float kScaleReferenceMeters = 1.0f;
    constexpr float kMinNormalizationScale = 0.02f;
    constexpr float kMaxNormalizationScale = 50.0f;
    constexpr float kMinFinalScale = 0.05f;
    constexpr float kMaxFinalScale = 3.0f;
    constexpr float kMinimumColliderThickness = std::max(Physics::kMinimumShapeDimensionMeters, 1e-3f);
    constexpr float kMinimumBerryColliderRadius = 0.15f;
    constexpr float kMinimumBlueberryColliderRadius = 0.2f;

    using ::SweetsSpace::SweetsPrototypeManifest;

    std::string ResolveManifestKeyAlias(const std::string& key)
    {
        const auto& manifest = SweetsPrototypeManifest::Instance();
        if (const auto resolvedType = manifest.Resolve(key))
        {
            if (const auto* descriptor = manifest.Find(*resolvedType))
            {
                return descriptor->key;
            }
        }

        return key;
    }

    using ::Physics::Box2DShapeConfig;
    using ::SweetsSpace::ColliderPreset;
    using ::SweetsSpace::ColliderShape;
    using ::SweetsSpace::SweetsPrototypeManifest;
    using ::SweetsSpace::SweetsType;
    using ::SweetsSpace::ToString;
    using ::SweetsSpace::SweetsTypeHash;

    bool HasMeaningfulDifference(float previous, float current, float epsilon = kHorizontalBoundsEpsilon);
    bool HasMeaningfulDifference(const float3& a, const float3& b, float epsilon = kHorizontalBoundsEpsilon);

    void ReportInvalidBox2DConfig(SweetsType type, const std::string& reason)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[ConfigError] type='" << ToString(type) << "' " << reason << "\n";
        Diagnostics::LogInfo(oss.str());

        assert(false && "Invalid Box2DShapeConfig for sweets type");
    }

    float ComputeNormalizationScale(const Render::Model& model, const char* context)
    {
        if (!model.HasLocalBounds())
        {
            return 1.0f;
        }

        const AABB bounds = model.GetLocalBounds();
        const float3 extents = bounds.Extents();
        const float largestExtent = std::max({ extents.x, extents.y, extents.z });
        if (!std::isfinite(largestExtent) || largestExtent <= 0.0f)
        {
            return 1.0f;
        }

        const float rawScale = kScaleReferenceMeters / largestExtent;
        if (!std::isfinite(rawScale) || rawScale <= 0.0f)
        {
            return 1.0f;
        }

        const float clamped = std::clamp(rawScale, kMinNormalizationScale, kMaxNormalizationScale);
        if (std::fabs(clamped - 1.0f) > 0.01f)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[ScaleNorm] context='" << context << "' の extents=("
                << extents.x << ',' << extents.y << ',' << extents.z << ") raw=" << rawScale
                << " clamped=" << clamped << " referenceMeters=" << kScaleReferenceMeters << "\n";
            DebugTrace(oss.str());
        }

        return clamped;
    }

    float3 ClampScaleToGameplayRange(const float3& scale, const char* context)
    {
        float3 clamped
        {
            std::clamp(scale.x, kMinFinalScale, kMaxFinalScale),
            std::clamp(scale.y, kMinFinalScale, kMaxFinalScale),
            std::clamp(scale.z, kMinFinalScale, kMaxFinalScale),
        };

        if (HasMeaningfulDifference(scale, clamped, 1e-3f))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[ScaleNorm] context='" << context << "' のスケールを ("
                << scale.x << ',' << scale.y << ',' << scale.z << ") から ("
                << clamped.x << ',' << clamped.y << ',' << clamped.z << ") に丸めたよ bounds="
                << kMinFinalScale << "-" << kMaxFinalScale << "\n";
            DebugTrace(oss.str());
        }

        return clamped;
    }

    std::optional<AABB> ComputeBounds(const std::vector<MeshDX::Vertex>& vertices)
    {
        if (vertices.empty())
        {
            return std::nullopt;
        }

        float3 min
        {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
        };
        float3 max
        {
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
        };

        for (const auto& vertex : vertices)
        {
            min.x = std::min(min.x, vertex.pos.x);
            min.y = std::min(min.y, vertex.pos.y);
            min.z = std::min(min.z, vertex.pos.z);

            max.x = std::max(max.x, vertex.pos.x);
            max.y = std::max(max.y, vertex.pos.y);
            max.z = std::max(max.z, vertex.pos.z);
        }

        return AABB(max, min);
    }

    std::shared_ptr<Render::Model> CreateProceduralFairyModel()
    {
        static std::weak_ptr<Render::Model> cached;
        if (auto reused = cached.lock())
        {
            DebugTrace(std::string(kLogPrefix) + "キャッシュしてたフェアリーをそのまま使うよ\n");
            return reused;
        }

        ID3D11Device* device = GetDevice();
        if (!device)
        {
            DebugTrace(std::string(kLogPrefix) + "プロシージャルフェアリー作れないや、デバイスが見つからないっぽい\n");
            return nullptr;
        }

        constexpr float halfWidth = 0.4f;
        constexpr float halfDepth = 0.12f;
        constexpr float halfHeight = 0.7f;

        const DirectX::XMFLOAT4 baseColor{ 0.45f, 0.65f, 1.0f, 1.0f };
        const DirectX::XMFLOAT4 wingColor{ 0.6f, 0.8f, 1.0f, 1.0f };

        std::vector<MeshDX::Vertex> vertices;
        vertices.reserve(24);

        auto appendQuad = [&](auto positions, DirectX::XMFLOAT3 normal,
            const DirectX::XMFLOAT4& colFront,
            const DirectX::XMFLOAT4& colBack)
            {
                const std::array<DirectX::XMFLOAT2, 4> uvs{
                    DirectX::XMFLOAT2(0,1), DirectX::XMFLOAT2(1,1),
                    DirectX::XMFLOAT2(1,0), DirectX::XMFLOAT2(0,0)
                };

                auto addV = [&](auto pos, auto nrm, auto uv, auto col)
                    {
                        MeshDX::Vertex v{};
                        v.pos = pos;
                        v.normal = nrm;
                        v.uv = uv;
                        v.color = col;
                        for (float& w : v.weight) w = 0;
                        for (unsigned& i : v.index) i = 0;
                        vertices.push_back(v);
                    };

                DirectX::XMFLOAT3 n1 = normal;
                DirectX::XMFLOAT3 n2{ -normal.x, -normal.y, -normal.z };

                for (int i = 0; i < 4; i++) addV(positions[i], n1, uvs[i], colFront);
                for (int i = 0; i < 4; i++) addV(positions[i], n2, uvs[i], colBack);
            };

        const std::array<DirectX::XMFLOAT3, 4> quadZ{
            DirectX::XMFLOAT3(-halfWidth,-halfHeight,0),
            DirectX::XMFLOAT3(halfWidth,-halfHeight,0),
            DirectX::XMFLOAT3(halfWidth, halfHeight,0),
            DirectX::XMFLOAT3(-halfWidth, halfHeight,0)
        };
        appendQuad(quadZ, DirectX::XMFLOAT3(0, 0, 1), wingColor, baseColor);

        const std::array<DirectX::XMFLOAT3, 4> quadX{
            DirectX::XMFLOAT3(0,-halfHeight,-halfDepth),
            DirectX::XMFLOAT3(0,-halfHeight, halfDepth),
            DirectX::XMFLOAT3(0, halfHeight, halfDepth),
            DirectX::XMFLOAT3(0, halfHeight,-halfDepth)
        };
        appendQuad(quadX, DirectX::XMFLOAT3(1, 0, 0), baseColor, wingColor);

        std::vector<unsigned long> indices;
        indices.reserve(36);

        for (int q = 0; q < 2; q++)
        {
            unsigned long base = q * 8;
            unsigned long f[6] = { base, base + 1, base + 2, base, base + 2, base + 3 };
            unsigned long b[6] = { base + 4, base + 6, base + 5, base + 4, base + 7, base + 6 };
            indices.insert(indices.end(), f, f + 6);
            indices.insert(indices.end(), b, b + 6);
        }

        MeshDX::Description d{};
        d.pVtx = vertices.data();
        d.vtxSize = sizeof(MeshDX::Vertex);
        d.vtxCount = (UINT)vertices.size();
        d.pIdx = indices.data();
        d.idxSize = sizeof(unsigned long);
        d.idxCount = (UINT)indices.size();
        d.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        auto mesh = std::make_shared<MeshDX>(d, 0);
        if (!mesh->GetVertexBuffer().IsValid() || !mesh->HasIndexBuffer())
            return nullptr;

        auto mat = std::make_shared<MaterialDX>(
            DirectX::XMFLOAT4(0.5f, 0.75f, 1, 1),
            DirectX::XMFLOAT4(0.2f, 0.35f, 0.55f, 1),
            DirectX::XMFLOAT4(0.1f, 0.1f, 0.2f, 8),
            nullptr);

        const std::optional<AABB> bounds = ComputeBounds(vertices);

        auto model = std::make_shared<ModelDX>(
            std::vector<std::shared_ptr<MeshDX>>{mesh},
            std::vector<std::shared_ptr<MaterialDX>>{mat});

        if (bounds)
        {
            model->SetLocalBounds(*bounds);
        }

        cached = model;
        return model;
    }

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
        float2 centroidOffset{ 0.0f, 0.0f };
        float rawThickness{ 0.0f };
        float thickness{ 1.0f };
        float paddingFactor{ 1.0f };
    };

    struct HullExtents
    {
        float2 min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
        float2 max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
        bool valid{ false };
    };

    float SanitizeThicknessPadding(float value)
    {
        if (!std::isfinite(value))
        {
            return 1.0f;
        }

        return std::max(value, 1.0f);
    }

    float SanitizeMinThickness(float value)
    {
        if (!std::isfinite(value) || value <= 0.0f)
        {
            return kMinimumColliderThickness;
        }

        return std::max(value, kMinimumColliderThickness);
    }

    float ApplyThicknessEpsilon(float thickness, float minThickness)
    {
        const float sanitizedMin = SanitizeMinThickness(minThickness);
        if (!std::isfinite(thickness))
        {
            return sanitizedMin;
        }

        return std::max(thickness, sanitizedMin);
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

    void UpdateBatchUsage(::SweetsSpace::DeleteBatchContext* batch, ::SweetsSpace::UseOutcome outcome)
    {
        if (!batch)
        {
            return;
        }

        if (batch->usesRemaining > 0)
        {
            --batch->usesRemaining;
        }

        if (outcome == ::SweetsSpace::UseOutcome::Hit)
        {
            ++batch->usesRemaining;
        }
    }

    float ComputeSpeedSq(const Entities::PhysicalEntity& entity)
    {
        const auto v = entity.GetLinearVelocity();
        return (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
    }

    bool HasMeaningfulDifference(float previous, float current, float epsilon)
    {
        if (!std::isfinite(previous) || !std::isfinite(current))
        {
            return true;
        }

        return std::fabs(previous - current) > epsilon;
    }

    bool HasMeaningfulDifference(const float3& a, const float3& b, float epsilon)
    {
        return HasMeaningfulDifference(a.x, b.x, epsilon)
            || HasMeaningfulDifference(a.y, b.y, epsilon)
            || HasMeaningfulDifference(a.z, b.z, epsilon);
    }

    float ComputeVolumeScale(const float3& scale)
    {
        if (!(std::isfinite(scale.x) && std::isfinite(scale.y) && std::isfinite(scale.z)))
        {
            return 1.0f;
        }

        return std::max(0.0f, scale.x * scale.y * scale.z);
    }

    float ComputeAreaScaleXY(const float3& scale, Physics::Box2DShapeConfig::Type shape)
    {
        if (!(std::isfinite(scale.x) && std::isfinite(scale.y)))
        {
            return 1.0f;
        }

        switch (shape)
        {
        case Physics::Box2DShapeConfig::Type::Circle:
        {
            const float axisScale = std::max(scale.x, scale.y);
            return axisScale * axisScale;
        }
        default:
            return scale.x * scale.y;
        }
    }

    float ComputeDensityScale(const float3& scale, Physics::Box2DShapeConfig::Type shape)
    {
        const float areaScale = std::max(ComputeAreaScaleXY(scale, shape), std::numeric_limits<float>::min());
        const float volumeScale = ComputeVolumeScale(scale);
        return (areaScale > 0.0f) ? (volumeScale / areaScale) : 1.0f;
    }

    Box2DShapeConfig BuildBaseShapeConfig(SweetsType type)
    {
        Box2DShapeConfig config{};
        config.allowRotation = true;
        config.angularDamping = 0.2f;

        switch (type)
        {
        case SweetsType::Cake:
            config.type = Box2DShapeConfig::Type::Box;
            config.allowRotation = false;
            config.angularDamping = 6.0f;
            config.density = 1.2f;
            config.friction = 0.72f;
            config.restitution = 0.03f;
            config.vertices = {
                b2Vec2{ -0.5f, -0.5f },
                b2Vec2{ 0.5f, -0.5f },
                b2Vec2{ 0.5f, 0.5f },
                b2Vec2{ -0.5f, 0.5f },
            };
            break;
        case SweetsType::Pudding:
            config.type = Box2DShapeConfig::Type::Polygon;
            config.vertices = {
                b2Vec2{ -0.5f, -0.45f },
                b2Vec2{ 0.5f, -0.45f },
                b2Vec2{ 0.0f, 0.55f },
            };
            config.allowRotation = false;
            config.angularDamping = 0.65f;
            config.density = 0.95f;
            config.friction = 0.6f;
            config.restitution = 0.06f;
            break;
        case SweetsType::Sherbet:
            config.type = Box2DShapeConfig::Type::Circle;
            config.radius = 0.5f;
            config.angularDamping = 0.75f;
            config.density = 0.75f;
            config.friction = 0.35f;
            config.restitution = 0.28f;
            break;
        case SweetsType::Strawberry:
            config.type = Box2DShapeConfig::Type::Circle;
            config.radius = 0.45f;
            config.angularDamping = 2.5f;
            config.density = 0.85f;
            config.friction = 0.5f;
            config.restitution = 0.12f;
            break;
        case SweetsType::Blueberry:
            config.type = Box2DShapeConfig::Type::Circle;
            config.radius = 0.35f;
            config.angularDamping = 2.25f;
            config.density = 0.7f;
            config.friction = 0.42f;
            config.restitution = 0.18f;
            break;
        case SweetsType::Pineapple:
            config.type = Box2DShapeConfig::Type::Box;
            config.angularDamping = 0.5f;
            config.density = 1.05f;
            config.friction = 0.68f;
            config.restitution = 0.04f;
            config.vertices = {
                b2Vec2{ -0.32f, -0.6f },
                b2Vec2{ 0.32f, -0.6f },
                b2Vec2{ 0.38f, 0.15f },
                b2Vec2{ 0.0f, 0.65f },
                b2Vec2{ -0.38f, 0.15f },
            };
            break;
        case SweetsType::Banana:
            config.type = Box2DShapeConfig::Type::Polygon;
            config.vertices = {
                b2Vec2{ -0.6f, -0.08f },
                b2Vec2{ 0.6f, 0.08f },
                b2Vec2{ 0.55f, 0.24f },
                b2Vec2{ -0.45f, 0.24f },
            };
            config.angularDamping = 0.65f;
            config.density = 0.9f;
            config.friction = 0.55f;
            config.restitution = 0.08f;
            break;
        case SweetsType::Fairy:
            config.type = Box2DShapeConfig::Type::Circle;
            config.radius = 0.35f;
            config.allowRotation = false;
            config.angularDamping = 2.5f;
            config.density = 0.55f;
            config.friction = 0.4f;
            config.restitution = 0.06f;
            break;
        }

        if (config.type == Box2DShapeConfig::Type::None)
        {
            ReportInvalidBox2DConfig(type, "is missing an explicit collider shape definition");
        }

        if (!config.allowRotation)
        {
            config.fixedRotation = true;
            config.angularDamping = std::max(config.angularDamping, 6.0f);
        }

        return config;
    }

    bool ValidateBaseShapeDefinition(SweetsType type)
    {
        const auto config = BuildBaseShapeConfig(type);

        if (config.type == Box2DShapeConfig::Type::None)
        {
            return false;
        }

        switch (config.type)
        {
        case Box2DShapeConfig::Type::Box:
        case Box2DShapeConfig::Type::Polygon:
            if (config.vertices.size() < 3)
            {
                ReportInvalidBox2DConfig(type, "requires explicit vertices for its collider definition");
                return false;
            }
            break;
        case Box2DShapeConfig::Type::Circle:
            if (!(config.radius > 0.0f))
            {
                ReportInvalidBox2DConfig(type, "requires a positive radius for its collider definition");
                return false;
            }
            break;
        default:
            break;
        }

        return true;
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

    std::vector<b2Vec2> BuildCircleApproximation(float radius, int segments = 14)
    {
        std::vector<b2Vec2> vertices;
        if (!(radius > 0.0f) || segments < 3)
        {
            return vertices;
        }

        vertices.reserve(static_cast<std::size_t>(segments));
        constexpr float kTwoPi = 6.28318530718f;
        const float step = kTwoPi / static_cast<float>(segments);
        for (int i = 0; i < segments; ++i)
        {
            const float angle = step * static_cast<float>(i);
            vertices.push_back(b2Vec2{ radius * std::cos(angle), radius * std::sin(angle) });
        }

        return vertices;
    }

}

namespace SweetsSpace
{

    std::string NormalizeKey(const std::string& key);
    MeshHullSummary AnalyzeModelGeometry(const std::string& path, float thicknessPadding, float minThickness);


    SweetsManager::ColliderBuildResult SweetsManager::BuildColliderForSweets(const std::string& kind,
        SweetsType type,
        const Transform& transform,
        const std::string& modelPath,
        const std::shared_ptr<Render::Model>& model)
    {
        ColliderBuildResult result{};
        result.shapeConfig = BuildBaseShapeConfig(type);
        static bool s_loggedPolygonFallback = false;

        (void)model;

        const std::string resolvedKind = ResolveKindKey(kind);
        const std::string normalizedKind = NormalizeKey(resolvedKind);
        const auto presetIt = m_factoryPresets.find(normalizedKind);
        const ColliderPreset* colliderPreset = presetIt != m_factoryPresets.end()
            ? &presetIt->second.colliderPreset
            : nullptr;
        const auto definitionIt = FindColliderDefinition(resolvedKind);
        const bool hasConfigDefinition = definitionIt != m_colliderDefinitions.end()
            && !IsColliderDefinitionEmpty(definitionIt->second);

        const float2 colliderScale{ transform.scale.x, transform.scale.y };
        const float uniformColliderScale = std::max(std::abs(colliderScale.x), std::abs(colliderScale.y));

        auto acquireGenerated = [&]() -> std::optional<GeneratedCollider>
        {
            if (auto cached = m_generatedColliders.find(normalizedKind); cached != m_generatedColliders.end())
            {
                return cached->second;
            }

            if (modelPath.empty())
            {
                return std::nullopt;
            }

            const MeshHullSummary summary = AnalyzeModelGeometry(modelPath, m_generatedColliderThicknessPadding, m_generatedColliderMinThickness);
            if (summary.hull.empty())
            {
                return std::nullopt;
            }

            GeneratedCollider generated{};
            generated.hull = summary.hull;
            generated.centroidOffset = summary.centroidOffset;
            generated.thickness = std::clamp(summary.thickness, 0.0f, Physics::kMinimumShapeDimensionMeters);
            m_generatedColliders[normalizedKind] = generated;
            return generated;
        };

        std::optional<GeneratedCollider> generated;
        if (m_forceGeneratedColliders || !m_colliderConfigOverridesEnabled || !hasConfigDefinition)
        {
            generated = acquireGenerated();
        }

        const bool hasGenerated = generated && !generated->hull.empty();
        bool appliedConfigDefinition = false;

        auto applyConfigDefinition = [&](const Collision::ColliderDefinition& definition, const char* reason) -> bool
        {
            bool applied = false;
            const float clampedThickness = std::clamp(definition.thickness, 0.0f, Physics::kMinimumShapeDimensionMeters);
            switch (definition.type)
            {
            case Collision::ColliderKind::Circle:
            {
                const float effectiveRadius = std::max(0.0f, ((definition.radius * definition.scale) + definition.margin) * uniformColliderScale);
                result.shapeConfig.type = Box2DShapeConfig::Type::Circle;
                result.shapeConfig.radius = effectiveRadius;
                result.shapeConfig.polygonRadius = std::max(0.0f, clampedThickness * 0.5f * uniformColliderScale);
                result.thickness = clampedThickness * uniformColliderScale;
                result.source = ColliderSource::Config;
                applied = true;
                break;
            }
            case Collision::ColliderKind::Hull:
            {
                result.meshPolygons.clear();
                result.meshBoundary.clear();

                std::vector<float2> scaledPoints;
                scaledPoints.reserve(definition.points.size());
                for (const auto& pt : definition.points)
                {
                    scaledPoints.push_back(float2{ pt.x * colliderScale.x, pt.y * colliderScale.y });
                }

                const float2 scaledOffset{ definition.offset.x * colliderScale.x, definition.offset.y * colliderScale.y };
                const float scaledMargin = definition.margin * uniformColliderScale;

                result.shapeConfig.vertices = BuildHullVertices(scaledPoints, scaledOffset, definition.scale, scaledMargin);
                if (result.shapeConfig.vertices.size() >= 3)
                {
                    result.shapeConfig.type = Box2DShapeConfig::Type::Polygon;
                    result.shapeConfig.polygonRadius = std::max(0.0f, clampedThickness * 0.5f * uniformColliderScale);
                    result.thickness = clampedThickness * uniformColliderScale;
                    result.source = ColliderSource::Config;
                    applied = true;
                }
                break;
            }
            case Collision::ColliderKind::Compound:
            {
                result.meshPolygons.clear();
                float maxThickness = 0.0f;
                for (const auto& child : definition.children)
                {
                    const float clampedChildThickness = std::clamp(child.thickness, 0.0f, Physics::kMinimumShapeDimensionMeters);
                    switch (child.type)
                    {
                    case Collision::ColliderKind::Circle:
                    {
                        const float radius = std::max(0.0f, ((child.radius * child.scale) + child.margin) * uniformColliderScale);
                        auto circlePoly = BuildCircleApproximation(radius);
                        if (circlePoly.size() >= 3)
                        {
                            result.meshPolygons.push_back(std::move(circlePoly));
                            applied = true;
                        }
                        break;
                    }
                    case Collision::ColliderKind::Hull:
                    {
                        std::vector<float2> scaledPoints;
                        scaledPoints.reserve(child.points.size());
                        for (const auto& pt : child.points)
                        {
                            scaledPoints.push_back(float2{ pt.x * colliderScale.x, pt.y * colliderScale.y });
                        }

                        const float2 scaledOffset{ child.offset.x * colliderScale.x, child.offset.y * colliderScale.y };
                        const float scaledMargin = child.margin * uniformColliderScale;

                        auto hullVertices = BuildHullVertices(scaledPoints, scaledOffset, child.scale, scaledMargin);
                        if (hullVertices.size() >= 3)
                        {
                            result.meshPolygons.push_back(std::move(hullVertices));
                            applied = true;
                        }
                        break;
                    }
                    default:
                        break;
                    }

                    maxThickness = std::max(maxThickness, clampedChildThickness * uniformColliderScale);
                }

                if (!result.meshPolygons.empty())
                {
                    result.shapeConfig.type = Box2DShapeConfig::Type::Polygon;
                    result.shapeConfig.polygonRadius = std::max(0.0f, maxThickness * 0.5f);
                    result.thickness = maxThickness;
                    result.source = ColliderSource::Config;
                }
                break;
            }
            default:
                break;
            }

            if (applied)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "コライダー設定を使ったよ reason='" << (reason ? reason : "")
                    << "' kind='" << resolvedKind << "' scale= ("
                    << colliderScale.x << ',' << colliderScale.y << ") uniform=" << uniformColliderScale << "\n";
                DebugTrace(oss.str());
            }

            appliedConfigDefinition = appliedConfigDefinition || applied;

            return applied;
        };

        auto applyHull = [&](const std::vector<float2>& hull, const float2& centroidOffset, float thickness)
        {
            const float clampedThickness = std::clamp(thickness, 0.0f, Physics::kMinimumShapeDimensionMeters);
            std::vector<float2> scaledHull;
            scaledHull.reserve(hull.size());
            for (const auto& pt : hull)
            {
                scaledHull.push_back(float2{ pt.x * colliderScale.x, pt.y * colliderScale.y });
            }

            const float2 scaledOffset{ centroidOffset.x * colliderScale.x, centroidOffset.y * colliderScale.y };

            std::vector<b2Vec2> hullVertices = BuildHullVertices(scaledHull, scaledOffset, 1.0f, 0.0f);
            if (hullVertices.size() >= 3)
            {
                result.shapeConfig.type = Box2DShapeConfig::Type::Polygon;
                result.shapeConfig.vertices = hullVertices;
                result.meshPolygons.clear();
                result.meshPolygons.push_back(hullVertices);
                result.shapeConfig.polygonRadius = std::max(0.0f, clampedThickness * 0.5f * uniformColliderScale);
                result.thickness = clampedThickness * uniformColliderScale;
                result.centroidOffset = scaledOffset;
            }
        };

        if (m_forceGeneratedColliders)
        {
            if (hasGenerated)
            {
                applyHull(generated->hull, generated->centroidOffset, generated->thickness);
                result.source = ColliderSource::Generated;
            }
        }
        else if (m_colliderConfigOverridesEnabled && hasConfigDefinition)
        {
            const bool preferGeneratedOverride = (type == SweetsType::Blueberry) && hasGenerated;
            bool appliedGeneratedOverride = false;
            if (preferGeneratedOverride)
            {
                applyHull(generated->hull, generated->centroidOffset, generated->thickness);
                if (result.shapeConfig.type != Box2DShapeConfig::Type::None)
                {
                    result.source = ColliderSource::Generated;
                    appliedGeneratedOverride = true;

                    std::ostringstream oss;
                    oss << kLogPrefix << "オーバーライド中だけどブルーベリーはメッシュ凸包を使うよ kind='"
                        << resolvedKind << "' verts=" << result.shapeConfig.vertices.size() << "\n";
                    DebugTrace(oss.str());
                }
            }

            if (!appliedGeneratedOverride)
            {
                const auto& definition = definitionIt->second;
                const bool appliedConfig = applyConfigDefinition(definition, "override-enabled");
                if (!appliedConfig && hasGenerated)
                {
                    applyHull(generated->hull, generated->centroidOffset, generated->thickness);
                    result.source = ColliderSource::Generated;
                }
            }
        }
        else if (hasGenerated)
        {
            applyHull(generated->hull, generated->centroidOffset, generated->thickness);
            result.source = ColliderSource::Generated;
        }
        else if (hasConfigDefinition)
        {
            const auto& definition = definitionIt->second;
            applyConfigDefinition(definition, "fallback-config");
        }
        else if (!hasGenerated)
        {
            generated = acquireGenerated();
            if (generated && !generated->hull.empty())
            {
                applyHull(generated->hull, generated->centroidOffset, generated->thickness);
                result.source = ColliderSource::Generated;
            }
        }

        const auto computePolygonDimensions = [](const std::vector<b2Vec2>& vertices) -> std::optional<std::pair<float, float>>
        {
            if (vertices.size() < 3)
            {
                return std::nullopt;
            }

            b2Vec2 min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
            b2Vec2 max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
            for (const auto& vertex : vertices)
            {
                min.x = std::min(min.x, vertex.x);
                min.y = std::min(min.y, vertex.y);
                max.x = std::max(max.x, vertex.x);
                max.y = std::max(max.y, vertex.y);
            }

            return std::make_optional(std::make_pair(max.x - min.x, max.y - min.y));
        };

        const auto buildCapsuleVertices = [](float width, float height) -> std::vector<b2Vec2>
        {
            std::vector<b2Vec2> vertices;
            if (!(width > 0.0f) || !(height > 0.0f))
            {
                return vertices;
            }

            const float halfWidth = width * 0.5f;
            const float halfHeight = height * 0.5f;
            const bool horizontal = halfWidth >= halfHeight;
            const float radius = horizontal ? halfHeight : halfWidth;
            const float spine = (horizontal ? halfWidth : halfHeight) - radius;
            if (!(radius > 0.0f))
            {
                return vertices;
            }

            constexpr int kArcSegments = 6;
            const float startRight = -B2_PI * 0.5f;
            const float endRight = B2_PI * 0.5f;
            const float startLeft = B2_PI * 0.5f;
            const float endLeft = B2_PI * 1.5f;

            const float offsetX = horizontal ? spine : 0.0f;
            const float offsetY = horizontal ? 0.0f : spine;

            auto appendArc = [&](float cx, float cy, float start, float end, bool skipFirst)
            {
                const float step = (end - start) / static_cast<float>(kArcSegments);
                for (int i = 0; i <= kArcSegments; ++i)
                {
                    if (skipFirst && i == 0)
                    {
                        continue;
                    }

                    const float angle = start + (step * static_cast<float>(i));
                    vertices.push_back(b2Vec2{ cx + (radius * std::cos(angle)), cy + (radius * std::sin(angle)) });
                }
            };

            appendArc(offsetX, offsetY, startRight, endRight, false);
            appendArc(-offsetX, -offsetY, startLeft, endLeft, true);

            return vertices;
        };

        if (colliderPreset && !appliedConfigDefinition)
        {
            std::optional<std::pair<float, float>> dimensions;
            if (!result.shapeConfig.vertices.empty())
            {
                dimensions = computePolygonDimensions(result.shapeConfig.vertices);
            }
            else if (!result.meshPolygons.empty())
            {
                dimensions = computePolygonDimensions(result.meshPolygons.front());
            }
            else if (result.shapeConfig.type == Box2DShapeConfig::Type::Circle && (result.shapeConfig.radius > 0.0f))
            {
                const float diameter = result.shapeConfig.radius * 2.0f;
                dimensions = std::make_optional(std::make_pair(diameter, diameter));
            }

            const float width = std::max(dimensions ? dimensions->first : Physics::kMinimumShapeDimensionMeters, Physics::kMinimumShapeDimensionMeters);
            const float height = std::max(dimensions ? dimensions->second : Physics::kMinimumShapeDimensionMeters, Physics::kMinimumShapeDimensionMeters);

            const float paddedWidth = width + (colliderPreset->padding.x * 2.0f * uniformColliderScale);
            const float paddedHeight = height + (colliderPreset->padding.y * 2.0f * uniformColliderScale);
            const float thickness = std::max({ result.thickness, colliderPreset->padding.z * uniformColliderScale, Physics::kMinimumShapeDimensionMeters });

            std::vector<b2Vec2> presetVertices;
            switch (colliderPreset->shape)
            {
            case ColliderShape::Capsule:
                presetVertices = buildCapsuleVertices(paddedWidth, paddedHeight);
                break;
            case ColliderShape::Box:
            default:
                presetVertices = {
                    b2Vec2{ -paddedWidth * 0.5f, -paddedHeight * 0.5f },
                    b2Vec2{ paddedWidth * 0.5f, -paddedHeight * 0.5f },
                    b2Vec2{ paddedWidth * 0.5f, paddedHeight * 0.5f },
                    b2Vec2{ -paddedWidth * 0.5f, paddedHeight * 0.5f },
                };
                break;
            }

            if (presetVertices.size() >= 3)
            {
                result.shapeConfig.type = Box2DShapeConfig::Type::Polygon;
                result.shapeConfig.vertices = presetVertices;
                result.meshPolygons.clear();
                result.meshPolygons.push_back(presetVertices);
                result.meshBoundary = presetVertices;
                result.shapeConfig.polygonRadius = std::max(result.shapeConfig.polygonRadius, thickness * 0.5f);
                result.thickness = thickness;
                result.centroidOffset = float2{ 0.0f, 0.0f };
                result.source = ColliderSource::Config;
            }
        }

        if (result.shapeConfig.type == Box2DShapeConfig::Type::None)
        {
            ReportInvalidBox2DConfig(type, "cannot build a collider without a Box2DShapeConfig; registration should abort earlier");
            return result;
        }

        const auto isInvalidPolygon = [](const std::vector<b2Vec2>& vertices)
        {
            return vertices.size() < 3;
        };

        result.meshPolygons.erase(
            std::remove_if(result.meshPolygons.begin(), result.meshPolygons.end(), isInvalidPolygon),
            result.meshPolygons.end());

        if ((result.shapeConfig.type == Box2DShapeConfig::Type::Box || result.shapeConfig.type == Box2DShapeConfig::Type::Polygon)
            && result.shapeConfig.vertices.size() < 3 && result.meshPolygons.empty())
        {
            ReportInvalidBox2DConfig(type, "requires explicit vertices instead of mesh-derived bounds");
            return result;
        }

        if (result.shapeConfig.type == Box2DShapeConfig::Type::Circle && !(result.shapeConfig.radius > 0.0f))
        {
            ReportInvalidBox2DConfig(type, "requires a positive radius instead of mesh-derived bounds");
            return result;
        }

        const float densityScale = ComputeDensityScale(transform.scale, result.shapeConfig.type);
        result.shapeConfig.density *= densityScale;

        auto computeScaledDimensions = [&](const std::vector<b2Vec2>& vertices) -> std::optional<std::pair<float, float>>
        {
            if (vertices.size() < 3)
            {
                return std::nullopt;
            }

            b2Vec2 min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
            b2Vec2 max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
            for (const auto& vertex : vertices)
            {
                const float scaledX = vertex.x * colliderScale.x;
                const float scaledY = vertex.y * colliderScale.y;
                min.x = std::min(min.x, scaledX);
                min.y = std::min(min.y, scaledY);
                max.x = std::max(max.x, scaledX);
                max.y = std::max(max.y, scaledY);
            }

            return std::make_optional(std::make_pair(max.x - min.x, max.y - min.y));
        };

        auto scaleVertices = [](std::vector<b2Vec2>& vertices, float factor)
        {
            for (auto& vertex : vertices)
            {
                vertex.x *= factor;
                vertex.y *= factor;
            }
        };

        const float minDimension = Physics::kMinimumShapeDimensionMeters;
        if (result.shapeConfig.type == Box2DShapeConfig::Type::Polygon)
        {
            if (const auto dimensions = computeScaledDimensions(result.shapeConfig.vertices))
            {
                const float width = dimensions->first;
                const float height = dimensions->second;
                const float smallest = std::min(width, height);
                if (!(smallest >= minDimension))
                {
                    if (smallest <= 0.0f)
                    {
                        result.shapeConfig.type = Box2DShapeConfig::Type::Circle;
                        result.shapeConfig.radius = std::max(minDimension * 0.5f, result.shapeConfig.radius);
                        result.shapeConfig.polygonRadius = 0.0f;
                        result.shapeConfig.vertices.clear();
                        result.meshPolygons.clear();
                    }
                    else
                    {
                        const float scaleFactor = minDimension / smallest;
                        scaleVertices(result.shapeConfig.vertices, scaleFactor);
                        for (auto& polygon : result.meshPolygons)
                        {
                            scaleVertices(polygon, scaleFactor);
                        }
                        result.shapeConfig.polygonRadius *= scaleFactor;
                        result.thickness *= scaleFactor;
                        result.centroidOffset.x *= scaleFactor;
                        result.centroidOffset.y *= scaleFactor;
                    }

                    if (!s_loggedPolygonFallback)
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Collider for kind='" << resolvedKind
                            << "' resized to meet Box2D min dimension (width=" << width
                            << ", height=" << height << ") target=" << minDimension << "\n";
                        Diagnostics::LogInfo(oss.str());
                        s_loggedPolygonFallback = true;
                    }

                    result.source = ColliderSource::Fallback;
                }
            }
        }

        auto enforceMinimumCircleRadius = [&](float minRadius)
        {
            if (result.shapeConfig.type != Box2DShapeConfig::Type::Circle)
            {
                return;
            }

            const float currentRadius = result.shapeConfig.radius;
            if (!(currentRadius > 0.0f) || currentRadius >= minRadius)
            {
                return;
            }

            const float scaleFactor = minRadius / currentRadius;
            result.shapeConfig.radius = minRadius;
            result.shapeConfig.polygonRadius *= scaleFactor;
            result.thickness *= scaleFactor;
            result.centroidOffset.x *= scaleFactor;
            result.centroidOffset.y *= scaleFactor;

            std::ostringstream oss;
            oss << kLogPrefix << "Collider for kind='" << resolvedKind
                << "' radius clamped to min playable size from " << currentRadius
                << " to " << minRadius << "\n";
            DebugTrace(oss.str());
        };

        switch (type)
        {
        case SweetsType::Strawberry:
            enforceMinimumCircleRadius(kMinimumBerryColliderRadius);
            break;
        case SweetsType::Blueberry:
            enforceMinimumCircleRadius(kMinimumBlueberryColliderRadius);
            break;
        default:
            break;
        }

        return result;
    }

    float ComputeFallbackHorizontalLimit(float margin)
    {
        const float sanitizedMargin = std::max(0.0f, margin);
        return (kPlatformWidth * 0.5f) + sanitizedMargin;
    }

    ::SweetsSpace::SweetsState EvaluateState(::SweetsSpace::ActiveSweets& sweets, ::SweetsSpace::SweetsState currentState, const SweetsSpace::SweetsManager& manager)
    {
        if (currentState == ::SweetsSpace::SweetsState::Carried)
        {
            return ::SweetsSpace::SweetsState::Carried;
        }

        const float speedSq = ComputeSpeedSq(sweets);
        if (speedSq > kRestVelocityThresholdSq)
        {
            return ::SweetsSpace::SweetsState::Active;
        }

        bool foundationOnly = false;
        bool touching = manager.HasSupportingContact(sweets, &foundationOnly);
        if (!touching)
        {
            if (auto* physicsMachine = sweets.GetPhysicsMachine())
            {
                const b2BodyId bodyId = physicsMachine->FindBody(sweets);
                if (b2Body_IsValid(bodyId))
                {
                    const int capacity = b2Body_GetContactCapacity(bodyId);
                    if (capacity > 0)
                    {
                        std::vector<b2ContactData> contacts(static_cast<std::size_t>(capacity));
                        const int count = b2Body_GetContactData(bodyId, contacts.data(), capacity);
                        for (int i = 0; i < count; ++i)
                        {
                            if (contacts[i].manifold.pointCount > 0)
                            {
                                touching = true;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (touching)
        {
            if (foundationOnly)
            {
                return ::SweetsSpace::SweetsState::Active;
            }

            // Keep previously stacked sweets stacked, but leave newly supported sweets Active so
            // pickup eligibility is not blocked by contact flags alone.
            return currentState == ::SweetsSpace::SweetsState::Stacked
                ? ::SweetsSpace::SweetsState::Stacked
                : ::SweetsSpace::SweetsState::Active;
        }

        return ::SweetsSpace::SweetsState::Active;
    }

    bool IsHorizontallyOutOfBounds(
        ::SweetsSpace::ActiveSweets& sweets,
        const std::optional<std::pair<float, float>>& worldBounds,
        float margin,
        float fallbackLimit,
        bool* usedFallback)
    {
        sweets.SyncColliderBounds();
        const AABB bounds = sweets.GetAABB();
        float minX = bounds.min.x;
        float maxX = bounds.max.x;

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

    std::vector<::SweetsSpace::WeightedSweetsType> ParseWeights(const std::string& raw, bool* outError)
    {
        std::vector<::SweetsSpace::WeightedSweetsType> weights;
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
                        oss << kLogPrefix << "重量トークン '" << weightToken
                            << "' (元='" << raw << "') のパースに失敗しちゃった: " << ex.what() << "\n";
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
                oss << kLogPrefix << "重量指定 '" << raw << "' に知らないスイーツキー '" << kind
                    << "' が混じってたよ \n";
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

    float3 ScaleVector(const float3& value, const float3& scale)
    {
        return float3{ value.x * scale.x, value.y * scale.y, value.z * scale.z };
    }

    HullExtents ComputeHullExtents(const std::vector<float2>& hull)
    {
        HullExtents extents{};
        if (hull.empty())
        {
            return extents;
        }

        for (const auto& pt : hull)
        {
            if (!std::isfinite(pt.x) || !std::isfinite(pt.y))
            {
                continue;
            }

            extents.valid = true;
            extents.min.x = std::min(extents.min.x, pt.x);
            extents.min.y = std::min(extents.min.y, pt.y);
            extents.max.x = std::max(extents.max.x, pt.x);
            extents.max.y = std::max(extents.max.y, pt.y);
        }

        return extents;
    }

    ModelBounds ComputeBoundsFromHull(const std::vector<float2>& hull, const float2& centroidOffset, float thickness)
    {
        ModelBounds bounds{};
        HullExtents extents{};

        for (const auto& pt : hull)
        {
            const float2 shifted{ pt.x + centroidOffset.x, pt.y + centroidOffset.y };
            if (!std::isfinite(shifted.x) || !std::isfinite(shifted.y))
            {
                continue;
            }

            extents.valid = true;
            extents.min.x = std::min(extents.min.x, shifted.x);
            extents.min.y = std::min(extents.min.y, shifted.y);
            extents.max.x = std::max(extents.max.x, shifted.x);
            extents.max.y = std::max(extents.max.y, shifted.y);
        }

        if (!extents.valid)
        {
            return bounds;
        }

        bounds.min = float3{ extents.min.x, extents.min.y, -thickness * 0.5f };
        bounds.max = float3{ extents.max.x, extents.max.y, thickness * 0.5f };
        bounds.valid = true;
        return bounds;
    }

    float ComputeAdaptiveThicknessFromExtents(float width, float height, const float3& scale, float minThickness)
    {
        const float boundsMaxExtent = std::max(width, height);
        const float largestScaleComponent = std::max(scale.x, std::max(scale.y, scale.z));
        const float computed = std::max(boundsMaxExtent * 0.02f, largestScaleComponent * 0.02f);
        return ApplyThicknessEpsilon(computed, minThickness);
    }

    float ComputeAdaptiveThickness(const std::vector<float2>& hull, const float3& scale, float minThickness)
    {
        const HullExtents extents = ComputeHullExtents(hull);
        const float width = extents.valid ? (extents.max.x - extents.min.x) : 0.0f;
        const float height = extents.valid ? (extents.max.y - extents.min.y) : 0.0f;
        return ComputeAdaptiveThicknessFromExtents(width, height, scale, minThickness);
    }

    float SignedTriangleArea(const float2& a, const float2& b, const float2& c)
    {
        const float abx = b.x - a.x;
        const float aby = b.y - a.y;
        const float acx = c.x - a.x;
        const float acy = c.y - a.y;
        return (abx * acy) - (aby * acx);
    }

    std::vector<float2> SimplifyHull(const std::vector<float2>& hull, std::size_t targetMin, std::size_t targetMax)
    {
        if (hull.empty())
        {
            return hull;
        }

        std::vector<float2> simplified = hull;
        const std::size_t minimum = std::max<std::size_t>(3, std::min(targetMin, targetMax));

        auto ensureCCW = [](std::vector<float2>& polygon) {
            float signedAreaSum = 0.0f;
            for (std::size_t i = 0; i < polygon.size(); ++i)
            {
                const std::size_t next = (i + 1) % polygon.size();
                signedAreaSum += (polygon[i].x * polygon[next].y) - (polygon[next].x * polygon[i].y);
            }

            if (signedAreaSum < 0.0f)
            {
                std::reverse(polygon.begin(), polygon.end());
            }
        };

        ensureCCW(simplified);

        while (simplified.size() < minimum)
        {
            float largestLenSq = -1.0f;
            std::size_t insertEdge = 0;

            for (std::size_t i = 0; i < simplified.size(); ++i)
            {
                const std::size_t next = (i + 1) % simplified.size();
                const float dx = simplified[next].x - simplified[i].x;
                const float dy = simplified[next].y - simplified[i].y;
                const float lenSq = (dx * dx) + (dy * dy);

                if (lenSq > largestLenSq)
                {
                    largestLenSq = lenSq;
                    insertEdge = i;
                }
            }

            const std::size_t next = (insertEdge + 1) % simplified.size();
            const float2 midpoint{ (simplified[insertEdge].x + simplified[next].x) * 0.5f,
                                   (simplified[insertEdge].y + simplified[next].y) * 0.5f };

            simplified.insert(simplified.begin() + static_cast<std::ptrdiff_t>(next), midpoint);
        }

        while (simplified.size() > targetMax && simplified.size() > minimum)
        {
            float smallestMagnitude = std::numeric_limits<float>::max();
            std::size_t removeIndex = 0;

            for (std::size_t i = 0; i < simplified.size(); ++i)
            {
                const std::size_t prev = (i + simplified.size() - 1) % simplified.size();
                const std::size_t next = (i + 1) % simplified.size();
                const float area = SignedTriangleArea(simplified[prev], simplified[i], simplified[next]);
                const float magnitude = std::abs(area);

                if (magnitude < smallestMagnitude)
                {
                    smallestMagnitude = magnitude;
                    removeIndex = i;
                }
            }

            simplified.erase(simplified.begin() + static_cast<std::ptrdiff_t>(removeIndex));
        }

        ensureCCW(simplified);
        return simplified;
    }

    MeshHullSummary AnalyzeModelGeometry(const std::string& path, float thicknessPadding, float minThickness)
    {
        MeshHullSummary summary{};
        const float sanitizedPadding = SanitizeThicknessPadding(thicknessPadding);
        summary.paddingFactor = sanitizedPadding;

        struct PositionOnly
        {
            float x;
            float y;
            float z;
        };

        std::vector<PositionOnly> positions;
        std::vector<UINT> indices;
        float minZ = std::numeric_limits<float>::max();
        float maxZ = std::numeric_limits<float>::lowest();

        Assimp::Importer importer;
        const unsigned int flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices;
        const aiScene* scene = importer.ReadFile(path.c_str(), flags);
        if (!scene || !scene->HasMeshes())
        {
            std::ostringstream oss;
            oss << kLogPrefix
                << "ジオメトリ確認用のモデル読み込みに失敗したよ path='" << path << "'"
                << " reason='" << (scene ? "メッシュなし" : importer.GetErrorString()) << "'\n";
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
                minZ = std::min(minZ, pos.z);
                maxZ = std::max(maxZ, pos.z);
            }

            const unsigned int numFaces = mesh->mNumFaces;
            for (unsigned int faceIndex = 0; faceIndex < numFaces; ++faceIndex)
            {
                const aiFace& face = mesh->mFaces[faceIndex];
                if (face.mNumIndices < 3)
                {
                    continue;
                }

                // aiProcess_Triangulate をかけてるからここは三角形になるよ
                indices.push_back(baseIndex + face.mIndices[0]);
                indices.push_back(baseIndex + face.mIndices[1]);
                indices.push_back(baseIndex + face.mIndices[2]);
            }
        }

        if (positions.empty())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "ジオメトリ解析で '" << path << "' の頂点が一つも見つからなかったよ\n";
            DebugTrace(oss.str());
            return summary;
        }

        MeshBuffer::Description desc{};
        desc.pVtx = positions.data();
        desc.vtxSize = sizeof(PositionOnly);
        desc.vtxCount = static_cast<UINT>(positions.size());
        desc.pIdx = indices.empty() ? nullptr : indices.data();
        desc.idxSize = indices.empty() ? 0u : sizeof(UINT);
        desc.idxCount = static_cast<UINT>(indices.size());
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        const std::string normalizedKind = NormalizeKey(path);
        const auto rawHull = Collision::BuildConvexHullFromMesh(desc, normalizedKind, path);
        const std::vector<float2> originalHull = rawHull.hullPointsCentered;
        constexpr std::size_t kMinHullPoints = 6;
        constexpr std::size_t kMaxHullPoints = 12;
        std::vector<float2> simplified = SimplifyHull(originalHull, kMinHullPoints, kMaxHullPoints);
        if (simplified.empty())
        {
            simplified = originalHull;
        }
        summary.centroidOffset = rawHull.centroidOffset;
        summary.hull = simplified;
        const HullExtents extents = ComputeHullExtents(summary.hull);
        const float width  = extents.valid ? (extents.max.x - extents.min.x) : 0.0f;
        const float height = extents.valid ? (extents.max.y - extents.min.y) : 0.0f;

        const float geometryThickness = (maxZ >= minZ)
            ? std::max(maxZ - minZ, 0.0f)
            : 0.0f;
        const float adaptiveThickness = ComputeAdaptiveThicknessFromExtents(width, height, float3{1.0f, 1.0f, 1.0f}, minThickness);
        summary.rawThickness = std::max(ApplyThicknessEpsilon(geometryThickness, minThickness), adaptiveThickness);
        const float paddedThickness = summary.rawThickness * sanitizedPadding;
        summary.thickness = paddedThickness;

        summary.bounds = ComputeBoundsFromHull(summary.hull, summary.centroidOffset, summary.thickness);


        std::ostringstream oss;
        oss << kLogPrefix << "ジオメトリ解析したよ path='" << path
            << "' format=" << format
            << " vertices=" << positions.size()
            << " indices=" << indices.size()
            << " hullPoints=" << summary.hull.size()
            << " polygonParts=1"
            << " centroidOffset=(" << summary.centroidOffset.x << ',' << summary.centroidOffset.y << ')'
            << " rawThickness=" << summary.rawThickness
            << " adaptiveThickness=" << summary.thickness
            << " paddingFactor=" << summary.paddingFactor
            << " minThickness=" << minThickness << "\n";
        DebugTrace(oss.str());

#if defined(_DEBUG)
        if (!summary.hull.empty() && summary.bounds.valid)
        {
            std::ostringstream debugHull;
            debugHull << kLogPrefix << "凸包ポイント（重心基準）一覧=";
            for (const auto& p : summary.hull)
            {
                debugHull << '(' << p.x << ',' << p.y << ") ";
            }
            debugHull << " boundsMin=(" << summary.bounds.min.x << ',' << summary.bounds.min.y << ',' << summary.bounds.min.z
                << ") boundsMax=(" << summary.bounds.max.x << ',' << summary.bounds.max.y << ',' << summary.bounds.max.z << ")\n";
            DebugTrace(debugHull.str());
        }
#endif

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

    bool SweetsManager::HasSupportingContact(const ActiveSweets& sweets, bool* foundationOnly) const
    {
        if (foundationOnly)
        {
            *foundationOnly = false;
        }

        auto it = m_contactStates.find(const_cast<ActiveSweets*>(&sweets));
        if (it == m_contactStates.end())
        {
            return false;
        }

        const bool hasFoundation = it->second.foundationContacts > 0;
        const bool hasSweetsContact = it->second.sweetsContacts > 0;
        if (foundationOnly)
        {
            *foundationOnly = hasFoundation && !hasSweetsContact;
        }

        return hasFoundation || hasSweetsContact;
    }

   
    int SweetsManager::GetSweetsCount() const
    {
        int count = 0;
        for (const auto& pair : m_byState)
        {
            count += static_cast<int>(pair.second.size());
        }
        return count;
    }
    void SweetsManager::UpdateContactStateBegin(ActiveSweets* sweets, Entities::PhysicalEntity* other)
    {
        if (!sweets || !other)
        {
            return;
        }

        ContactState& state = m_contactStates[sweets];
        const Collision::CollisionLayer otherLayer = other->GetCollisionLayer();
        if (otherLayer == Collision::CollisionLayer::Floor)
        {
            ++state.foundationContacts;
        }
        else if (otherLayer == Collision::CollisionLayer::Sweets && dynamic_cast<ActiveSweets*>(other))
        {
            ++state.sweetsContacts;
        }
    }

    void SweetsManager::UpdateContactStateEnd(ActiveSweets* sweets, Entities::PhysicalEntity* other)
    {
        if (!sweets || !other)
        {
            return;
        }

        auto it = m_contactStates.find(sweets);
        if (it == m_contactStates.end())
        {
            return;
        }

        ContactState& state = it->second;
        const Collision::CollisionLayer otherLayer = other->GetCollisionLayer();
        if (otherLayer == Collision::CollisionLayer::Floor && state.foundationContacts > 0)
        {
            --state.foundationContacts;
        }
        else if (otherLayer == Collision::CollisionLayer::Sweets && dynamic_cast<ActiveSweets*>(other))
        {
            if (state.sweetsContacts > 0)
            {
                --state.sweetsContacts;
            }
        }

        if (state.foundationContacts <= 0 && state.sweetsContacts <= 0)
        {
            m_contactStates.erase(it);
        }
    }

    void SweetsManager::ClearContactState(ActiveSweets* sweets)
    {
        if (!sweets)
        {
            return;
        }

        m_contactStates.erase(sweets);
    }

    std::optional<SweetsManager::FactoryPreset> SweetsManager::BuildDefaultFactoryPreset(SweetsType type)
    {
        FactoryPreset preset{};
        preset.spawnWeight = 0.0f;
        preset.minScale = float3{ 1.0f, 1.0f, 1.0f };
        preset.maxScale = float3{ 1.0f, 1.0f, 1.0f };
        preset.colliderPreset = ColliderPreset{};

        switch (type)
        {
        case SweetsType::Cake:
            preset.spawnWeight = 1.0f;
            preset.minScale = float3{ 0.85f, 0.85f, 0.85f };
            preset.maxScale = float3{ 1.35f, 1.35f, 1.35f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Box, float3{ 0.02f, 0.02f, 0.02f } };
            return preset;
        case SweetsType::Pudding:
            preset.spawnWeight = 1.0f;
            preset.minScale = float3{ 0.9f, 0.9f, 0.9f };
            preset.maxScale = float3{ 1.2f, 1.2f, 1.2f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Box, float3{ 0.01f, 0.04f, 0.01f } };
            return preset;
        case SweetsType::Sherbet:
            preset.spawnWeight = 1.0f;
            preset.minScale = float3{ 0.75f, 0.75f, 0.75f };
            preset.maxScale = float3{ 1.4f, 1.4f, 1.4f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Capsule, float3{ 0.01f, 0.01f, 0.01f } };
            return preset;
        case SweetsType::Strawberry:
            preset.spawnWeight = 1.0f;
            preset.minScale = float3{ 0.8f, 0.8f, 0.8f };
            preset.maxScale = float3{ 1.25f, 1.25f, 1.25f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Capsule, float3{ 0.0125f, 0.0125f, 0.0125f } };
            return preset;
        case SweetsType::Blueberry:
            preset.spawnWeight = 1.0f;
            preset.minScale = float3{ 0.65f, 0.65f, 0.65f };
            preset.maxScale = float3{ 1.15f, 1.15f, 1.15f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Capsule, float3{ 0.01f, 0.01f, 0.01f } };
            return preset;
        case SweetsType::Pineapple:
            preset.spawnWeight = 0.8f;
            preset.minScale = float3{ 0.85f, 0.85f, 0.85f };
            preset.maxScale = float3{ 1.2f, 1.2f, 1.2f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Box, float3{ 0.02f, 0.05f, 0.02f } };
            return preset;
        case SweetsType::Banana:
            preset.spawnWeight = 0.9f;
            preset.minScale = float3{ 0.8f, 0.8f, 0.8f };
            preset.maxScale = float3{ 1.2f, 1.2f, 1.2f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Capsule, float3{ 0.015f, 0.015f, 0.015f } };
            return preset;
        default:
            return std::nullopt;
        }
    }

    bool SweetsManager::ParseFactoryRow(const std::vector<std::string>& tokens, FactoryPreset& outPreset, size_t lineNumber)
    {
        if (tokens.size() < 11)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "工場プリセットの" << lineNumber << "行目、トークン数が" << tokens.size()
                << "個で足りてないよ（最低11個欲しい） \n";
            DebugTrace(oss.str());
            return false;
        }

        bool parseError = false;
        parseError |= !TryParseFloat(tokens[1], &outPreset.spawnWeight);
        parseError |= !TryParseFloat(tokens[2], &outPreset.minScale.x);
        parseError |= !TryParseFloat(tokens[3], &outPreset.minScale.y);
        parseError |= !TryParseFloat(tokens[4], &outPreset.minScale.z);
        parseError |= !TryParseFloat(tokens[5], &outPreset.maxScale.x);
        parseError |= !TryParseFloat(tokens[6], &outPreset.maxScale.y);
        parseError |= !TryParseFloat(tokens[7], &outPreset.maxScale.z);
        parseError |= !TryParseFloat(tokens[9], &outPreset.colliderPreset.padding.x);
        parseError |= !TryParseFloat(tokens[10], &outPreset.colliderPreset.padding.y);

        if (tokens.size() > 11)
        {
            parseError |= !TryParseFloat(tokens[11], &outPreset.colliderPreset.padding.z);
        }
        else
        {
            outPreset.colliderPreset.padding.z = 0.0f;
        }

        const std::string colliderShape = NormalizeKey(tokens[8]);
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
            oss << kLogPrefix << "" << lineNumber << "行目のコライダープリセット '" << tokens[10]
                << "' が知らない形だったから、とりあえず Box にしとくね\n";
            DebugTrace(oss.str());
            outPreset.colliderPreset.shape = ColliderShape::Box;
        }

        outPreset.spawnWeight = std::max(0.0f, outPreset.spawnWeight);
        return !parseError;
    }

    std::map<std::string, SweetsManager::FactoryPreset> SweetsManager::LoadFactoryPresets(bool* parseError)
    {
        std::map<std::string, FactoryPreset> overrides;
        std::ifstream file(kFactoryPresetCsvPath);
        if (!file.is_open())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "工場プリセットの CSV が '" << kFactoryPresetCsvPath
                << "' に見当たらないので、昔のデフォルトに戻すね\n";
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

            const std::string resolvedKey = ResolveManifestKeyAlias(tokens[0]);
            const std::string normalizedKey = NormalizeKey(resolvedKey);
            overrides[normalizedKey] = preset;

            std::ostringstream oss;
            oss << kLogPrefix
                << "キー '" << tokens[0] << "' の工場プリセットを読んだよ"
                << " resolvedKey='" << resolvedKey << "'"
                << " spawnWeight=" << preset.spawnWeight
                << " minScale=(" << preset.minScale.x << ',' << preset.minScale.y << ',' << preset.minScale.z << ')'
                << " maxScale=(" << preset.maxScale.x << ',' << preset.maxScale.y << ',' << preset.maxScale.z << ')'
                << " collider=" << ToString(preset.colliderPreset.shape)
                << " padding=(" << preset.colliderPreset.padding.x << ',' << preset.colliderPreset.padding.y << ','
                << preset.colliderPreset.padding.z << ")\n";
            DebugTrace(oss.str());
        }

        return overrides;
    }

    std::string SweetsManager::ResolveKindKey(const std::string& key) const
    {
        return ResolveManifestKeyAlias(key);
    }

    std::map<std::string, Collision::ColliderDefinition>::const_iterator SweetsManager::FindColliderDefinition(const std::string& key) const
    {
        const std::string canonicalKey = ResolveKindKey(key);
        const std::string normalizedKey = NormalizeKey(key);
        const std::string normalizedCanonical = NormalizeKey(canonicalKey);

        if (auto normalized = m_colliderDefinitions.find(normalizedKey); normalized != m_colliderDefinitions.end())
        {
            return normalized;
        }

        if (normalizedCanonical != normalizedKey)
        {
            if (auto canonical = m_colliderDefinitions.find(normalizedCanonical); canonical != m_colliderDefinitions.end())
            {
                return canonical;
            }
        }

        return m_colliderDefinitions.end();
    }

    class LateUpdateNode : public Core::UpdateNode
    {
    public:
        explicit LateUpdateNode(::SweetsSpace::SweetsManager& owner)
            : Core::UpdateNode(Core::UpdatePhase::Late, 0)
            , m_owner(owner)
        {
        }

        void OnUpdate(Core::UpdateContext& ctx) override
        {
            m_owner.OnLateUpdate(ctx);
        }

    private:
        ::SweetsSpace::SweetsManager& m_owner;
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
        , m_randomRotationMinDegrees{ 0.0f, 0.0f, 0.0f }
        , m_randomRotationMaxDegrees{ 0.0f, 0.0f, 0.0f }
        , m_liveSweetsCap(kDefaultLiveSweetsCap)
        , m_warnedMissingDrawSystem(false)
        , m_warnedMissingPhysicsMachine(false)
        , m_warnedMissingScheduler(false)
        , m_warnedMissingPhaseCallback(false)
        , m_warnedMissingSpawnOrigins(false)
        , m_loggedContactNotification(false)
        , m_warnedMissingColliderDefinitionKinds()
        , m_warnedMissingFactoryPresetKinds()
        , m_warnedMissingGeneratedHull(false)
        , m_hasHorizontalBounds(false)
        , m_minHorizontalBound(0.0f)
        , m_maxHorizontalBound(0.0f)
        , m_horizontalBoundsMargin(kHorizontalOutOfBoundsMargin)
        , m_generatedColliderThicknessPadding(SanitizeThicknessPadding(kDefaultGeneratedThicknessPadding))
        , m_generatedColliderMinThickness(SanitizeMinThickness(Physics::kMinimumShapeDimensionMeters))
        , m_forceGeneratedColliders(false)
        , m_colliderConfigOverridesEnabled(true)
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
            DebugTrace(std::string(kLogPrefix) + "コライダー定義のロードに失敗したから、AABB の代替を使うね\n");
        }
        else
        {
            std::map<std::string, Collision::ColliderDefinition> normalizedDefinitions;
            for (const auto& entry : m_colliderDefinitions)
            {
                const std::string normalizedKey = NormalizeKey(entry.first);
                normalizedDefinitions[normalizedKey] = entry.second;

                if (normalizedKey != entry.first)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "コライダー定義のキーを '" << entry.first
                        << "' から '" << normalizedKey << "' に揃えておいたよ \n";
                    DebugTrace(oss.str());
                }
            }

            m_colliderDefinitions.swap(normalizedDefinitions);
        }

        Render::ModelCache& cache = Render::ModelCache::Instance();
        const auto& manifest = SweetsPrototypeManifest::Instance();

        bool factoryParseError = false;
        const auto factoryOverrides = LoadFactoryPresets(&factoryParseError);
        const ModelBounds fallbackBounds{ float3{ -0.5f, -0.5f, -0.5f }, float3{ 0.5f, 0.5f, 0.5f }, true };
        const auto prototypes = manifest.Enumerate();
        if (prototypes.empty())
        {
            DebugTrace(std::string(kLogPrefix) + "スイーツのプロトタイプが一つも登録されてないから、スポナー表は空のままだよ\n");
        }

        for (const auto& prototype : prototypes)
        {
            const auto* descriptor = manifest.Find(prototype.type);
            if (!descriptor)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "タイプ '" << ToString(prototype.type)
                    << "' のプロトタイプを探せなかったのでスキップするね \n";
                DebugTrace(oss.str());
                continue;
            }

            const std::string kind = descriptor->key;
            const std::string resolvedKind = ResolveKindKey(kind);
            const std::string normalizedKind = NormalizeKey(resolvedKind);
            const bool isAliasedKind = NormalizeKey(kind) != normalizedKind;
            const std::string aliasSuffix = isAliasedKind ? std::string{" (alias for '"} + kind + "')" : std::string{};
            const std::string descriptorPath = descriptor->modelPath;
            const auto overrideIt = factoryOverrides.find(normalizedKind);
            const bool hasOverride = overrideIt != factoryOverrides.end();
            std::optional<FactoryPreset> defaultPreset = BuildDefaultFactoryPreset(prototype.type);

            if (!hasOverride && !defaultPreset)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "種類 '" << kind
                    << "' (type=" << ToString(prototype.type) << ") に使える工場プリセットがないから、登録はパスするよ\n";
                DebugTrace(warn.str());
                continue;
            }

            FactoryPreset preset = hasOverride ? overrideIt->second : *defaultPreset;
            m_factoryPresets[normalizedKind] = preset;

            std::ostringstream presetLog;
            presetLog << kLogPrefix << "種類 '" << resolvedKind << "' の工場プリセットを決めたよ";
            if (isAliasedKind)
            {
                presetLog << " (from key '" << kind << "')";
            }
            presetLog << " source=" << (hasOverride ? "config" : "fallback")
                << " spawnWeight=" << preset.spawnWeight
                << " minScale=(" << preset.minScale.x << ',' << preset.minScale.y << ',' << preset.minScale.z << ')'
                << " maxScale=(" << preset.maxScale.x << ',' << preset.maxScale.y << ',' << preset.maxScale.z << ')'
                << " collider=" << ToString(preset.colliderPreset.shape)
                << " padding=(" << preset.colliderPreset.padding.x << ',' << preset.colliderPreset.padding.y << ','
                << preset.colliderPreset.padding.z << ")"
                << " overrideParseError=" << std::boolalpha << factoryParseError << "\n";
            DebugTrace(presetLog.str());

            const std::string registeredPath = descriptorPath.empty() ? kPlaceholderModelPath : descriptorPath;
            std::ostringstream oss;
            oss << kLogPrefix << "スイーツの種類 '" << kind
                << "' をアセット '" << registeredPath << "' と一緒に登録するね\n";
            DebugTrace(oss.str());

            const SweetsType type = prototype.type;

            if (!ValidateBaseShapeDefinition(type))
            {
                DebugTrace(std::string(kLogPrefix) + "スイーツの形状設定に問題があるから、ファクトリー登録をスキップするね\n");
                continue;
            }

            auto factory = [this, &cache, kind, type, preset,
                resolvedKind, normalizedKind, isAliasedKind, aliasSuffix](const Transform& transform)
                -> std::shared_ptr<ActiveSweets>
            {
                const auto& manifest = SweetsPrototypeManifest::Instance();
                const auto* descriptor = manifest.Find(type);
                std::string descriptorKey = descriptor ? descriptor->key : resolvedKind;
                std::string descriptorPath = descriptor ? descriptor->modelPath : std::string{};

                if (!descriptor)
                {
                    std::ostringstream warn;
                    warn << kLogPrefix << "種類 '" << resolvedKind
                        << "'" << aliasSuffix << " のプロトタイプ説明が見当たらないから、プレースホルダー '" << kPlaceholderModelPath << "' を使うよ\n";
                    DebugTrace(warn.str());
                    descriptorPath = kPlaceholderModelPath;
                }
                else if (descriptorPath.empty())
                {
                    std::ostringstream warn;
                    warn << kLogPrefix << "種類 '" << descriptorKey << "' のパスが空だったから、プレースホルダー '" << kPlaceholderModelPath << "' に頼るね\n";
                    DebugTrace(warn.str());
                    descriptorPath = kPlaceholderModelPath;
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "種類 '" << descriptorKey << "' のプロトタイプを解決したよ"
                        << aliasSuffix << " path='" << descriptor->modelPath << "'.\n";
                    DebugTrace(oss.str());
                }

                auto model = type == SweetsType::Fairy ? CreateProceduralFairyModel() : nullptr;
                if (!model)
                {
                    const std::string primaryPath = descriptorPath.empty() ? kPlaceholderModelPath : descriptorPath;
                    model = cache.Get(primaryPath);
                    if (!model && primaryPath != kPlaceholderModelPath)
                    {
                        std::ostringstream warn;
                        warn << kLogPrefix << "スイーツ '" << descriptorKey
                            << "' のモデル読み込みに失敗したから、プレースホルダー '" << kPlaceholderModelPath
                            << "' を試してみるね \n";
                        DebugTrace(warn.str());
                        model = cache.Get(kPlaceholderModelPath);
                    }
                }

                if (!model)
                {
                    std::ostringstream fail;
                    fail << kLogPrefix << "スイーツ '" << descriptorKey
                        << "' は代替も全部外れでモデルが用意できなかったよ\n";
                    DebugTrace(fail.str());

                    return nullptr;
                }

                static std::unordered_set<std::string> s_loggedModelLoads;
                if (s_loggedModelLoads.insert(primaryPath).second)
                {
                    std::ostringstream modelLog;
                    modelLog << kLogPrefix << "[SpawnModel] kind='" << descriptorKey
                        << "' path='" << primaryPath << "' loaded=" << std::boolalpha << static_cast<bool>(model) << "\n";
                    Diagnostics::LogInfo(modelLog.str());
                }

                Transform physicsTransform = transform;
                VisualTransform visualTransform{};
                visualTransform.position = physicsTransform.position;
                visualTransform.rotation = physicsTransform.rotation;

                const auto computeDeterministicScale = [](float minValue, float maxValue)
                {
                    const float lo = std::min(minValue, maxValue);
                    const float hi = std::max(minValue, maxValue);
                    return (lo + hi) * 0.5f;
                };

                const float3 sampledScale{
                    computeDeterministicScale(preset.minScale.x, preset.maxScale.x),
                    computeDeterministicScale(preset.minScale.y, preset.maxScale.y),
                    computeDeterministicScale(preset.minScale.z, preset.maxScale.z)
                };

                const float normalizationScale = ComputeNormalizationScale(*model, descriptorKey.c_str());
                float3 finalScale = float3
                {
                    physicsTransform.scale.x * sampledScale.x * normalizationScale,
                    physicsTransform.scale.y * sampledScale.y * normalizationScale,
                    physicsTransform.scale.z * sampledScale.z * normalizationScale
                };

                finalScale = ClampScaleToGameplayRange(finalScale, descriptorKey.c_str());
                visualTransform.scale = finalScale;
                physicsTransform.scale = visualTransform.scale;

                const Render::PivotInfo pivotInfo = Render::ComputePivotInfoOriginBottom(model, visualTransform.scale);
                float appliedPivotOffsetY = pivotInfo.offsetY;
                if ((type == SweetsType::Strawberry || type == SweetsType::Blueberry) && pivotInfo.rawOffsetY < 0.0f)
                {
                    appliedPivotOffsetY = std::max(pivotInfo.offsetY, 0.0f);
                }

                physicsTransform.position.y += appliedPivotOffsetY;
                visualTransform.position.y += appliedPivotOffsetY;

                const auto colliderBuild = BuildColliderForSweets(kind, type, physicsTransform, descriptor->modelPath, model);

                if (colliderBuild.shapeConfig.type == Box2DShapeConfig::Type::None)
                {
                    DebugTrace(std::string(kLogPrefix) + "ボディ形状が無効だから、このスイーツの生成はスキップするね\n");
                    return nullptr;
                }

                auto sweets = MakeSweets<ActiveSweets>(kind, model, physicsTransform, colliderBuild.shapeConfig, Collision::CollisionLayer::Sweets,
                    Core::UpdatePhase::Physics, 0);

                if (sweets)
                {
                    sweets->SetBox2DMesh(colliderBuild.meshPolygons, colliderBuild.meshBoundary);
                    sweets->SetVisualTransform(visualTransform);

                    const char* sourceLabel = "fallback";
                    switch (colliderBuild.source)
                    {
                    case ColliderSource::Config:
                        sourceLabel = "config";
                        break;
                    case ColliderSource::Generated:
                        sourceLabel = "mesh";
                        break;
                    default:
                        break;
                    }

                    std::ostringstream created;
                    created << kLogPrefix << "[SpawnRender] tag='" << sweets->GetTag()
                        << "' renderPos=(" << visualTransform.position.x << ',' << visualTransform.position.y << ',' << visualTransform.position.z
                        << ") renderRotDeg=(" << visualTransform.rotation.x << ',' << visualTransform.rotation.y << ',' << visualTransform.rotation.z
                        << ") renderScale=(" << visualTransform.scale.x << ',' << visualTransform.scale.y << ',' << visualTransform.scale.z
                        << ") pivotOffsetY(raw/clamped/applied)=" << pivotInfo.rawOffsetY << '/' << pivotInfo.offsetY << '/' << appliedPivotOffsetY
                        << " halfHeight=" << pivotInfo.halfHeight
                        << " colliderSource=" << sourceLabel
                        << " policy=" << Render::kPivotPolicyName << " で描画したよ\n";
                    Diagnostics::LogInfo(created.str());
                }

                return sweets;
            };

            auto [_, inserted] = m_spawner.emplace(normalizedKind, std::move(factory));
            if (!inserted)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "種類 '" << kind
                    << "' のスポナーはもうあったから、新しいファクトリーは無視したよ \n";
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
                    oss << kLogPrefix << "状態 " << StateToString(bucketState)
                        << " から null のスイーツを掃除したよ\n";
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
                    oss << kLogPrefix << "Update 無効化中だったから、状態 " << StateToString(bucketState)
                        << " から sweets ptr=" << sweets.get() << " を片付けたよ \n";
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
                    if (usedFallbackBounds && !m_warnedMissingHorizontalBounds)
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "水平境界のフォールバックを使うね 幅制限="
                            << fallbackHorizontalLimit << " (半幅＋マージン) \n";
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

                    const SweetsState desiredState = EvaluateState(*sweets, bucketState, *this);
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
            oss << kLogPrefix << "状態変更 ptr=" << change.sweets.get()
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
            oss << kLogPrefix << "後処理で " << stagedThisFrame << " 件を待機、 " << markedSafe
                << " 件を削除OKにして、 " << removed
                << " 件のスイーツを消したよ \n";
            DebugTrace(oss.str());
        }
#endif
    }

    void SweetsManager::DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<::CameraSpace::ICamera>& camera)
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

            sweets->SyncColliderBounds();
            const AABB bounds = sweets->GetAABB();
            float sweetsTop = bounds.max.y;

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
            oss << kLogPrefix << "積み上げ進捗=" << m_stackedInBatch
                << '/' << m_batchStackTarget << " tag='" << sweets->GetTag() << "' だよ \n";
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

        m_contactStates.clear();

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
        oss << kLogPrefix << "ClearAllSweets で " << staged
            << " 個を破棄予約したよ（すでに予約済み " << alreadyPending << " 個）/ "
            << m_byState.size() << " 状態分 \n";
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
        oss << kLogPrefix << "フェーズが " << m_currentPhase << " に変わったよ\n";
        DebugTrace(oss.str());

        OnPhaseConfigurationChanged(m_currentPhase);

        if (m_phaseChangeCallback)
        {
            m_phaseChangeCallback(m_currentPhase);
        }
        else if (!m_warnedMissingPhaseCallback)
        {
            DebugTrace(std::string(kLogPrefix) + "フェーズ変更のコールバックがないので、スポーン配置はそのままにしとくね \n");
            m_warnedMissingPhaseCallback = true;
        }
    }

    int SweetsManager::GetCurrentPhase() const noexcept
    {
        return m_currentPhase;
    }

    std::vector<std::pair<std::string, SweetsManager::FactoryPreset>> SweetsManager::EnumerateFactoryPresets() const
    {
        std::vector<std::pair<std::string, FactoryPreset>> presets;
        presets.reserve(m_factoryPresets.size());
        for (const auto& entry : m_factoryPresets)
        {
            presets.push_back(entry);
        }
        return presets;
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
        const std::string resolvedKind = ResolveKindKey(kind);
        const std::string normalizedKind = NormalizeKey(kind);
        const std::string normalizedResolvedKind = NormalizeKey(resolvedKind);

        auto it = m_spawner.find(normalizedResolvedKind);
        if (it == m_spawner.end() && normalizedResolvedKind != normalizedKind)
        {
            it = m_spawner.find(normalizedKind);
        }
        if (it == m_spawner.end())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "知らない種類 '" << kind << "' でスポーン要求が来たよ"
                << " (resolved='" << resolvedKind
                << "' normalizedRaw='" << normalizedKind
                << "' normalizedResolved='" << normalizedResolvedKind << "') 登録済みの種類:";
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
            oss << kLogPrefix << "種類 '" << kind << "' のスポナーが null を返しちゃった \n";
            DebugTrace(oss.str());
            return nullptr;
        }
    
        SetState(sweets, SweetsState::Active);
        RegisterWithSystems(sweets);

        if (m_physicsMachine && sweets)
        {
            const b2BodyId bodyId = m_physicsMachine->FindBody(*sweets);
            if (b2Body_IsValid(bodyId))
            {
                const b2Vec2 position = b2Body_GetPosition(bodyId);
                Transform synced = sweets->GetTransform();
                synced.position.x = position.x;
                synced.position.y = position.y;
                sweets->SetTransform(synced);
            }
        }

        return sweets;
    }

    PickupQueryResult SweetsManager::FindNearestCarryable(
        const float3& pickupCenter,
        float halfWidth,
        float halfHeight,
        float halfDepth,
        float depthThreshold,
        const std::vector<std::shared_ptr<ActiveSweets>>& exclude,
        float maxDistance) const
    {
        PickupQueryResult result{};
        result.failReason = PickupFailReason::InvalidConfiguration;

        const auto sanitizeCoordinate = [](float value)
        {
            return std::isfinite(value) ? value : 0.0f;
        };

        const float3 sanitizedCenter
        {
            sanitizeCoordinate(pickupCenter.x),
            sanitizeCoordinate(pickupCenter.y),
            sanitizeCoordinate(pickupCenter.z),
        };
        if (!(halfWidth > 0.0f) || !(halfHeight > 0.0f))
        {
            return result;
        }
        const float sanitizedHalfDepth = std::max(0.0f, std::isfinite(halfDepth) ? halfDepth : 0.0f);
        const float sanitizedDepthThreshold = (std::isfinite(depthThreshold) && depthThreshold >= 0.0f)
            ? depthThreshold
            : std::numeric_limits<float>::infinity();
        const float sanitizedMaxDistance = (std::isfinite(maxDistance) && maxDistance > 0.0f)
            ? maxDistance
            : std::numeric_limits<float>::infinity();

        const float broadPhaseHalfWidth = halfWidth + sanitizedMaxDistance;
        const float broadPhaseHalfHeight = halfHeight + sanitizedMaxDistance;
        const float broadPhaseHalfDepth = std::max(sanitizedHalfDepth, sanitizedDepthThreshold);

        AABB pickupBounds{};
        pickupBounds.min.x = sanitizedCenter.x - broadPhaseHalfWidth;
        pickupBounds.max.x = sanitizedCenter.x + broadPhaseHalfWidth;
        pickupBounds.min.y = sanitizedCenter.y - broadPhaseHalfHeight;
        pickupBounds.max.y = sanitizedCenter.y + broadPhaseHalfHeight;
        pickupBounds.min.z = sanitizedCenter.z - broadPhaseHalfDepth;
        pickupBounds.max.z = sanitizedCenter.z + broadPhaseHalfDepth;

        const auto intersects = [&](const AABB& bounds)
        {
            const float overlapX = std::min(bounds.max.x, pickupBounds.max.x) - std::max(bounds.min.x, pickupBounds.min.x);
            const float overlapY = std::min(bounds.max.y, pickupBounds.max.y) - std::max(bounds.min.y, pickupBounds.min.y);
            const float overlapZ = std::min(bounds.max.z, pickupBounds.max.z) - std::max(bounds.min.z, pickupBounds.min.z);
            return overlapX >= 0.0f && overlapY >= 0.0f && overlapZ >= 0.0f;
        };

        const auto computeDepthDelta = [&](const AABB& bounds)
        {
            const float frontExcess = bounds.min.z - (sanitizedCenter.z + sanitizedHalfDepth);
            const float backExcess = (sanitizedCenter.z - sanitizedHalfDepth) - bounds.max.z;
            const float outside = std::max(frontExcess, backExcess);
            return outside > 0.0f ? outside : 0.0f;
        };

        const auto isExcluded = [&](const std::shared_ptr<ActiveSweets>& sweets)
        {
            return std::find(exclude.begin(), exclude.end(), sweets) != exclude.end();
        };

        std::shared_ptr<ActiveSweets> best;
        float bestDistanceSq = std::numeric_limits<float>::max();
        float nearestMeasuredDepthDelta = std::numeric_limits<float>::quiet_NaN();

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

                if (isExcluded(sweets))
                {
                    continue;
                }

                sweets->SyncColliderBounds();
                const AABB bounds = sweets->GetAABB();
                if (!intersects(bounds))
                {
                    continue;
                }

                const float centerX = (bounds.min.x + bounds.max.x) * 0.5f;
                const float centerY = (bounds.min.y + bounds.max.y) * 0.5f;
                const float dx = centerX - sanitizedCenter.x;
                const float dy = centerY - sanitizedCenter.y;
                const float distanceSq = (dx * dx) + (dy * dy);
                if (distanceSq <= bestDistanceSq)
                {
                    bestDistanceSq = distanceSq;
                    best = sweets;
                    nearestMeasuredDepthDelta = computeDepthDelta(bounds);
                }
            }
        };

        scanState(SweetsState::Ground);
        scanState(SweetsState::Active);
        scanState(SweetsState::Stacked);

        if (bestDistanceSq == std::numeric_limits<float>::max())
        {
            result.failReason = PickupFailReason::NoCandidates;
            return result;
        }

        const float bestDistance = std::sqrt(bestDistanceSq);
        result.distance = bestDistance;
        result.depthDelta = nearestMeasuredDepthDelta;

        if (bestDistance > sanitizedMaxDistance)
        {
            result.failReason = PickupFailReason::DistanceExceeded;
            return result;
        }

        if (nearestMeasuredDepthDelta > sanitizedDepthThreshold)
        {
            result.failReason = PickupFailReason::DepthExceeded;
            return result;
        }

        result.result = PickupResult::Success;
        result.failReason = PickupFailReason::None;
        result.sweets = best;
        return result;
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

        ClearContactState(sweets.get());

        if (m_physicsMachine)
        {
            m_physicsMachine->UnregisterBody(*sweets);
        }
        else if (!m_warnedMissingPhysicsMachine)
        {
            DebugTrace(std::string(kLogPrefix) + "TryMarkAsCarried で物理マシンが見つからないから、このままシミュレーションしちゃうかも \n");
            m_warnedMissingPhysicsMachine = true;
        }

        SetState(sweets, SweetsState::Carried);

        std::ostringstream oss;
        oss << kLogPrefix << "TryMarkAsCarried 成功 tag='" << sweets->GetTag() << "'\n";
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
            oss << kLogPrefix << "ReleaseCarriedSweets 却下 tag='" << sweets->GetTag() << "' state="
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
            DebugTrace(std::string(kLogPrefix) + "ReleaseCarriedSweets で物理マシンがないから、ボディがシミュレーションされないよ \n");
            m_warnedMissingPhysicsMachine = true;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "ReleaseCarriedSweets 成功 tag='" << sweets->GetTag() << "'\n";
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
            oss << kLogPrefix << "スイーツ '" << sweets->GetTag()
                << "' をスケジューラに登録できなかったよ スケジューラが設定されてないみたい \n";
            DebugTrace(oss.str());
            m_warnedMissingScheduler = true;
        }

        if (m_physicsMachine)
        {
            const auto& colliderComponent = sweets->GetBoxColliderComponent();
            const bool hasColliderComponent = colliderComponent && !colliderComponent->Empty();
            const auto shapeConfig = sweets->GetBox2DShapeConfig();
            const bool hasShapeConfig = shapeConfig && shapeConfig->type != Physics::Box2DShapeConfig::Type::None;
            const bool hasMeshFixtures = sweets->HasBox2DMesh() || !sweets->GetBox2DBoundary().empty();

            if (!hasColliderComponent && !hasShapeConfig && !hasMeshFixtures)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "スイーツ '" << sweets->GetTag()
                    << "' の物理登録を飛ばすね コライダー設定が見当たらない \n";
                DebugTrace(oss.str());
                return;
            }

            const bool alreadyRegistered = m_registeredPhysicsBodies.find(sweets.get()) != m_registeredPhysicsBodies.end()
                || b2Body_IsValid(m_physicsMachine->FindBody(*sweets));
            if (!alreadyRegistered)
            {
                m_physicsMachine->RegisterBody(*sweets);
            }

            const int fixtureCount = colliderComponent ? colliderComponent->createdFixtureCount : 0;
            const bool hasBody = sweets->HasBox2DBody();

            if (!hasBody || fixtureCount <= 0)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "スイーツ '" << sweets->GetTag()
                    << "' の物理登録に失敗 hasBody=" << std::boolalpha << hasBody
                    << " fixtures=" << fixtureCount << "\n";
                DebugTrace(oss.str());
                return;
            }

            m_registeredPhysicsBodies.insert(sweets.get());
        }
        else if (!m_warnedMissingPhysicsMachine)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "スイーツ '" << sweets->GetTag()
                << "' を物理に登録できなかったよ 物理マシンが設定されてない \n";
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
            oss << kLogPrefix << "スイーツ '" << sweets->GetTag()
                << "' を描画登録できなかったよ ドローシステムが設定されてない \n";
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
            oss << kLogPrefix << context << " でスイーツ '" << sweets->GetTag()
                << "' をスケジューラから外せなかったよ スケジューラ未設定 \n";
            DebugTrace(oss.str());
            m_warnedMissingScheduler = true;
        }

        if (m_physicsMachine)
        {
            const bool tracked = m_registeredPhysicsBodies.erase(sweets.get()) > 0;
            if (tracked || b2Body_IsValid(m_physicsMachine->FindBody(*sweets)))
            {
                m_physicsMachine->UnregisterBody(*sweets);
            }
        }
        else if (logWarnings && !m_warnedMissingPhysicsMachine)
        {
            std::ostringstream oss;
            oss << kLogPrefix << context << " でスイーツ '" << sweets->GetTag()
                << "' の物理登録を外せなかったよ 物理マシン未設定 \n";
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
            oss << kLogPrefix << context << " でスイーツ '" << sweets->GetTag()
                << "' の描画登録を外せなかったよ ドローシステム未設定 \n";
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
        ClearContactState(sweets.get());
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
            oss << kLogPrefix << context << " で目標より生存スイーツが減ったから、あとで補充するね  live="
                << liveAfterRemoval << " target=" << desiredLiveTarget << " remainingToStack=" << remainingToStack
                << " stacked=" << m_stackedInBatch << '/' << m_batchStackTarget
                << " queue=" << m_spawnQueue.size()
                << " burstInterval=" << std::fixed << std::setprecision(2) << m_activeBurstInterval << "\n";
            DebugTrace(oss.str());
        }

#if defined(_DEBUG)
        const auto& pos = sweets->GetTransform().position;
        std::ostringstream oss;
        oss << kLogPrefix << context << " でスイーツ tag='" << sweets->GetTag()
            << "' を削除 state=" << StateToString(previousState)
            << " deleteReason=" << ::SweetsSpace::ToString(reason)
            << " outcome=" << ::SweetsSpace::ToString(outcome)
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
            oss << kLogPrefix << "フェーズのスポーン設定が '" << path << "' に見つからないから、デフォルトを使うね \n";
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

        std::unordered_set<std::string> registeredKindsNormalized;
        for (const auto& entry : m_spawner)
        {
            registeredKindsNormalized.insert(NormalizeKey(entry.first));
        }

        const std::vector<WeightedSweetsType> defaultWeights = BuildDefaultWeights();
        std::unordered_map<SweetsType, float, SweetsTypeHash> defaultWeightLookup;
        for (const auto& entry : defaultWeights)
        {
            defaultWeightLookup[entry.kind] = entry.weight;
        }

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
                oss << kLogPrefix << "スポーン設定の" << lineNumber << "行目に '=' がないよ: " << line << "\n";
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
                    oss << kLogPrefix << "" << lineNumber << "行目のフェーズ番号が読めなかったよ: " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                    currentPhase = -1;
                }
                continue;
            }

            if (currentPhase < 0)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "フェーズ番号を決める前にキー '" << key << "' (" << lineNumber << "行目) が来たので無視するね \n";
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
                    oss << kLogPrefix << "" << lineNumber << "行目の interval が読めなかったよ: " << ex.what() << "\n";
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
                    oss << kLogPrefix << "" << lineNumber << "行目の burstInterval が読めなかったよ: " << ex.what() << "\n";
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
                    oss << kLogPrefix << "" << lineNumber << "行目の simultaneous が読めなかったよ: " << ex.what() << "\n";
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
                    oss << kLogPrefix << "" << lineNumber << "行目の simultaneousTarget が読めなかったよ: " << ex.what() << "\n";
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
                        oss << kLogPrefix << "フェーズ " << currentPhase << " が知らないスイーツタイプ '"
                            << ToString(entry.kind) << "' を参照してるからスキップするね \n";
                        DebugTrace(oss.str());
                        continue;
                    }

                    const std::string normalizedKind = NormalizeKey(descriptor->key);
                    if (registeredKindsNormalized.find(normalizedKind) == registeredKindsNormalized.end())
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "フェーズ " << currentPhase << " が知らないスイーツ種類 '"
                            << descriptor->key << "' を参照してるからスキップするね \n";
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
                    oss << kLogPrefix << "フェーズ " << currentPhase
                        << " の weight が有効な値ゼロになったからデフォルトに戻すね \n";
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
                oss << kLogPrefix << "" << lineNumber << "行目のキー '" << key << "' は知らない設定だよ \n";
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
                config.weights = defaultWeights;
            }

            if (config.weights.empty())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "フェーズ " << entry.first
                    << " はデフォルト補填しても weight が残らなかったからスキップするよ \n";
                DebugTrace(oss.str());
                continue;
            }

            EnsureRegisteredWeights(config, entry.first, registeredKindsNormalized, defaultWeightLookup);

            m_phaseConfigs.push_back(std::move(config));
        }

        if (m_phaseConfigs.empty())
        {
            DebugTrace(std::string(kLogPrefix) + "有効なフェーズ設定を読み込めなかったので、デフォルトを当てるね \n");
            ApplyDefaultPhaseConfigs();
            return false;
        }

        m_activePhaseConfigIndex = -1;
        OnPhaseConfigurationChanged(m_currentPhase);

        std::ostringstream oss;
        oss << kLogPrefix << "'" << path << "' から " << m_phaseConfigs.size()
            << " 個のスポーンフェーズを読んだよ (parseError=" << std::boolalpha << parseError << ")\n";
        DebugTrace(oss.str());
        return !parseError;
    }

    void SweetsManager::EnsureRegisteredWeights(PhaseSpawnConfig& config, int phaseIndex,
        const std::unordered_set<std::string>& registeredKindsNormalized,
        const std::unordered_map<SweetsType, float, SweetsTypeHash>& defaultWeightLookup) const
    {
        auto& manifest = SweetsPrototypeManifest::Instance();

        std::unordered_set<std::string> presentNormalized;
        for (const auto& weight : config.weights)
        {
            const auto* descriptor = manifest.Find(weight.kind);
            if (!descriptor)
            {
                continue;
            }

            presentNormalized.insert(NormalizeKey(descriptor->key));
        }

        std::vector<SweetsType> missingTypes;
        missingTypes.reserve(registeredKindsNormalized.size());
        for (const auto& normalizedKind : registeredKindsNormalized)
        {
            if (presentNormalized.find(normalizedKind) != presentNormalized.end())
            {
                continue;
            }

            const auto resolvedType = manifest.Resolve(normalizedKind);
            if (resolvedType)
            {
                missingTypes.push_back(*resolvedType);
            }
        }

        if (missingTypes.empty())
        {
            return;
        }

        std::sort(missingTypes.begin(), missingTypes.end(), [](SweetsType lhs, SweetsType rhs)
        {
            return static_cast<int>(lhs) < static_cast<int>(rhs);
        });

        std::ostringstream warn;
        warn << kLogPrefix << "[Spawn] フェーズ " << phaseIndex
            << " で登録済みの種類に weight が足りないよ:";
        for (auto missing : missingTypes)
        {
            warn << ' ' << ToString(missing);
        }
        warn << " (registered=" << registeredKindsNormalized.size()
            << " configured=" << presentNormalized.size() << ")\n";
        DebugTrace(warn.str());

        if (!kAutoIncludeMissingWeightKinds)
        {
            return;
        }

        for (const auto missing : missingTypes)
        {
            float fallbackWeight = 1.0f;
            const auto defaultIt = defaultWeightLookup.find(missing);
            if (defaultIt != defaultWeightLookup.end() && defaultIt->second > 0.0f)
            {
                fallbackWeight = defaultIt->second;
            }

            config.weights.push_back({ missing, fallbackWeight });
            presentNormalized.insert(NormalizeKey(ToString(missing)));

            std::ostringstream added;
            added << kLogPrefix << "[Spawn] フェーズ " << phaseIndex
                << " に不足してた種類 '" << ToString(missing)
                << "' をフォールバック weight=" << std::fixed << std::setprecision(2) << fallbackWeight
                << " で自動追加したよ (autoInclude=" << std::boolalpha << kAutoIncludeMissingWeightKinds << ")\n";
            DebugTrace(added.str());
        }
    }

    void SweetsManager::SetHorizontalBounds(float minX, float maxX, std::optional<float> margin)
    {
        if (!(std::isfinite(minX) && std::isfinite(maxX)))
        {
            DebugTrace(std::string(kLogPrefix) + "有限値じゃない水平境界の更新は無視したよ \n");
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
        oss << kLogPrefix << "水平境界を更新したよ min=" << m_minHorizontalBound
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
        oss << kLogPrefix << "自動生成コライダーの厚みパディングを " << sanitized
            << (clamped ? " (最小1.0に丸めた)" : "")
            << " にしたよ キャッシュ済みコライダーは再生成まで前の厚みのまま \n";
        DebugTrace(oss.str());

        if (!changed)
        {
            DebugTrace(std::string(kLogPrefix) + "厚みパディングは前回と同じだよ \n");
        }
    }

    void SweetsManager::SetGeneratedColliderMinThickness(float thickness)
    {
        const float sanitized = SanitizeMinThickness(thickness);
        const bool clamped = std::fabs(sanitized - thickness) > 1e-4f;
        const bool changed = std::fabs(sanitized - m_generatedColliderMinThickness) > 1e-4f;
        m_generatedColliderMinThickness = sanitized;

        std::ostringstream oss;
        oss << kLogPrefix << "自動生成コライダーの最小厚みを " << sanitized
            << (clamped ? " (物理下限に丸めた)" : "")
            << " にしたよ 新しく作るコライダーはその薄さで使うね \n";
        DebugTrace(oss.str());

        if (!changed)
        {
            DebugTrace(std::string(kLogPrefix) + "最小厚みは前から変わってないよ \n");
        }
    }

    void SweetsManager::OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
    {
        if (!a || !b)
        {
            return;
        }

        auto* sweetsA = dynamic_cast<ActiveSweets*>(a);
        auto* sweetsB = dynamic_cast<ActiveSweets*>(b);
        if (!sweetsA && !sweetsB)
        {
            return;
        }

        if (sweetsA)
        {
            UpdateContactStateBegin(sweetsA, b);
        }

        if (sweetsB)
        {
            UpdateContactStateBegin(sweetsB, a);
        }

        if (!m_loggedContactNotification)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "'" << a->GetTag() << "' と '" << b->GetTag() << "' が接触し始めたよ \n";
            DebugTrace(oss.str());
            m_loggedContactNotification = true;
        }
    }

    void SweetsManager::OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
    {
        if (!a || !b)
        {
            return;
        }

        auto* sweetsA = dynamic_cast<ActiveSweets*>(a);
        auto* sweetsB = dynamic_cast<ActiveSweets*>(b);
        if (!sweetsA && !sweetsB)
        {
            return;
        }

        if (sweetsA)
        {
            UpdateContactStateEnd(sweetsA, b);
        }

        if (sweetsB)
        {
            UpdateContactStateEnd(sweetsB, a);
        }

        if (!m_loggedContactNotification)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "'" << a->GetTag() << "' と '" << b->GetTag() << "' の接触が終わったよ \n";
            DebugTrace(oss.str());
            m_loggedContactNotification = true;
        }
    }

    void SweetsManager::SetForceGeneratedColliders(bool force)
    {
        const bool changed = m_forceGeneratedColliders != force;
        m_forceGeneratedColliders = force;

        std::ostringstream oss;
        oss << kLogPrefix << "生成コライダー強制 " << (force ? "ON" : "OFF")
            << " 設定ファイルのコライダー定義は"
            << (force ? "生成ジオメトリがあるときは無視するね " : "上書きやフォールバックとして使うかも ")
            << "\n";
        DebugTrace(oss.str());

        if (!changed)
        {
            DebugTrace(std::string(kLogPrefix) + "生成コライダー強制フラグは前回と同じだよ \n");
        }
    }

    void SweetsManager::SetColliderConfigOverridesEnabled(bool enabled)
    {
        const bool changed = m_colliderConfigOverridesEnabled != enabled;
        m_colliderConfigOverridesEnabled = enabled;

        std::ostringstream oss;
        oss << kLogPrefix << "コライダー設定の上書きは " << (enabled ? "有効" : "無効")
            << " "
            << (enabled ? "生成済みがあっても（強制生成が有効でなければ）そっちに差し替えるよ "
                : "生成コライダーがない／オフのときだけ設定を使うよ ")
            << "\n";
        DebugTrace(oss.str());

        if (!changed)
        {
            DebugTrace(std::string(kLogPrefix) + "コライダー設定の上書きフラグは前と変わらないよ \n");
        }
    }

    void SweetsManager::SetSpawnOrigins(const std::vector<float3>& origins, std::optional<float> spawnHeightOffset)
    {
        m_spawnOrigins = origins;
        m_warnedMissingSpawnOrigins = m_spawnOrigins.empty();
        if (spawnHeightOffset.has_value())
        {
            m_spawnHeightOffset = *spawnHeightOffset;
        }
        const auto seed = std::random_device{}();
        m_random.seed(seed);


        std::ostringstream oss;
        oss << kLogPrefix << "スポーン元を " << m_spawnOrigins.size()
            << " 箇所設定したよ (heightOffset=" << m_spawnHeightOffset
            << " seed=" << seed << ") \n";
        DebugTrace(oss.str());
    }

    void SweetsManager::SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees)
    {
        m_randomRotationMinDegrees = minDegrees;
        m_randomRotationMaxDegrees = maxDegrees;

        std::ostringstream oss;
        oss << kLogPrefix << "スポーン回転の範囲を更新したよ minYawPitchRollDeg="
            << minDegrees.x << ',' << minDegrees.y << ',' << minDegrees.z
            << " maxYawPitchRollDeg=" << maxDegrees.x << ',' << maxDegrees.y << ','
            << maxDegrees.z << " (deterministic midpoint適用中)\n";
        DebugTrace(oss.str());
    }

    void SweetsManager::SetLiveSweetsCap(int cap)
    {
        const int sanitizedCap = std::max(0, cap);
        const bool changed = (sanitizedCap != m_liveSweetsCap);
        m_liveSweetsCap = sanitizedCap;
        const bool capActive = (m_liveSweetsCap > 0);

        std::ostringstream oss;
        oss << kLogPrefix << "同時出現上限は " << (sanitizedCap > 0 ? "設定" : "無効")
            << " (cap=" << sanitizedCap << ", queued=" << m_spawnQueue.size()
            << ", liveCount=" << m_lastLiveSweetsCount << ") にしたよ\n";
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
            oss << kLogPrefix << "保留中のスポーンキューをクリアするね (" << m_spawnQueue.size() << ") \n";
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
        const int liveSweets = CountLiveSweets();
        const bool liveCapActive = (m_liveSweetsCap > 0);
        const bool spawnBlockedByCap = liveCapActive && liveSweets >= m_liveSweetsCap;

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
        const int simultaneousTarget = std::max(1, config.simultaneousLiveTarget);

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
            m_spawnQueue.clear();
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
        }

        if (spawnBlockedByCap)
        {
            m_burstTimer = std::min(m_activeBurstInterval, m_burstTimer + deltaSeconds);
            return;
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
            }

            if (m_pendingTopUpAfterDrop)
            {
                m_pendingTopUpAfterDrop = false;
            }

            EmitSpawnRequests(deltaSeconds);
            m_lastLiveSweetsCount = CountLiveSweets();
            return;
        }

        if (liveAtTarget && forceSeedNewBatch)
        {
            if (m_pendingTopUpAfterDrop)
            {
                m_pendingTopUpAfterDrop = false;
            }
        }

        if (projectedLive >= desiredLiveTarget && !forceSeedNewBatch)
        {
            if (!m_batchInitialSpawnDone)
            {
                m_batchInitialSpawnDone = true;
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

        int enqueued = 0;
        for (int i = 0; i < allowedCount; ++i)
        {
            if (m_spawnQueue.size() >= kMaxSpawnQueueEntries)
            {
                break;
            }

            const std::string chosenKind = ChooseKindFromActiveConfig();
            const std::string resolvedKind = ResolveKindKey(chosenKind);
            const std::string kind = NormalizeKey(resolvedKind);
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
            oss << kLogPrefix << "スポーンキューへの積み込みが足りなかったよ enqueued=" << enqueued
                << " expected=" << allowedCount << " queueSize=" << m_spawnQueue.size() << ".\n";
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
        while (!m_spawnQueue.empty() && m_burstTimer >= m_activeBurstInterval)
        {
            m_burstTimer -= m_activeBurstInterval;
            const std::string requestedKind = m_spawnQueue.front();
            const std::string resolvedKind = ResolveKindKey(requestedKind);
            const std::string kind = NormalizeKey(resolvedKind);
            m_spawnQueue.pop_front();

            if (m_currentBurstRemaining > 0)
            {
                --m_currentBurstRemaining;
            }

            const std::vector<float3> emitOrigins = m_spawnOrigins.empty()
                ? std::vector<float3>{ NextSpawnOrigin() }
                : m_spawnOrigins;
            for (std::size_t originIndex = 0; originIndex < emitOrigins.size(); ++originIndex)
            {
                const float3& origin = emitOrigins[originIndex];
                Transform transform = BuildSpawnTransform(origin);
                Spawn(kind, transform);
            }
        }

        if (m_currentBurstRemaining < 0)
        {
            m_currentBurstRemaining = 0;
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
            oss << kLogPrefix << "スポーン候補選びでスイーツタイプ '" << ToString(*chosenType)
                << "' のデスクリプタを見つけられなかったよ \n";
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
        oss << kLogPrefix << "フェーズ変更で次のバッチを仕込んだよ interval="
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
            DebugTrace(std::string(kLogPrefix) + "デフォルトのスポーン設定を作れないよ スイーツが何も登録されてない \n");
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
        oss << kLogPrefix << "デフォルトのスポーン設定を適用したよ (" << m_phaseConfigs.size() << " phases) \n";
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
            /*else if (m_warnedMissingFactoryPresetKinds.insert(entry.first).second)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "種類 '" << entry.first
                    << "' に工場プリセットが見つからなかったから、デフォルトウェイト=" << spawnWeight << " で代用するね \n";
                DebugTrace(warn.str());
            }*/ // ここは足りないプリセットに気付いたとき用のデバッグ痕跡

            if (spawnWeight <= 0.0f)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "種類 '" << entry.first
                    << "' のデフォルトウェイトが " << spawnWeight << "<= 0 だったのでスキップするね \n";
                DebugTrace(warn.str());
                continue;
            }

            const auto resolvedType = manifest.Resolve(entry.first);
            if (!resolvedType)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "知らないスイーツキー '" << entry.first
                    << "' のデフォルトウェイトはスキップするよ \n";
                DebugTrace(warn.str());
                continue;
            }

            std::ostringstream resolved;
            resolved << kLogPrefix << "[SpawnWeights] key='" << entry.first << "' -> type="
                << ToString(*resolvedType) << " weight=" << spawnWeight << "\n";
            Diagnostics::LogInfo(resolved.str());

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
                DebugTrace(std::string(kLogPrefix) + "スポーン元リストが空だから、ワールド原点を使うね \n");
                m_warnedMissingSpawnOrigins = true;
            }
            return float3{ 0.0f, 0.0f, 0.0f };
        }

        std::uniform_int_distribution<std::size_t> dist(0, m_spawnOrigins.size() - 1);
        const std::size_t chosenIndex = dist(m_random);
        const float3 origin = m_spawnOrigins[chosenIndex];

        std::ostringstream oss;
        oss << kLogPrefix << "スポーン元インデックス=" << chosenIndex
            << " position=(" << origin.x << ',' << origin.y << ',' << origin.z
            << ") / 設定済み " << m_spawnOrigins.size() << " 箇所から選んだよ \n";
        DebugTrace(oss.str());

        return origin;
    }
   
    Transform SweetsManager::BuildSpawnTransform(const float3& origin) const
    {
        Transform transform{};
        transform.position = origin;
        static bool s_flipSpawnSign = false;
        s_flipSpawnSign = !s_flipSpawnSign;
        std::uniform_real_distribution<float> xJitterDist(1.0f, 1.5f);
        const float xJitter = (s_flipSpawnSign ? 1.0f : -1.0f) * xJitterDist(m_random);
        transform.position.x += xJitter;
        transform.position.y += m_spawnHeightOffset;
        transform.rotation = float3{ 0.0f, 0.0f, 0.0f };
        transform.scale = float3{ 1.0f, 1.0f, 1.0f };

        const auto computeDeterministicRotation = [](float minDeg, float maxDeg)
        {
            const float lo = std::min(minDeg, maxDeg);
            const float hi = std::max(minDeg, maxDeg);
            return (lo + hi) * 0.5f;
        };

        const float sampledPitch = computeDeterministicRotation(m_randomRotationMinDegrees.x, m_randomRotationMaxDegrees.x);
        const float sampledYaw = computeDeterministicRotation(m_randomRotationMinDegrees.y, m_randomRotationMaxDegrees.y);
        const float sampledRoll = computeDeterministicRotation(m_randomRotationMinDegrees.z, m_randomRotationMaxDegrees.z);
        transform.rotation = float3{ sampledPitch, sampledYaw, sampledRoll };

        std::ostringstream oss;
        oss << kLogPrefix << "スポーン回転を決めたよ pitch/yaw/roll(deg)="
            << sampledPitch << ',' << sampledYaw << ',' << sampledRoll
            << " origin=" << origin.x << ',' << origin.y << ',' << origin.z << "\n";
        DebugTrace(oss.str());

        return transform;
    }
}
