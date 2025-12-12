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
    using ::SweetsSpace::SweetsTypeHash;

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
            DebugTrace(std::string(kLogPrefix) + "Reusing cached procedural fairy.\n");
            return reused;
        }

        ID3D11Device* device = GetDevice();
        if (!device)
        {
            DebugTrace(std::string(kLogPrefix) + "Cannot create procedural fairy: device missing.\n");
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
            return std::numeric_limits<float>::min();
        }

        return std::max(value, std::numeric_limits<float>::min());
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

    bool HasMeaningfulDifference(float previous, float current, float epsilon = kHorizontalBoundsEpsilon)
    {
        if (!std::isfinite(previous) || !std::isfinite(current))
        {
            return true;
        }

        return std::fabs(previous - current) > epsilon;
    }

    struct ColliderBuildResult
    {
        Box2DShapeConfig shapeConfig{};
        std::optional<Physics::BoxColliderComponent> colliderComponent{};
        std::vector<std::vector<b2Vec2>> meshPolygons;
        std::vector<b2Vec2> meshBoundary;
    };

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
            config.angularDamping = 0.85f;
            config.density = 0.85f;
            config.friction = 0.5f;
            config.restitution = 0.12f;
            break;
        case SweetsType::Blueberry:
            config.type = Box2DShapeConfig::Type::Circle;
            config.radius = 0.35f;
            config.angularDamping = 0.7f;
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
        default:
            config.type = Box2DShapeConfig::Type::Box;
            config.angularDamping = 0.9f;
            config.density = 0.9f;
            config.friction = 0.6f;
            config.restitution = 0.05f;
            break;
        }

        if (!config.allowRotation)
        {
            config.fixedRotation = true;
            config.angularDamping = std::max(config.angularDamping, 6.0f);
        }

        return config;
    }

    void ApplyMaterialToCollider(Physics::BoxColliderComponent& component, const Box2DShapeConfig& shapeConfig)
    {
        component.shapeDef.density = shapeConfig.density;
        component.shapeDef.material.friction = shapeConfig.friction;
        component.shapeDef.material.restitution = shapeConfig.restitution;
    }

    ColliderBuildResult BuildColliderForSweets(SweetsType type, const Transform& transform, const std::shared_ptr<Render::Model>& model)
    {
        ColliderBuildResult result{};
        result.shapeConfig = BuildBaseShapeConfig(type);

        Physics::BoxColliderComponent colliderComponent{};
        ApplyMaterialToCollider(colliderComponent, result.shapeConfig);

        // Keep collider vertices in local space; PhysicsMachine::CreateBody applies the sweets transform scale when creating fixtures.

        switch (type)
        {
        case SweetsType::Cake:
            colliderComponent.AddBox(0.5f, 0.5f);
            result.colliderComponent = colliderComponent;
            break;
        case SweetsType::Pudding:
            colliderComponent.AddPolygon({
                b2Vec2{ -0.5f, -0.45f },
                b2Vec2{ 0.5f, -0.45f },
                b2Vec2{ 0.0f, 0.55f },
            });
            result.colliderComponent = colliderComponent;
            break;
        case SweetsType::Sherbet:
            colliderComponent.AddCircle(0.5f);
            result.colliderComponent = colliderComponent;
            break;
        case SweetsType::Strawberry:
            colliderComponent.AddCircle(0.45f);
            result.colliderComponent = colliderComponent;
            break;
        case SweetsType::Blueberry:
            colliderComponent.AddCircle(0.35f);
            result.colliderComponent = colliderComponent;
            break;
        case SweetsType::Pineapple:
            colliderComponent.AddBox(0.32f, 0.6f);
            result.colliderComponent = colliderComponent;
            break;
        case SweetsType::Banana:
            colliderComponent.AddPolygon({
                b2Vec2{ -0.6f, -0.08f },
                b2Vec2{ 0.6f, 0.08f },
                b2Vec2{ 0.55f, 0.24f },
                b2Vec2{ -0.45f, 0.24f },
            });
            result.colliderComponent = colliderComponent;
            break;
        default:
            if (model && model->HasLocalBounds())
            {
                const AABB& bounds = model->GetLocalBounds();
                const float minX = std::min(bounds.min.x, bounds.max.x);
                const float maxX = std::max(bounds.min.x, bounds.max.x);
                const float minY = std::min(bounds.min.y, bounds.max.y);
                const float maxY = std::max(bounds.min.y, bounds.max.y);

                std::vector<b2Vec2> polygon = {
                    b2Vec2{ minX, minY },
                    b2Vec2{ maxX, minY },
                    b2Vec2{ maxX, maxY },
                    b2Vec2{ minX, maxY },
                };

                result.meshBoundary = polygon;
                result.meshPolygons.emplace_back(std::move(polygon));
            }
            break;
        }

        const float scaleFactor = transform.scale.x * transform.scale.y * transform.scale.z;
        result.shapeConfig.density *= scaleFactor;
        if (result.colliderComponent)
        {
            result.colliderComponent->shapeDef.density = result.shapeConfig.density;
            result.colliderComponent->shapeDef.material.friction = result.shapeConfig.friction;
            result.colliderComponent->shapeDef.material.restitution = result.shapeConfig.restitution;
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

        bool touching = manager.HasSupportingContact(sweets);
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
            return ::SweetsSpace::SweetsState::Stacked;
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
            oss << kLogPrefix << "Failed to load model for geometry analysis path='" << path
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

                // aiProcess_Triangulate guarantees triangles
                indices.push_back(baseIndex + face.mIndices[0]);
                indices.push_back(baseIndex + face.mIndices[1]);
                indices.push_back(baseIndex + face.mIndices[2]);
            }
        }

        if (positions.empty())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Geometry analysis found no positions for '" << path << "'.\n";
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
        oss << kLogPrefix << "Analyzed geometry path='" << path
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
            debugHull << kLogPrefix << "Hull points (centroid-relative)=";
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

    bool SweetsManager::HasSupportingContact(const ActiveSweets& sweets) const
    {
        auto it = m_contactStates.find(const_cast<ActiveSweets*>(&sweets));
        if (it == m_contactStates.end())
        {
            return false;
        }

        return (it->second.foundationContacts > 0) || (it->second.sweetsContacts > 0);
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
            oss << kLogPrefix << "Factory preset row on line " << lineNumber
                << " had " << tokens.size() << " tokens; expected at least 11.\n";
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
            oss << kLogPrefix << "Unknown collider preset '" << tokens[10]
                << "' on line " << lineNumber << "; defaulting to Box.\n";
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

            const std::string resolvedKey = ResolveManifestKeyAlias(tokens[0]);
            const std::string normalizedKey = NormalizeKey(resolvedKey);
            overrides[normalizedKey] = preset;

            std::ostringstream oss;
            oss << kLogPrefix << "Loaded factory preset for key '" << tokens[0]
                << "' resolvedKey='" << resolvedKey << "'"
                << "' spawnWeight=" << preset.spawnWeight
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
        , m_loggedContactNotification(false)
        , m_warnedMissingColliderDefinitionKinds()
        , m_warnedMissingFactoryPresetKinds()
        , m_warnedMissingGeneratedHull(false)
        , m_hasHorizontalBounds(false)
        , m_minHorizontalBound(0.0f)
        , m_maxHorizontalBound(0.0f)
        , m_horizontalBoundsMargin(kHorizontalOutOfBoundsMargin)
        , m_generatedColliderThicknessPadding(SanitizeThicknessPadding(kDefaultGeneratedThicknessPadding))
        , m_generatedColliderMinThickness(SanitizeMinThickness(0.0f))
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
                    oss << kLogPrefix << "Collider definition key normalized from '" << entry.first
                        << "' to '" << normalizedKey << "'.\n";
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
                warn << kLogPrefix << "No factory preset available for kind '" << kind
                    << "' (type=" << ToString(prototype.type) << "); skipping registration.\n";
                DebugTrace(warn.str());
                continue;
            }

            FactoryPreset preset = hasOverride ? overrideIt->second : *defaultPreset;
            m_factoryPresets[normalizedKind] = preset;

            std::ostringstream presetLog;
            presetLog << kLogPrefix << "Resolved factory preset for kind '" << resolvedKind;
            if (isAliasedKind)
            {
                presetLog << "' (from key '" << kind << "')";
            }
            presetLog << "' source=" << (hasOverride ? "config" : "fallback")
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
            oss << kLogPrefix << "Registering sweets kind '" << kind
                << "' with asset '" << registeredPath << "'.\n";
            DebugTrace(oss.str());

            const SweetsType type = prototype.type;

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
                    warn << kLogPrefix << "Prototype descriptor missing for kind '" << resolvedKind
                        << "'" << aliasSuffix << "; using placeholder path '" << kPlaceholderModelPath << "'.\n";
                    DebugTrace(warn.str());
                    descriptorPath = kPlaceholderModelPath;
                }
                else if (descriptorPath.empty())
                {
                    std::ostringstream warn;
                    warn << kLogPrefix << "Descriptor for kind '" << descriptorKey << "' path empty;"
                        << " falling back to placeholder '" << kPlaceholderModelPath << "'.\n";
                    DebugTrace(warn.str());
                    descriptorPath = kPlaceholderModelPath;
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Resolved prototype for kind '" << descriptorKey << "'"
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
                        warn << kLogPrefix << "Model load failed for sweets kind '" << descriptorKey
                            << "' path='" << primaryPath << "'; trying placeholder '" << kPlaceholderModelPath
                            << "'.\n";
                        DebugTrace(warn.str());
                        model = cache.Get(kPlaceholderModelPath);
                    }
                }

                if (!model)
                {
                    std::ostringstream fail;
                    fail << kLogPrefix << "No model available for sweets kind '" << descriptorKey
                        << "' after fallbacks.\n";
                    DebugTrace(fail.str());

                    return nullptr;
                }

                Transform physicsTransform = transform;
                SweetsVisualTransform visualTransform{};
                visualTransform.position = physicsTransform.position;
                visualTransform.rotation = physicsTransform.rotation;

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

                visualTransform.scale = float3
                {
                    physicsTransform.scale.x * sampledScale.x,
                    physicsTransform.scale.y * sampledScale.y,
                    physicsTransform.scale.z * sampledScale.z
                };

                physicsTransform.scale = visualTransform.scale;

                const float visualOffsetY = Render::ComputePivotOffsetYOriginBottom(model, visualTransform.scale);
                physicsTransform.position.y += visualOffsetY;
                visualTransform.position.y += visualOffsetY;

                const auto colliderBuild = BuildColliderForSweets(type, physicsTransform, model);

                auto sweets = MakeSweets<ActiveSweets>(kind, model, physicsTransform, colliderBuild.shapeConfig, Collision::CollisionLayer::Sweets,
                    Core::UpdatePhase::Physics, 0);

                if (sweets)
                {
                    sweets->SetVisualTransform(visualTransform);

                    std::ostringstream created;
                    created << kLogPrefix << "[SpawnRender] tag='" << sweets->GetTag()
                        << "' renderPos=(" << visualTransform.position.x << ',' << visualTransform.position.y << ',' << visualTransform.position.z
                        << ") renderRotDeg=(" << visualTransform.rotation.x << ',' << visualTransform.rotation.y << ',' << visualTransform.rotation.z
                        << ") renderScale=(" << visualTransform.scale.x << ',' << visualTransform.scale.y << ',' << visualTransform.scale.z
                        << ") pivotOffsetY=" << visualOffsetY
                        << " policy=" << Render::kPivotPolicyName << '\n';
                    Diagnostics::LogInfo(created.str());
                }

                return sweets;
            };

            auto [_, inserted] = m_spawner.emplace(normalizedKind, std::move(factory));
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
            oss << kLogPrefix << "Spawn requested for unknown kind '" << kind
                << "' (resolved='" << resolvedKind
                << "' normalizedRaw='" << normalizedKind
                << "' normalizedResolved='" << normalizedResolvedKind << "'). Registered kinds:";
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

    std::shared_ptr<ActiveSweets> SweetsManager::FindNearestCarryable(const float3& position, float halfWidth, float halfHeight) const
    {
        if (!(halfWidth > 0.0f) || !(halfHeight > 0.0f))
        {
            return nullptr;
        }

        std::shared_ptr<ActiveSweets> best;
        float bestDistanceSq = std::numeric_limits<float>::max();

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

                sweets->SyncColliderBounds();
                const AABB bounds = sweets->GetAABB();
                const float minX = position.x - halfWidth;
                const float maxX = position.x + halfWidth;
                const float minY = position.y - halfHeight;
                const float maxY = position.y + halfHeight;

                const bool overlaps = !(bounds.max.x < minX || bounds.min.x > maxX || bounds.max.y < minY || bounds.min.y > maxY);
                if (!overlaps)
                {
                    continue;
                }

                const float centerX = (bounds.min.x + bounds.max.x) * 0.5f;
                const float centerY = (bounds.min.y + bounds.max.y) * 0.5f;
                const float centerZ = (bounds.min.z + bounds.max.z) * 0.5f;
                const float dx = centerX - position.x;
                const float dy = centerY - position.y;
                const float dz = centerZ - position.z;
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
            const auto& colliderComponent = sweets->GetBoxColliderComponent();
            const bool hasColliderComponent = colliderComponent && !colliderComponent->Empty();
            const auto shapeConfig = sweets->GetBox2DShapeConfig();
            const bool hasShapeConfig = shapeConfig && shapeConfig->type != Physics::Box2DShapeConfig::Type::None;
            const bool hasMeshFixtures = sweets->HasBox2DMesh() || !sweets->GetBox2DBoundary().empty();

            if (!hasColliderComponent && !hasShapeConfig && !hasMeshFixtures)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Skipping physics registration for sweets '" << sweets->GetTag()
                    << "': collider configuration missing.\n";
                DebugTrace(oss.str());
                return;
            }

            const bool alreadyRegistered = m_registeredPhysicsBodies.find(sweets.get()) != m_registeredPhysicsBodies.end()
                || b2Body_IsValid(m_physicsMachine->FindBody(*sweets));
            if (!alreadyRegistered)
            {
                m_physicsMachine->RegisterBody(*sweets);
            }

            const int fixtureCount = colliderComponent ? static_cast<int>(colliderComponent->fixtures.size()) : 0;
            const bool hasBody = sweets->HasBox2DBody();

            if (!hasBody || fixtureCount <= 0)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Physics registration failed for sweets '" << sweets->GetTag()
                    << "' hasBody=" << std::boolalpha << hasBody
                    << " fixtures=" << fixtureCount << "\n";
                DebugTrace(oss.str());
                return;
            }

            m_registeredPhysicsBodies.insert(sweets.get());
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
            const bool tracked = m_registeredPhysicsBodies.erase(sweets.get()) > 0;
            if (tracked || b2Body_IsValid(m_physicsMachine->FindBody(*sweets)))
            {
                m_physicsMachine->UnregisterBody(*sweets);
            }
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
                    if (registeredKindsNormalized.find(normalizedKind) == registeredKindsNormalized.end())
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
                config.weights = defaultWeights;
            }

            if (config.weights.empty())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Skipping phase " << entry.first
                    << " because no valid weights were available even after defaults.\n";
                DebugTrace(oss.str());
                continue;
            }

            EnsureRegisteredWeights(config, entry.first, registeredKindsNormalized, defaultWeightLookup);

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
        warn << kLogPrefix << "[Spawn] Phase " << phaseIndex
            << " missing spawn weights for registered kinds:";
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
            added << kLogPrefix << "[Spawn] Phase " << phaseIndex
                << " auto-added missing kind '" << ToString(missing)
                << "' with fallback weight=" << std::fixed << std::setprecision(2) << fallbackWeight
                << " (autoInclude=" << std::boolalpha << kAutoIncludeMissingWeightKinds << ")\n";
            DebugTrace(added.str());
        }
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

    void SweetsManager::SetGeneratedColliderMinThickness(float thickness)
    {
        const float sanitized = SanitizeMinThickness(thickness);
        const bool clamped = std::fabs(sanitized - thickness) > 1e-4f;
        const bool changed = std::fabs(sanitized - m_generatedColliderMinThickness) > 1e-4f;
        m_generatedColliderMinThickness = sanitized;

        std::ostringstream oss;
        oss << kLogPrefix << "Generated collider minimum thickness set to " << sanitized
            << (clamped ? " (clamped to smallest positive float)" : "")
            << "; new colliders will use flattened thickness.\n";
        DebugTrace(oss.str());

        if (!changed)
        {
            DebugTrace(std::string(kLogPrefix) + "Minimum thickness unchanged from prior value.\n");
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
            oss << kLogPrefix << "Contact begin between '" << a->GetTag() << "' and '" << b->GetTag() << "'.\n";
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
            oss << kLogPrefix << "Contact end between '" << a->GetTag() << "' and '" << b->GetTag() << "'.\n";
            DebugTrace(oss.str());
            m_loggedContactNotification = true;
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
            /*else if (m_warnedMissingFactoryPresetKinds.insert(entry.first).second)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "No factory preset found for kind '" << entry.first
                    << "' while building default weights; using fallback weight=" << spawnWeight << ".\n";
                DebugTrace(warn.str());
            }*/ //̃CoOĂ̂ŃRgAEgi~nCj

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
        static bool s_flipSpawnSign = false;
        s_flipSpawnSign = !s_flipSpawnSign;
        std::uniform_real_distribution<float> xJitterDist(1.0f, 1.5f);
        const float xJitter = (s_flipSpawnSign ? 1.0f : -1.0f) * xJitterDist(m_random);
        transform.position.x += xJitter;
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
