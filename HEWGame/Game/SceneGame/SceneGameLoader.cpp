#include "SceneGameLoader.h"

#include "AnimationCache.h"
#include "AssimpModelLorder.h"
#include "ModelCache.h"
#include "AppGame.h"
#include "ColliderRegistry.h"
#include "Systems/Physics/ColliderBuilder.h"
#include "SweetsManager.h"
#include "SweetsPrototypeManifest.h"
#include "Systems/Draw/DrawSystem.h"
#include "Systems/Physics/IPhysicsBinder.h"
#include "TextureCache.h"
#include "Entities/Player.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
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
    Render::AnimationCache& animeCache = Render::AnimationCache::Instance();

    if (!config.device)
    {
        LogMessage(m_log, std::string(m_logPrefix) + "Failed to configure ModelCache: device was null.\n");
    }
    else
    {
        TextureCache& textureCache = App::Game::Instance().GetTextureCache();
        if (!cache.IsConfiguredFor(config.device, textureCache))
        {
            cache.SetLoader(std::make_unique<AssimpModelLorder>(), config.device, textureCache);
        }
        if (!animeCache.IsConfiguredFor(config.device))
        {
            animeCache.SetLoader(std::make_unique<AssimpModelLorder>(), config.device);
        }
    }

    auto& manifest = SweetsSpace::SweetsPrototypeManifest::Instance();
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
    Physics::ColliderBuilder::RegistryBuildRequest colliderRequest{};
    colliderRequest.key = resolvedColliderKey;
    colliderRequest.transform = trans;
    colliderRequest.bodyType = playerBodyType;
    colliderRequest.fixedRotation = true;
    colliderRequest.allowRotation = false;
    colliderRequest.angularDamping = 5.0f;
    colliderRequest.linearDamping = 0.0f;
    colliderRequest.scaleOffset = true;
    colliderRequest.allowFallback = false;

    Physics::ColliderBuilder::BuildResult colliderBuild = colliderBuilder.BuildFromRegistry(colliderRequest);
    const auto colliderHasShapes = [](const Physics::ColliderBuilder::BuildResult& build)
    {
        return build.collider && !build.collider->Empty();
    };
    const auto shapeConfigIsValid = [](const Physics::ColliderBuilder::BuildResult& build)
    {
        return build.shapeConfig && build.shapeConfig->type != Physics::Box2DShapeConfig::Type::None;
    };

    if (!colliderHasShapes(colliderBuild) || !shapeConfigIsValid(colliderBuild))
    {
        std::ostringstream oss;
        oss << m_logPrefix << "[PlayerCollider] Collider definition for key='" << resolvedColliderKey
            << "' is invalid; physics registration will be skipped.\n";
        LogMessage(m_log, oss.str());
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
        result.playerPickupBoxWidth = playerColliderHalfExtents.x * 2.0f;
        result.playerPickupBoxHeight = playerColliderHalfExtents.y * 2.0f;
    }
    else
    {
        result.playerPickupBoxWidth = config.playerPickupBoxWidth;
        result.playerPickupBoxHeight = config.playerPickupBoxHeight;
    }

    const bool colliderReady = colliderHasShapes(colliderBuild) && shapeConfigIsValid(colliderBuild);

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
            << " halfExtents=(" << playerColliderHalfExtents.x << ',' << playerColliderHalfExtents.y << ',' << playerColliderHalfExtents.z << ')'
            << " pickupBox=(" << result.playerPickupBoxWidth << ',' << result.playerPickupBoxHeight << ")\n";
        LogMessage(m_log, oss.str());
    }

    result.playerPickupOriginOffset = playerColliderOffset;
    if (!std::isfinite(result.playerPickupOriginOffset.x)
        || !std::isfinite(result.playerPickupOriginOffset.y)
        || !std::isfinite(result.playerPickupOriginOffset.z))
    {
        result.playerPickupOriginOffset = float3{ 0.0f, 0.0f, 0.0f };
    }

    Systems::PhysicsMachine* physicsMachine = config.physicsBinder ? config.physicsBinder->GetPhysicsMachine() : nullptr;
    Core::Input* input = config.services ? config.services->GetInput() : nullptr;
    result.player = std::make_unique<Entities::Player>(
        trans,
        result.playerModel,
        physicsMachine,
        config.sweetsManager,
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
        if (playerColliderHalfExtents.x > 0.0f && playerColliderHalfExtents.y > 0.0f && playerColliderHalfExtents.z > 0.0f)
        {
            result.player->SetColliderFootprint(playerColliderHalfExtents, playerColliderOffset);
        }
        result.player->SetBaseMoveSpeed(config.playerBaseHorizontalSpeed, config.playerBaseVerticalSpeed);
        result.player->SetSpeedMultiplier(config.playerSpeedMultiplier);
        result.player->SetPickupArea(result.playerPickupBoxWidth, result.playerPickupBoxHeight, config.playerPickupOffset);
        result.player->SetPickupMaxDistance(config.playerPickupMaxDistance);
        result.player->SetPickupDepthThreshold(config.playerPickupDepthThreshold);
        result.player->SetPickupOriginOffset(result.playerPickupOriginOffset);
        if (colliderReady)
        {
            if (config.physicsBinder)
            {
                config.physicsBinder->RegisterPhysicsBody(*result.player);
            }
            {
                std::ostringstream oss;
                const auto configuredBodyType = playerShapeConfig ? playerShapeConfig->bodyType : playerBodyType;
                const b2BodyType createdBodyType = result.player->GetPhysical().GetBodyType();
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
                oss << m_logPrefix << "Player physics body registered (expected="
                    << bodyTypeToString(configuredBodyType)
                    << ", created=" << bodyTypeToString(createdBodyType)
                    << ", match=" << std::boolalpha << typeMatches << ").\n";
                LogMessage(m_log, oss.str());
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
            oss << m_logPrefix << "[PlayerCollider] Skipping physics registration; colliderReady="
                << std::boolalpha << colliderReady
                << " shapes=" << (boxColliderComponent ? boxColliderComponent->shapes.size() : 0u)
                << " shapeType=" << (playerShapeConfig ? shapeToString(playerShapeConfig->type) : "None")
                << "\n";
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
    const auto& manifest = SweetsSpace::SweetsPrototypeManifest::Instance();
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
