#include "Sweets/SweetsFactory.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>

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

#include "AABB.h"
#include "BoxColliderComponent.h"
#include "Collision/ConvexHullBuilder.h"
#include "ConfigSectionReader.h"
#include "Diagnostics/Logger.h"
#include "DirectX.h"
#include "GameplayConfig.h"
#include "MeshBuffer.h"
#include "ModelCache.h"
#include "ModelPivot.h"
#include "Render/ModelBounds.h"
#include "Physics/ShapeConstants.h"
#include "Sweets/BaseSweets.h"
#include "Systems/PhysicsMachine.h"

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
    constexpr float kDefaultGeneratedThicknessPadding = 1.1f;
    constexpr float kMinimumColliderThickness = std::max(Physics::kMinimumShapeDimensionMeters, 1e-3f);
    constexpr const char* kGameplayConfigPath = GameplayConfig::kConfigPath;

    const char* ToString(SweetsSpace::ColliderShape shape)
    {
        switch (shape)
        {
        case SweetsSpace::ColliderShape::Box:
            return "Box";
        case SweetsSpace::ColliderShape::Capsule:
            return "Capsule";
        case SweetsSpace::ColliderShape::Circle:
            return "Circle";
        default:
            return "Unknown";
        }
    }

    std::string ResolveManifestKeyAlias(const std::string& key)
    {
        const auto& manifest = SweetsSpace::SweetsPrototypeManifest::Instance();
        if (const auto resolvedType = manifest.Resolve(key))
        {
            if (const auto* descriptor = manifest.Find(*resolvedType))
            {
                return descriptor->key;
            }
        }

        return key;
    }

    void ReportInvalidBox2DConfig(SweetsSpace::SweetsType type, const std::string& reason)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[ConfigError] type='" << ToString(type) << "' " << reason << "\n";
        Diagnostics::LogInfo(oss.str());

        assert(false && "Invalid Box2DShapeConfig for sweets type");
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

    Physics::Box2DShapeConfig BuildBaseShapeConfig(SweetsSpace::SweetsType type)
    {
        Physics::Box2DShapeConfig config{};
        config.allowRotation = true;
        config.angularDamping = 0.2f;

        const auto registryDefinition = Physics::ColliderRegistry::Instance().Find(ToString(type));
        if (registryDefinition)
        {
            config.type = InferShapeType(registryDefinition->shapes);
            config.density = std::max(0.0f, registryDefinition->shapeDef.density);
            config.friction = std::max(0.0f, registryDefinition->shapeDef.material.friction);
            config.restitution = std::clamp(registryDefinition->shapeDef.material.restitution, 0.0f, 1.0f);
            config.linearDamping = std::max(0.0f, registryDefinition->linearDamping);
        }
        else
        {
            ReportInvalidBox2DConfig(type, "is missing a collider registry definition");
        }

        if (config.type == Physics::Box2DShapeConfig::Type::None)
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

    bool ValidateBaseShapeDefinition(SweetsSpace::SweetsType type)
    {
        const auto registryDefinition = Physics::ColliderRegistry::Instance().Find(ToString(type));
        if (!registryDefinition)
        {
            ReportInvalidBox2DConfig(type, "cannot resolve collider registry entry");
            return false;
        }

        if (registryDefinition->shapes.empty())
        {
            ReportInvalidBox2DConfig(type, "collider registry entry is missing shapes");
            return false;
        }

        for (const auto& shape : registryDefinition->shapes)
        {
            switch (shape.type)
            {
            case Physics::ColliderShapeType::Circle:
                if (!(shape.radius > 0.0f))
                {
                    ReportInvalidBox2DConfig(type, "collider registry circle has non-positive radius");
                    return false;
                }
                break;
            case Physics::ColliderShapeType::Box:
            case Physics::ColliderShapeType::Polygon:
                if (shape.vertices.size() < 3)
                {
                    ReportInvalidBox2DConfig(type, "collider registry polygon/box is missing vertices");
                    return false;
                }
                break;
            default:
                break;
            }
        }

        return InferShapeType(registryDefinition->shapes) != Physics::Box2DShapeConfig::Type::None;
    }

    float2 ScaleColliderOffset(const float2& offset, const float3& scale)
    {
        return float2{ offset.x * scale.x, offset.y * scale.y };
    }

    struct MeshHullSummary
    {
        std::vector<float2> hull;
        float2 centroidOffset{ 0.0f, 0.0f };
        float thickness{ 0.0f };
        float rawThickness{ 0.0f };
        float paddingFactor{ 1.0f };
        ModelBounds bounds{};
    };

    float SanitizeThicknessPadding(float value)
    {
        if (!(value >= 1.0f))
        {
            return 1.0f;
        }
        return value;
    }

    float SanitizeMinThickness(float thickness)
    {
        const float sanitizedMin = std::max(Physics::kMinimumShapeDimensionMeters, 1e-3f);
        if (!(thickness > 0.0f))
        {
            return sanitizedMin;
        }

        return std::max(thickness, sanitizedMin);
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
                const float area = ((simplified[next].x - simplified[i].x) * (simplified[prev].y - simplified[i].y))
                    - ((simplified[next].y - simplified[i].y) * (simplified[prev].x - simplified[i].x));
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

        const std::string normalizedKind = SweetsSpace::NormalizeKey(path);
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
        const float geometryThickness = (maxZ >= minZ)
            ? std::max(maxZ - minZ, 0.0f)
            : 0.0f;
        summary.rawThickness = std::max(geometryThickness, minThickness);
        summary.thickness = summary.rawThickness * sanitizedPadding;

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

        return summary;
    }

    float2 ComputeColliderCenterFromModel(const std::shared_ptr<Render::Model>& model)
    {
        if (!model)
        {
            return float2{ 0.0f, 0.0f };
        }

        const std::string& modelPath = model->GetPath();
        if (!modelPath.empty())
        {
            const MeshHullSummary summary = AnalyzeModelGeometry(
                modelPath,
                kDefaultGeneratedThicknessPadding,
                kMinimumColliderThickness);
            const float2 center = summary.centroidOffset;
            if (std::isfinite(center.x) && std::isfinite(center.y))
            {
                return center;
            }
        }

        return float2{ 0.0f, 0.0f };
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

    bool ParseFloatList(const std::string& value, std::vector<float>& outValues)
    {
        outValues.clear();
        std::stringstream stream(value);
        std::string token;
        while (std::getline(stream, token, ','))
        {
            token = TrimToken(token);
            if (token.empty())
            {
                continue;
            }

            float parsed = 0.0f;
            if (!TryParseFloat(token, &parsed))
            {
                return false;
            }
            outValues.push_back(parsed);
        }

        return !outValues.empty();
    }

    bool ParseFloat3Value(const std::string& value, float3& outValue)
    {
        std::vector<float> components;
        if (!ParseFloatList(value, components))
        {
            return false;
        }

        if (components.size() == 1)
        {
            outValue = float3{ components[0], components[0], components[0] };
            return true;
        }

        if (components.size() == 2)
        {
            outValue = float3{ components[0], components[1], components[1] };
            return true;
        }

        outValue = float3{ components[0], components[1], components[2] };
        return true;
    }
}

namespace SweetsSpace
{
    SweetsFactory::SweetsFactory(Systems::PhysicsMachine& physicsMachine)
        : m_physicsMachine(&physicsMachine)
        , m_generatedColliderThicknessPadding(SanitizeThicknessPadding(kDefaultGeneratedThicknessPadding))
        , m_generatedColliderMinThickness(SanitizeMinThickness(Physics::kMinimumShapeDimensionMeters))
    {
        if (!Physics::ColliderRegistry::Instance().LoadFromFile(kGameplayConfigPath))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[ConfigError] collider registry '" << kGameplayConfigPath
                << "' を読み込めなかったからデフォルト形状に頼るかもしれないよ\n";
            Diagnostics::LogInfo(oss.str());
        }

        Render::ModelCache& cache = Render::ModelCache::Instance();
        const auto& manifest = SweetsPrototypeManifest::Instance();

        bool factoryParseError = false;
        const auto factoryOverrides = LoadFactoryPresets(&factoryParseError);
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
                << " scale=(" << preset.scale.x << ',' << preset.scale.y << ',' << preset.scale.z << ')'
                << " collider=" << ToString(preset.colliderPreset.shape)
                << " padding=(" << preset.colliderPreset.padding.x << ',' << preset.colliderPreset.padding.y << ','
                << preset.colliderPreset.padding.z << ")"
                << " overrideParseError=" << std::boolalpha << factoryParseError << "\n";
            DebugTrace(presetLog.str());

            const std::string registeredPath = descriptorPath.empty() ? std::string("<missing>") : descriptorPath;
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
                        << "'" << aliasSuffix << " のプロトタイプ説明が見当たらないからスポーンをスキップするね\n";
                    DebugTrace(warn.str());
                    return nullptr;
                }

                if (descriptorPath.empty())
                {
                    std::ostringstream warn;
                    warn << kLogPrefix << "種類 '" << descriptorKey << "' のパスが空だったからスポーンをスキップするね\n";
                    DebugTrace(warn.str());
                    return nullptr;
                }

                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "種類 '" << descriptorKey << "' のプロトタイプを解決したよ"
                        << aliasSuffix << " path='" << descriptor->modelPath << "'.\n";
                    DebugTrace(oss.str());
                }

                const std::string primaryPath = descriptorPath;
                auto model = cache.Get(primaryPath);

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
                physicsTransform.scale = preset.scale;
                const float2 colliderCenter = ComputeColliderCenterFromModel(model);
                const float2 modelColliderOffset = ScaleColliderOffset(colliderCenter, physicsTransform.scale);
                float2 colliderOffset{ 0.0f, 0.0f };
                VisualTransform visualTransform{};
                visualTransform.position = physicsTransform.position;
                visualTransform.rotation = physicsTransform.rotation;
                visualTransform.scale = physicsTransform.scale;

                const bool allowMeshPhysics = descriptor->allowMeshPhysics;

                auto shapeConfig = BuildBaseShapeConfig(type);

                Physics::BoxColliderComponent colliderComponent{};
                std::string colliderLookupId = resolvedKind;
                auto registryDefinition = Physics::ColliderRegistry::Instance().Find(colliderLookupId);
                if (!registryDefinition && resolvedKind != ToString(type))
                {
                    colliderLookupId = ToString(type);
                    registryDefinition = Physics::ColliderRegistry::Instance().Find(colliderLookupId);
                }

                if (!registryDefinition)
                {
                    colliderOffset = modelColliderOffset;
                }

                shapeConfig.offset = b2Vec2{ colliderOffset.x, colliderOffset.y };
                colliderComponent.colliderOffset = b2Vec2{ colliderOffset.x, colliderOffset.y };

                if (registryDefinition)
                {
                    colliderComponent.colliderId = colliderLookupId;
                    colliderComponent.shapes = registryDefinition->shapes;
                    colliderComponent.shapeDef = registryDefinition->shapeDef;

                    shapeConfig.type = InferShapeType(registryDefinition->shapes);
                    shapeConfig.density = std::max(0.0f, registryDefinition->shapeDef.density);
                    shapeConfig.friction = std::max(0.0f, registryDefinition->shapeDef.material.friction);
                    shapeConfig.restitution = std::clamp(registryDefinition->shapeDef.material.restitution, 0.0f, 1.0f);
                }
                else
                {
                    colliderComponent.colliderId = resolvedKind;
                    if (m_warnedMissingColliderDefinitionKinds.insert(resolvedKind).second)
                    {
                        std::ostringstream warn;
                        warn << kLogPrefix << "colliderId='" << resolvedKind
                            << "' のレジストリエントリが見つからないから、フィクスチャの構築はスキップされるかも\n";
                        DebugTrace(warn.str());
                    }
                }

                auto sweets = MakeSweets<ActiveSweets>(kind, model, physicsTransform, shapeConfig, Collision::CollisionLayer::Sweets,
                    Core::UpdatePhase::Physics, 0, allowMeshPhysics);

                if (sweets)
                {
                    const std::string colliderIdForLog = colliderComponent.colliderId;
                    sweets->SetBoxColliderComponent(std::move(colliderComponent));
                    sweets->SetVisualTransform(visualTransform);

#if defined(_DEBUG)
                    std::ostringstream colliderLog;
                    colliderLog << kLogPrefix << "[SpawnCollider] tag='" << sweets->GetTag()
                        << "' bodyPos=(" << physicsTransform.position.x << ',' << physicsTransform.position.y << ',' << physicsTransform.position.z
                        << ") colliderId='" << colliderIdForLog << "'"
                        << "\n";
                    DebugTrace(colliderLog.str());
#endif

                    std::ostringstream created;
                    created << kLogPrefix << "[SpawnRender] tag='" << sweets->GetTag()
                        << "' renderPos=(" << visualTransform.position.x << ',' << visualTransform.position.y << ',' << visualTransform.position.z
                        << ") renderRotDeg=(" << visualTransform.rotation.x << ',' << visualTransform.rotation.y << ',' << visualTransform.rotation.z
                        << ") renderScale=(" << visualTransform.scale.x << ',' << visualTransform.scale.y << ',' << visualTransform.scale.z
                        << " colliderId='" << resolvedKind << "'"
                        << " policy=PhysicsAligned (OriginBottom disabled for sweets)\n";
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
    }

    std::shared_ptr<ActiveSweets> SweetsFactory::Create(const std::string& kind, const Transform& transform)
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

        return sweets;
    }

    std::vector<std::pair<std::string, FactoryPreset>> SweetsFactory::EnumerateFactoryPresets() const
    {
        std::vector<std::pair<std::string, FactoryPreset>> presets;
        presets.reserve(m_factoryPresets.size());
        for (const auto& entry : m_factoryPresets)
        {
            presets.push_back(entry);
        }
        return presets;
    }

    const std::map<std::string, FactoryPreset>& SweetsFactory::GetFactoryPresets() const
    {
        return m_factoryPresets;
    }

    std::optional<FactoryPreset> SweetsFactory::FindFactoryPreset(const std::string& normalizedKind) const
    {
        auto it = m_factoryPresets.find(normalizedKind);
        if (it == m_factoryPresets.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    const SweetsFactory::SpawnerMap& SweetsFactory::GetSpawners() const
    {
        return m_spawner;
    }

    std::string SweetsFactory::ResolveKindKey(const std::string& key) const
    {
        return ResolveManifestKeyAlias(key);
    }

    void SweetsFactory::SetGeneratedColliderThicknessPadding(float factor)
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

    void SweetsFactory::SetGeneratedColliderMinThickness(float thickness)
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

    void SweetsFactory::SetForceGeneratedColliders(bool force)
    {
        m_forceGeneratedColliders = force;
        std::ostringstream oss;
        oss << kLogPrefix << "コライダー強制生成は " << std::boolalpha << m_forceGeneratedColliders << " にしたよ\n";
        DebugTrace(oss.str());
    }

    void SweetsFactory::SetForceConfigColliders(bool force, const std::vector<std::string>& kinds)
    {
        m_forceConfigColliders = force;
        m_forceConfigColliderKinds.clear();

        if (m_forceConfigColliders)
        {
            for (const auto& kind : kinds)
            {
                const std::string resolved = ResolveKindKey(kind);
                if (!resolved.empty())
                {
                    m_forceConfigColliderKinds.insert(NormalizeKey(resolved));
                }
            }
        }

        std::ostringstream oss;
        oss << kLogPrefix << "コライダー設定強制は " << std::boolalpha << m_forceConfigColliders
            << " (kinds=" << m_forceConfigColliderKinds.size() << ") にしたよ\n";
        DebugTrace(oss.str());
    }

    void SweetsFactory::SetColliderConfigOverridesEnabled(bool enabled)
    {
        m_colliderConfigOverridesEnabled = enabled;
        std::ostringstream oss;
        oss << kLogPrefix << "コライダー config 上書きは " << std::boolalpha << m_colliderConfigOverridesEnabled << " にしたよ\n";
        DebugTrace(oss.str());
    }

    std::optional<FactoryPreset> SweetsFactory::BuildDefaultFactoryPreset(SweetsType type)
    {
        FactoryPreset preset{};
        preset.spawnWeight = 0.0f;
        preset.scale = float3{ 1.0f, 1.0f, 1.0f };
        preset.colliderPreset = ColliderPreset{};

        switch (type)
        {
        case SweetsType::Cream:
            preset.spawnWeight = 1.0f;
            preset.scale = float3{ 0.26f, 0.26f, 0.26f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Circle, float3{ 0.003f, 0.003f, 0.003f } };
            return preset;
        case SweetsType::Strawberry:
            preset.spawnWeight = 1.0f;
            preset.scale = float3{ 0.24f, 0.24f, 0.24f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Circle, float3{ 0.01f, 0.01f, 0.01f } };
            return preset;
        case SweetsType::Blueberry:
            preset.spawnWeight = 1.0f;
            preset.scale = float3{ 0.18f, 0.18f, 0.18f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Circle, float3{ 0.008f, 0.008f, 0.008f } };
            return preset;
        case SweetsType::Pineapple:
            preset.spawnWeight = 0.8f;
            preset.scale = float3{ 0.34f, 0.34f, 0.34f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Box, float3{ 0.01f, 0.016f, 0.01f } };
            return preset;
        case SweetsType::Banana:
            preset.spawnWeight = 0.9f;
            preset.scale = float3{ 0.32f, 0.32f, 0.32f };
            preset.colliderPreset = ColliderPreset{ ColliderShape::Capsule, float3{ 0.005f, 0.005f, 0.005f } };
            return preset;
        default:
            return std::nullopt;
        }
    }

    std::map<std::string, FactoryPreset> SweetsFactory::LoadFactoryPresets(bool* parseError)
    {
        std::map<std::string, FactoryPreset> overrides;
        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(kGameplayConfigPath, GameplayConfig::kSectionSweetsFactory, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "工場プリセットの設定が '" << kGameplayConfigPath
                << "' の section '" << GameplayConfig::kSectionSweetsFactory
                << "' に見当たらないので、昔のデフォルトに戻すね\n";
            DebugTrace(oss.str());
            return overrides;
        }

        FactoryPreset preset{};
        std::string presetKey;
        bool hasActive = false;
        const auto flush = [&]()
        {
            if (!hasActive || presetKey.empty())
            {
                return;
            }

            const std::string resolvedKey = ResolveManifestKeyAlias(presetKey);
            const std::string normalizedKey = NormalizeKey(resolvedKey);
            overrides[normalizedKey] = preset;

            std::ostringstream oss;
            oss << kLogPrefix
                << "キー '" << presetKey << "' の工場プリセットを読んだよ"
                << " resolvedKey='" << resolvedKey << "'"
                << " spawnWeight=" << preset.spawnWeight
                << " scale=(" << preset.scale.x << ',' << preset.scale.y << ',' << preset.scale.z << ')'
                << " collider=" << ToString(preset.colliderPreset.shape)
                << " padding=(" << preset.colliderPreset.padding.x << ',' << preset.colliderPreset.padding.y << ','
                << preset.colliderPreset.padding.z << ")\n";
            DebugTrace(oss.str());

            hasActive = false;
        };

        for (const auto& entry : lines)
        {
            const std::string key = entry.key;
            const std::string value = entry.value;

            if (key == "preset" || key == "key")
            {
                flush();
                preset = FactoryPreset{};
                preset.scale = float3{ 1.0f, 1.0f, 1.0f };
                preset.colliderPreset.shape = ColliderShape::Box;
                preset.colliderPreset.padding = float3{ 0.0f, 0.0f, 0.0f };
                presetKey = value;
                hasActive = true;
                continue;
            }

            if (!hasActive)
            {
                continue;
            }

            bool localError = false;
            if (key == "spawnWeight")
            {
                localError |= !TryParseFloat(value, &preset.spawnWeight);
                preset.spawnWeight = std::max(0.0f, preset.spawnWeight);
            }
            else if (key == "scale")
            {
                localError |= !ParseFloat3Value(value, preset.scale);
            }
            else if (key == "padding")
            {
                localError |= !ParseFloat3Value(value, preset.colliderPreset.padding);
            }
            else if (key == "colliderPreset" || key == "colliderShape")
            {
                const std::string colliderShape = NormalizeKey(value);
                if (colliderShape == "capsule")
                {
                    preset.colliderPreset.shape = ColliderShape::Capsule;
                }
                else if (colliderShape == "circle")
                {
                    preset.colliderPreset.shape = ColliderShape::Circle;
                }
                else if (colliderShape == "box" || colliderShape.empty())
                {
                    preset.colliderPreset.shape = ColliderShape::Box;
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "" << entry.lineNumber << "行目のコライダープリセット '" << value
                        << "' が知らない形だったから、とりあえず Box にしとくね\n";
                    DebugTrace(oss.str());
                    preset.colliderPreset.shape = ColliderShape::Box;
                }
            }

            if (localError && parseError)
            {
                *parseError = true;
            }
        }

        flush();

        return overrides;
    }

    std::map<std::string, Collision::ColliderDefinition>::const_iterator SweetsFactory::FindColliderDefinition(const std::string& key) const
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

    ColliderBuildResult SweetsFactory::BuildColliderForSweets(const std::string& kind,
        SweetsType type,
        const Transform& transform,
        const std::string& modelPath,
        const std::shared_ptr<Render::Model>& model,
        const float2& colliderCenter,
        bool allowMeshPhysics)
    {
        (void)kind;
        (void)modelPath;
        (void)model;
        (void)allowMeshPhysics;

        ColliderBuildResult result{};
        result.shapeConfig = BuildBaseShapeConfig(type);
        const float2 scaledCenter = ScaleColliderOffset(colliderCenter, transform.scale);
        result.shapeConfig.offset = b2Vec2{ scaledCenter.x, scaledCenter.y };
        result.source = ColliderSource::Config;
        result.centroidOffset = scaledCenter;
        return result;
    }
}
