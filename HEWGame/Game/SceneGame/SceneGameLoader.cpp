#include "SceneGameLoader.h"

#include "AssimpModelLorder.h"
#include "ModelCache.h"
#include "AppGame.h"
#include "ColliderRegistry.h"
#include "ColliderConfigLoader.h"
#include "GameplayConfig.h"
#include "Systems/Physics/ColliderBuilder.h"
#include "SceneGameConfigLoader.h"
#include "MeteorsPrototypeManifest.h"
#include "Systems/Draw/DrawSystem.h"
#include "Systems/Physics/IPhysicsBinder.h"
#include "TextureCache.h"
#include "Entities/Player.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <sstream>
#include <utility>

namespace
{
    constexpr bool kPlayerIsKinematic = false;
    void LogMessage(const SceneGameLoader::LogFn& log, const std::string& message)
    {
        if (log)
        {
            log(message);
        }
    }
}

SceneGameLoader::SceneGameLoader(LogFn log, const char* logPrefix)
    : m_log(std::move(log))
    , m_logPrefix(logPrefix)
{
}

SceneGameLoader::Result SceneGameLoader::Load(const Config& config) const
{
    Result result{};

    Render::ModelCache& cache = Render::ModelCache::Instance();

    if (!config.device)
    {
        LogMessage(m_log, std::string(m_logPrefix) + "Failed to configure ModelCache: device was null.\n");
    }
    else
    {
        TextureCache& textureCache = App::Game::Instance().GetTextureCache();
        if (!cache.IsConfiguredFor(config.device, textureCache))
        {
            const SceneGameConfig::ModelImportSettings modelImportSettings = SceneGameConfig::LoadModelImportSettings();
            cache.SetLoader(
                std::make_unique<AssimpModelLorder>(std::chrono::milliseconds(modelImportSettings.importTimeoutMs)),
                config.device,
                textureCache);
        }
    }

    auto& manifest = MeteorsSpace::MeteorsPrototypeManifest::Instance();
    const auto* fairyDescriptor = manifest.Find("Fairy");
    if (!fairyDescriptor && !result.playerModel)
    {
        LogMessage(m_log, std::string(m_logPrefix) + "[ModelLoad] Fairy descriptor missing; using procedural fallback.\n");
    }

    const std::string primaryModelPath = fairyDescriptor ? fairyDescriptor->modelPath : std::string{};
    const std::string playerColliderKey = fairyDescriptor ? fairyDescriptor->key : std::string("Fairy");
    const std::string resolvedModelPath = primaryModelPath;
    if (!primaryModelPath.empty())
    {
        result.playerModel = cache.Get(primaryModelPath);
        if (!result.playerModel)
        {
            LogMessage(m_log, std::string(m_logPrefix) + "[ModelLoad] Primary fairy model missing; player will use procedural fallback.\n");
        }
    }
    else
    {
        LogMessage(m_log, std::string(m_logPrefix) + "[ModelLoad] Fairy model path missing; player will use procedural fallback.\n");
    }

    if (result.playerModel)
    {
        std::ostringstream oss;
        oss << m_logPrefix << "[ModelLoad] Player model ready path='" << resolvedModelPath
            << "' refCount=" << result.playerModel.use_count() << "\n";
        LogMessage(m_log, oss.str());
    }
    else
    {
        LogMessage(m_log, std::string(m_logPrefix) + "[ModelLoad] Failed to resolve any fairy model; player will use procedural fallback.\n");
    }

    if (config.drawSystem && config.services)
    {
        config.drawSystem->SetRenderer(config.services->GetRenderer());
    }

    std::optional<Physics::BoxColliderComponent> boxColliderComponent;
    std::optional<Physics::Box2DShapeConfig> playerShapeConfig;
    float3 playerColliderHalfExtents{ 0.0f, 0.0f, 0.0f };
    float3 playerColliderOffset{ 0.0f, 0.0f, 0.0f };
    Transform trans = config.playerSpawnTransform.transform;

    const b2BodyType playerBodyType = kPlayerIsKinematic ? b2_kinematicBody : b2_dynamicBody;
    const std::string resolvedColliderKey = NormalizeColliderKey(ResolvePlayerColliderKey(playerColliderKey));
    Physics::ColliderBuilder colliderBuilder(m_log, m_logPrefix);
    std::map<std::string, Collision::ColliderDefinition> rawColliderDefinitions;
    const bool hasPlayerColliderDefinitions = Collision::LoadColliderDefinitions(
        GameplayConfig::kConfigPath,
        GameplayConfig::kSectionPlayerColliders,
        rawColliderDefinitions);
    std::map<std::string, Collision::ColliderDefinition> playerColliderDefinitions;
    if (hasPlayerColliderDefinitions)
    {
        for (const auto& entry : rawColliderDefinitions)
        {
            playerColliderDefinitions[NormalizeColliderKey(entry.first)] = entry.second;
        }
    }
    Physics::ColliderBuilder::RegistryBuildRequest colliderRequest{};
    colliderRequest.key = resolvedColliderKey;
    colliderRequest.transform = trans;
    colliderRequest.bodyType = playerBodyType;
    colliderRequest.fixedRotation = true;
    colliderRequest.allowRotation = false;
    colliderRequest.angularDamping = 5.0f;
    colliderRequest.linearDamping = 0.0f;
    colliderRequest.scaleOffset = true;
    colliderRequest.tuning = &config.playerColliderTuning;
    colliderRequest.allowFallback = false;
    if (!playerColliderDefinitions.empty())
    {
        colliderRequest.configDefinitions = &playerColliderDefinitions;
    }
    if (resolvedColliderKey == "player")
    {
        colliderRequest.forceConfig = true;
        colliderRequest.allowFallback = true;
    }

    {
        const bool configHasPlayer = playerColliderDefinitions.find("player") != playerColliderDefinitions.end();
        std::ostringstream oss;
        oss << m_logPrefix << "[PlayerCollider] Config definitions=" << playerColliderDefinitions.size()
            << " key='player' present=" << std::boolalpha << configHasPlayer << "\n";
        LogMessage(m_log, oss.str());
    }

    {
        const auto& registry = Physics::ColliderRegistry::Instance();
        const bool registryHit = registry.Find(resolvedColliderKey).has_value();
        std::ostringstream oss;
        oss << m_logPrefix << "[PlayerCollider] Registry lookup before build key='" << resolvedColliderKey
            << "' hit=" << std::boolalpha << registryHit << "\n";
        LogMessage(m_log, oss.str());
    }

    Physics::ColliderBuilder::BuildResult colliderBuild = colliderBuilder.BuildFromRegistry(colliderRequest);
    const auto colliderHasShapes = [](const Physics::ColliderBuilder::BuildResult& build)
    {
        return build.collider && !build.collider->Empty();
    };
    const auto shapeConfigIsValid = [](const Physics::ColliderBuilder::BuildResult& build)
    {
        return build.shapeConfig && build.shapeConfig->type != Physics::Box2DShapeConfig::Type::None;
    };

    bool colliderReady = colliderHasShapes(colliderBuild) && shapeConfigIsValid(colliderBuild);
    if (!colliderReady)
    {
        std::ostringstream oss;
        oss << m_logPrefix << "[PlayerCollider] Collider definition for key='" << resolvedColliderKey
            << "' invalid; attempting fallback collider.\n";
        LogMessage(m_log, oss.str());

        SceneGameConfig::PlayerColliderTuning fallbackTuning{};
        colliderRequest.allowFallback = true;
        colliderRequest.tuning = &fallbackTuning;
        Physics::ColliderBuilder::BuildResult fallbackBuild = colliderBuilder.BuildFromRegistry(colliderRequest);
        if (colliderHasShapes(fallbackBuild) && shapeConfigIsValid(fallbackBuild))
        {
            colliderBuild = std::move(fallbackBuild);
            colliderReady = true;
        }
    }

    if (colliderReady)
    {
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
    }
    else
    {
        std::ostringstream oss;
        oss << m_logPrefix << "[PlayerCollider] ERROR: fallback collider build failed for key='"
            << resolvedColliderKey << "'; skipping physics registration to prevent missing collider.\n";
        LogMessage(m_log, oss.str());
    }

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

        oss << m_logPrefix << "[PlayerCollider] source=" << colliderBuild.source
            << " shapeSource=" << colliderBuild.shapeSource
            << " key='" << resolvedColliderKey << "' registryHit=" << colliderBuild.registryHit
            << " registryShapes=" << colliderBuild.registryHasShapes
            << " applied=" << colliderReady
            << " shapeCount=" << (boxColliderComponent ? boxColliderComponent->shapes.size() : 0u)
            << " shape=" << (playerShapeConfig ? shapeToString(playerShapeConfig->type) : "None")
            << " offset=(" << playerColliderOffset.x << ',' << playerColliderOffset.y << ',' << playerColliderOffset.z << ')'
            << " halfExtents=(" << playerColliderHalfExtents.x << ',' << playerColliderHalfExtents.y << ',' << playerColliderHalfExtents.z << ")\n";
        LogMessage(m_log, oss.str());
    }

    Systems::PhysicsMachine* physicsMachine = config.physicsBinder ? config.physicsBinder->GetPhysicsMachine() : nullptr;
    Core::Input* input = config.services ? config.services->GetInput() : nullptr;
    result.player = std::make_unique<Entities::Player>(
        trans,
        result.playerModel,
        physicsMachine,
        input,
        Collision::CollisionLayer::Player,
        kPlayerIsKinematic,
        Core::UpdatePhase::Gameplay,
        0,
        "player");

    if (result.player)
    {
        if (config.physicsBinder)
        {
            config.physicsBinder->SetPlayer(result.player.get());
        }
        if (boxColliderComponent)
        {
            result.player->SetBoxColliderComponent(boxColliderComponent);
        }
        if (playerShapeConfig)
        {
            result.player->SetBox2DShapeConfig(playerShapeConfig);
        }
        result.player->SetBaseMoveSpeed(config.playerBaseHorizontalSpeed, config.playerBaseVerticalSpeed);
        result.player->SetSpeedMultiplier(config.playerSpeedMultiplier);
        result.player->SetYawOffsetRadians(config.playerSpawnTransform.playerYawOffsetRadians);
        result.player->ConfigureMovement(
            config.playerMovementSettings.accel,
            config.playerMovementSettings.brake,
            config.playerMovementSettings.turnRate,
            config.playerMovementSettings.rollResponse,
            config.playerMovementSettings.inputSmoothing,
            config.playerMovementSettings.maxSpeed);
        result.player->SetWrapMargin(config.playerMovementSettings.wrapMargin);
        result.player->SetWrapEnabled(config.playerMovementSettings.wrapX, config.playerMovementSettings.wrapY);
        if (config.physicsBinder && boxColliderComponent && playerShapeConfig && colliderReady)
        {
            config.physicsBinder->RegisterPhysicsBody(*result.player);
        }
        {
            std::ostringstream oss;
            const auto configuredBodyType = playerShapeConfig ? playerShapeConfig->bodyType : playerBodyType;
            const b2BodyType createdBodyType = result.player->GetPhysical().GetBodyType();
            const bool hasBody = result.player->HasBox2DBody();
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
            oss << m_logPrefix << "Player physics body registration attempted (expected="
                << bodyTypeToString(configuredBodyType)
                << ", created=" << bodyTypeToString(createdBodyType)
                << ", match=" << std::boolalpha << typeMatches
                << ", hasBody=" << hasBody
                << ", colliderReady=" << colliderReady << ").\n";
            LogMessage(m_log, oss.str());
        }
        if (config.drawSystem)
        {
            config.drawSystem->RegisterRenderable(*result.player);
        }
    }

    return result;
}

std::string SceneGameLoader::NormalizeColliderKey(const std::string& key)
{
    std::string normalized;
    normalized.reserve(key.size());
    for (unsigned char ch : key)
    {
        normalized.push_back(static_cast<char>(std::tolower(ch)));
    }
    return normalized;
}

std::string SceneGameLoader::ResolvePlayerColliderKey(const std::string& key)
{
    const auto& manifest = MeteorsSpace::MeteorsPrototypeManifest::Instance();
    const auto resolvedType = manifest.Resolve(key);
    std::string resolvedKey = NormalizeColliderKey(key);
    if (resolvedType)
    {
        const auto* descriptor = manifest.Find(*resolvedType);
        if (descriptor && !descriptor->key.empty())
        {
            resolvedKey = NormalizeColliderKey(descriptor->key);
        }
    }

    if (resolvedKey == "fairy")
    {
        resolvedKey = "player";
    }

    const auto& registry = Physics::ColliderRegistry::Instance();
    if (registry.Find(resolvedKey))
    {
        return resolvedKey;
    }

    const std::string fallbackKey = NormalizeColliderKey("player");
    if (resolvedKey != fallbackKey && registry.Find(fallbackKey))
    {
        return fallbackKey;
    }

    return resolvedKey;
}
