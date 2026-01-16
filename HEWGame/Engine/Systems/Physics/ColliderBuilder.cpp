#include "Systems/Physics/ColliderBuilder.h"

#include "ColliderRegistry.h"
#include "GameplayConfig.h"

#include <algorithm>
#include <limits>
#include <sstream>

namespace
{
    constexpr float kMinGeneratedThickness = 0.05f;

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

    void EmitLog(const Physics::ColliderBuilder::LogFn& log, const std::string& message)
    {
        if (log)
        {
            log(message);
        }
    }

    void EmitBuildSummary(const Physics::ColliderBuilder::LogFn& log,
        const std::string& prefix,
        const std::string& source,
        const std::string& key,
        Physics::Box2DShapeConfig::Type shapeType,
        size_t shapeCount)
    {
        if (!log)
        {
            return;
        }

        auto shapeToString = [](Physics::Box2DShapeConfig::Type type)
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
        oss << prefix << "[ColliderBuilder] key='" << key
            << "' source=" << source
            << " shape=" << shapeToString(shapeType)
            << " shapeCount=" << shapeCount
            << "\n";
        log(oss.str());
    }

    std::optional<std::map<std::string, Collision::ColliderDefinition>> LoadPlayerConfigDefinitions(
        const Physics::ColliderBuilder::LogFn& log,
        const std::string& logPrefix)
    {
        std::map<std::string, Collision::ColliderDefinition> definitions;
        if (Collision::LoadColliderDefinitions(GameplayConfig::kConfigPath, GameplayConfig::kSectionPlayerColliders, definitions))
        {
            return definitions;
        }

        std::ostringstream oss;
        oss << logPrefix << "[ColliderBuilder] Failed to load config definitions for section '"
            << GameplayConfig::kSectionPlayerColliders << "' from '" << GameplayConfig::kConfigPath << "'.\n";
        EmitLog(log, oss.str());
        return std::nullopt;
    }

    Physics::ColliderBuilder::BuildResult BuildFromRegistryInternal(
        const Physics::ColliderBuilder::LogFn& log,
        const std::string& logPrefix,
        const Physics::ColliderBuilder::RegistryBuildRequest& request,
        const SceneGameConfig::PlayerColliderTuning& tuning,
        const std::string& colliderId)
    {
        Physics::ColliderBuilder::BuildResult result{};
        result.source = "RegistryMissing";

        const auto registryDefinition = Physics::ColliderRegistry::Instance().Find(colliderId);
        result.registryHit = registryDefinition.has_value();
        result.registryHasShapes = registryDefinition && !registryDefinition->shapes.empty();

        if (!registryDefinition)
        {
            std::ostringstream oss;
            oss << logPrefix << "[ColliderBuilder] Registry definition missing for id='" << colliderId
                << "'; unable to build collider from registry.\n";
            EmitLog(log, oss.str());
            return result;
        }

        if (!result.registryHasShapes)
        {
            std::ostringstream oss;
            oss << logPrefix << "[ColliderBuilder] Registry collider '" << colliderId
                << "' has no shapes; collider will not be created.\n";
            EmitLog(log, oss.str());
            result.source = "RegistryEmpty";
            return result;
        }

        Physics::BoxColliderComponent collider{};
        collider.colliderId = colliderId;
        collider.shapeDef = registryDefinition->shapeDef;

        const bool applyScale = colliderId != "player";
        const float3 shapeScale = applyScale ? request.transform.scale : float3{ 1.0f, 1.0f, 1.0f };
        collider.shapes.reserve(registryDefinition->shapes.size());
        const float uniformScale = std::max(shapeScale.x, shapeScale.y);
        for (const auto& shape : registryDefinition->shapes)
        {
            Physics::ColliderShapeDescriptor scaledShape{};
            scaledShape.type = shape.type;

            switch (shape.type)
            {
            case Physics::ColliderShapeType::Circle:
            {
                scaledShape.radius = shape.radius * uniformScale;
                if (!shape.vertices.empty())
                {
                    const b2Vec2& center = shape.vertices.front();
                    scaledShape.vertices.push_back(b2Vec2{ center.x * shapeScale.x, center.y * shapeScale.y });
                }
                break;
            }
            case Physics::ColliderShapeType::Box:
            case Physics::ColliderShapeType::Polygon:
            default:
            {
                scaledShape.vertices.reserve(shape.vertices.size());
                for (const auto& vertex : shape.vertices)
                {
                    scaledShape.vertices.push_back(b2Vec2{ vertex.x * shapeScale.x, vertex.y * shapeScale.y });
                }
                break;
            }
            }

            collider.shapes.push_back(std::move(scaledShape));
        }

        const float3 scaledOffset = (request.scaleOffset && applyScale)
            ? ScaleVector(request.offset, request.transform.scale)
            : request.offset;
        collider.colliderOffset = b2Vec2{ scaledOffset.x, scaledOffset.y };

        Physics::Box2DShapeConfig shapeConfig{};
        shapeConfig.bodyType = request.bodyType;
        shapeConfig.fixedRotation = request.fixedRotation;
        shapeConfig.allowRotation = request.allowRotation;
        shapeConfig.angularDamping = request.angularDamping;
        shapeConfig.linearDamping = request.useRegistryLinearDamping
            ? std::max(0.0f, registryDefinition->linearDamping)
            : request.linearDamping;

        FootprintAccumulator footprint{};
        const float paddedThickness = std::max(
            kMinGeneratedThickness,
            (tuning.fallbackSize.z + tuning.fallbackPadding.z * 2.0f) * shapeScale.z);
        footprint.ExpandHalfZ(paddedThickness);

        for (const auto& shape : collider.shapes)
        {
            switch (shape.type)
            {
            case Physics::ColliderShapeType::Circle:
            {
                const float radius = shape.radius;
                const b2Vec2 center = shape.vertices.empty()
                    ? b2Vec2{ 0.0f, 0.0f }
                    : shape.vertices.front();
                footprint.IncludeCircle(center, radius);
                break;
            }
            case Physics::ColliderShapeType::Box:
            case Physics::ColliderShapeType::Polygon:
            default:
            {
                footprint.IncludeVertices(shape.vertices);
                break;
            }
            }
        }

        shapeConfig.type = InferShapeType(collider.shapes);
        if (shapeConfig.type == Physics::Box2DShapeConfig::Type::None)
        {
            std::ostringstream oss;
            oss << logPrefix << "[ColliderBuilder] Registry collider '" << colliderId
                << "' does not declare a supported shape type.\n";
            EmitLog(log, oss.str());
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

    Physics::ColliderBuilder::BuildResult BuildFromConfigInternal(
        const Physics::ColliderBuilder::LogFn& log,
        const std::string& logPrefix,
        const Physics::ColliderBuilder::RegistryBuildRequest& request,
        const SceneGameConfig::PlayerColliderTuning& tuning,
        const std::map<std::string, Collision::ColliderDefinition>& definitions,
        const std::string& resolvedKey)
    {
        Physics::ColliderBuilder::BuildResult result{};

        Physics::Box2DShapeConfig shapeConfig{};
        shapeConfig.type = Physics::Box2DShapeConfig::Type::None;
        shapeConfig.bodyType = request.bodyType;
        shapeConfig.fixedRotation = request.fixedRotation;
        shapeConfig.allowRotation = request.allowRotation;
        shapeConfig.angularDamping = request.angularDamping;
        shapeConfig.linearDamping = request.linearDamping;

        const auto registryDefinition = Physics::ColliderRegistry::Instance().Find(resolvedKey);
        result.registryHit = registryDefinition.has_value();
        result.registryHasShapes = registryDefinition && !registryDefinition->shapes.empty();

        const auto configIt = definitions.find(resolvedKey);
        if (configIt == definitions.end())
        {
            std::ostringstream oss;
            oss << logPrefix << "[ColliderBuilder] Definition missing for key='" << resolvedKey
                << "'; model-based generation is disabled; collider cannot be created from config.\n";
            EmitLog(log, oss.str());
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
        const bool applyScale = resolvedKey != "player";
        const float3 shapeScale = applyScale ? request.transform.scale : float3{ 1.0f, 1.0f, 1.0f };
        const float3 scaledOffset = (request.scaleOffset && applyScale)
            ? ScaleVector(tuning.offset, shapeScale)
            : tuning.offset;
        collider.colliderOffset = b2Vec2{ scaledOffset.x, scaledOffset.y };

        FootprintAccumulator footprint{};
        const auto appendDefinition = [&](const Collision::ColliderDefinition& def, const b2Vec2& parentOffset, float parentScale, const auto& self) -> void
        {
            const float combinedScale = std::max(0.0f, def.scale) * parentScale;
            const b2Vec2 scaledOffset{
                parentOffset.x + def.offset.x * shapeScale.x,
                parentOffset.y + def.offset.y * shapeScale.y
            };

            const float marginX = def.margin2 ? def.margin2->x : (def.margin3 ? def.margin3->x : def.margin);
            const float marginY = def.margin2 ? def.margin2->y : (def.margin3 ? def.margin3->y : def.margin);

            switch (def.type)
            {
            case Collision::ColliderKind::Circle:
            {
                float radius = def.radius * combinedScale * std::max(shapeScale.x, shapeScale.y);
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

                float thickness = (def.thickness + (def.margin3 ? def.margin3->z * 2.0f : 0.0f)) * shapeScale.z;
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
                    float vx = p.x * combinedScale * shapeScale.x;
                    float vy = p.y * combinedScale * shapeScale.y;
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

                float thickness = (def.thickness + (def.margin3 ? def.margin3->z * 2.0f : 0.0f)) * shapeScale.z;
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
            oss << logPrefix << "[ColliderBuilder] Config definition for '" << resolvedKey
                << "' produced no shapes; collider cannot be created.\n";
            EmitLog(log, oss.str());
            result.source = "ConfigEmpty";
            return result;
        }

        shapeConfig.type = InferShapeType(collider.shapes);
        if (shapeConfig.type == Physics::Box2DShapeConfig::Type::None)
        {
            std::ostringstream oss;
            oss << logPrefix << "[ColliderBuilder] Config definition for '" << resolvedKey
                << "' lacks a supported shape type.\n";
            EmitLog(log, oss.str());
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

    Physics::ColliderBuilder::BuildResult BuildFallbackInternal(
        const Physics::ColliderBuilder::RegistryBuildRequest& request,
        const SceneGameConfig::PlayerColliderTuning& tuning,
        const std::string& resolvedKey)
    {
        Physics::ColliderBuilder::BuildResult result{};
        result.source = "Fallback";

        Physics::BoxColliderComponent collider{};
        collider.colliderId = resolvedKey;

        const float3 scaledSize = ScaleVector(tuning.fallbackSize, request.transform.scale);
        const float3 scaledPadding = ScaleVector(tuning.fallbackPadding, request.transform.scale);
        const float3 scaledOffset = ScaleVector(tuning.fallbackOffset, request.transform.scale);

        const float halfWidth = std::max(kMinGeneratedThickness * 0.5f, scaledSize.x * 0.5f + scaledPadding.x);
        const float halfHeight = std::max(kMinGeneratedThickness * 0.5f, scaledSize.y * 0.5f + scaledPadding.y);

        collider.AddBox(halfWidth, halfHeight);
        collider.colliderOffset = b2Vec2{ scaledOffset.x, scaledOffset.y };

        Physics::Box2DShapeConfig shapeConfig{};
        shapeConfig.bodyType = request.bodyType;
        shapeConfig.fixedRotation = request.fixedRotation;
        shapeConfig.allowRotation = request.allowRotation;
        shapeConfig.angularDamping = request.angularDamping;
        shapeConfig.linearDamping = request.linearDamping;
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
}

namespace Physics
{
    ColliderBuilder::ColliderBuilder(LogFn log, const char* logPrefix)
        : m_log(std::move(log))
        , m_logPrefix(logPrefix ? logPrefix : "")
    {
    }

    ColliderBuilder::BuildResult ColliderBuilder::BuildFromRegistry(const RegistryBuildRequest& request) const
    {
        BuildResult build{};
        const std::string resolvedKey = request.key;
        const std::string logPrefix = m_logPrefix.empty() ? std::string("[ColliderBuilder] ") : m_logPrefix;

        const auto colliderHasShapes = [](const BuildResult& result)
        {
            return result.collider && !result.collider->Empty();
        };
        const auto shapeConfigIsValid = [](const BuildResult& result)
        {
            return result.shapeConfig && result.shapeConfig->type != Box2DShapeConfig::Type::None;
        };

        const SceneGameConfig::PlayerColliderTuning defaultTuning{};
        const SceneGameConfig::PlayerColliderTuning& tuning = request.tuning ? *request.tuning : defaultTuning;

        std::optional<std::map<std::string, Collision::ColliderDefinition>> loadedConfigDefinitions;
        const std::map<std::string, Collision::ColliderDefinition>* configDefinitions = request.configDefinitions;
        if (!configDefinitions && request.forceConfig && resolvedKey == "player")
        {
            loadedConfigDefinitions = LoadPlayerConfigDefinitions(m_log, logPrefix);
            if (loadedConfigDefinitions)
            {
                configDefinitions = &*loadedConfigDefinitions;
            }
        }

        const bool hasConfigDefinitions = configDefinitions
            && configDefinitions->find(resolvedKey) != configDefinitions->end();
        const bool configAvailable = hasConfigDefinitions;

        if (request.forceConfig && configAvailable)
        {
            build = BuildFromConfigInternal(m_log, logPrefix, request, tuning, *configDefinitions, resolvedKey);
        }
        else
        {
            build = BuildFromRegistryInternal(m_log, logPrefix, request, tuning, resolvedKey);
        }

        if (!colliderHasShapes(build) || !shapeConfigIsValid(build))
        {
            if (request.forceConfig)
            {
                build.attemptedRegistryFallback = true;
                std::ostringstream oss;
                oss << logPrefix << "[ColliderBuilder] Config build unavailable; attempting registry fallback for key='"
                    << resolvedKey << "'.\n";
                EmitLog(m_log, oss.str());

                BuildResult registryBuild = BuildFromRegistryInternal(m_log, logPrefix, request, tuning, resolvedKey);
                if (colliderHasShapes(registryBuild) && shapeConfigIsValid(registryBuild))
                {
                    build = registryBuild;
                }
            }
            else if (configAvailable)
            {
                build.attemptedConfigFallback = true;
                std::ostringstream oss;
                oss << logPrefix << "[ColliderBuilder] Registry build unavailable; attempting config fallback for key='"
                    << resolvedKey << "'.\n";
                EmitLog(m_log, oss.str());

                BuildResult configBuild = BuildFromConfigInternal(m_log, logPrefix, request, tuning, *configDefinitions, resolvedKey);
                if (colliderHasShapes(configBuild) && shapeConfigIsValid(configBuild))
                {
                    build = configBuild;
                }
            }
        }

        if (request.allowFallback && (!colliderHasShapes(build) || !shapeConfigIsValid(build)))
        {
            build.attemptedFallback = true;
            std::ostringstream oss;
            oss << logPrefix << "[ColliderBuilder] Registry/config build failed; using fallback collider for key='"
                << resolvedKey << "'.\n";
            EmitLog(m_log, oss.str());

            build = BuildFallbackInternal(request, tuning, resolvedKey);
        }

        EmitBuildSummary(m_log,
            logPrefix,
            build.shapeSource.empty() ? build.source : build.shapeSource,
            resolvedKey,
            build.shapeConfig ? build.shapeConfig->type : Box2DShapeConfig::Type::None,
            build.collider ? build.collider->shapes.size() : 0u);

        return build;
    }
}
