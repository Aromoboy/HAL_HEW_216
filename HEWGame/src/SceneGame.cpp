#include "Transform.h"
#include "SceneGame.h"
#include "Geometory.h"
#include "DrawMatrix.h"
#include "Vector.h"
#include "ShaderList.h"
#include "Defines.h"
#include "AssimpModelLorder.h"
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
#include "Render/ModelBounds.h"
#include "Entities/Player.h"
#include "DebugColliderRenderer.h"
#include "SceneComposer.h"
#include "SceneGameConfigLoader.h"
#include "SceneGamePlayerColliderBuilder.h"
#include "SceneGameUIOverlay.h"
#include "GameplayConfig.h"
#include "UIManager.h"
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
    auto composition = std::make_unique<SceneComposer::Composition>(composer.Compose());
    m_playerColliderDefinitions = std::move(composition->playerColliderDefinitions);
    m_playerColliderConfigLoaded = composition->playerColliderConfigLoaded;
    m_forceConfigPlayerCollider = composition->forceConfigPlayerCollider;
    m_playerColliderTuning = composition->playerColliderTuning;
    m_playerSpawnTransform = composition->playerSpawnTransform;
    m_stageData = std::move(composition->stageData);
    m_phaseManager = std::move(composition->phaseManager);
    m_playerBaseHorizontalSpeed = composition->playerBaseHorizontalSpeed;
    m_playerBaseVerticalSpeed = composition->playerBaseVerticalSpeed;
    m_playerPickupBoxWidth = composition->playerPickupBoxWidth;
    m_playerPickupBoxHeight = composition->playerPickupBoxHeight;
    m_playerPickupOffset = composition->playerPickupOffset;
    m_playerPickupMaxDistance = composition->playerPickupMaxDistance;
    m_playerPickupDepthThreshold = composition->playerPickupDepthThreshold;
    m_playerSpeedMultiplier = composition->playerSpeedMultiplier;
    m_heightBonusThresholdMeters = composition->heightBonusThresholdMeters;
    m_heightBonusSeconds = composition->heightBonusSeconds;
    m_physicsBinder = std::move(composition->physicsBinder);
    m_drawSystem = std::move(composition->drawSystem);
    m_sweetsManager = std::move(composition->sweetsManager);
    m_uiManager = std::move(composition->uiManager);
    m_backgroundRenderer = std::move(composition->backgroundRenderer);
    m_stageObjects = std::move(composition->stageObjects);
    m_stagePhysicsEntities = std::move(composition->stagePhysicsEntities);
    GuidelineSystem::Config guidelineConfig{};
    guidelineConfig.heightMeters = composition->guidelineHeightMeters;
    guidelineConfig.stepMeters = composition->guidelineStepMeters;
    guidelineConfig.thicknessPixels = composition->guidelineThicknessPixels;
    guidelineConfig.widthPaddingWorld = composition->guidelineWidthPaddingWorld;
    guidelineConfig.zOffset = composition->guidelineZOffset;
    guidelineConfig.cooldownDurationSeconds = composition->guidelineCooldownDurationSeconds;
    guidelineConfig.cooldownRemainingSeconds = composition->guidelineCooldownRemainingSeconds;
    guidelineConfig.centerX = composition->guidelineCenterX;
    guidelineConfig.widthWorld = composition->guidelineWidthWorld;
    guidelineConfig.thicknessWorld = composition->guidelineThicknessWorld;
    m_guidelineSystem.Configure(guidelineConfig, std::move(composition->guidelineActor));

    GameplayRulesSystem::Config gameplayRulesConfig{};
    gameplayRulesConfig.defaultPlaySeconds = kDefaultPlaySeconds;
    gameplayRulesConfig.countdownLogIntervalSeconds = kCountdownLogIntervalSeconds;
    gameplayRulesConfig.baseScorePerMeter = kBaseScorePerMeter;
    m_gameplayRules.Configure(gameplayRulesConfig, DebugTrace, kLogPrefix);

    if (m_physicsBinder)
    {
        m_contactForwarder.Bind(*m_physicsBinder, m_sweetsManager.get());
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
    SceneGamePlayerColliderBuilder colliderBuilder(DebugTrace, kLogPrefix);
    SceneGamePlayerColliderBuilder::BuildResult colliderBuild = forceConfigCollider
        ? colliderBuilder.BuildFromConfig(
            resolvedColliderKey,
            trans,
            m_playerColliderTuning,
            m_playerColliderDefinitions,
            playerBodyType)
        : colliderBuilder.BuildFromRegistry(
            resolvedColliderKey,
            trans,
            m_playerColliderTuning,
            playerBodyType);
    const auto colliderHasShapes = [](const SceneGamePlayerColliderBuilder::BuildResult& build)
    {
        return build.collider && !build.collider->Empty();
    };
    const auto shapeConfigIsValid = [](const SceneGamePlayerColliderBuilder::BuildResult& build)
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

            SceneGamePlayerColliderBuilder::BuildResult registryBuild = colliderBuilder.BuildFromRegistry(
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

            SceneGamePlayerColliderBuilder::BuildResult configBuild = colliderBuilder.BuildFromConfig(
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

        SceneGamePlayerColliderBuilder::BuildResult fallbackBuild = colliderBuilder.BuildFallback(
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

    m_gameplayRules.ResetSession();
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
    const GameplayRulesSystem::UpdateInput rulesInput{
        m_stageState == StageState::Playing,
        ctx.dt,
        m_sweetsManager.get(),
        &m_phaseManager,
        m_clearPhaseCount
    };
    const GameplayRulesSystem::UpdateResult rulesResult = m_gameplayRules.Update(rulesInput);
    if (rulesResult.timeUp && m_stageState == StageState::Playing)
    {
        SetStageState(StageState::TimeUp);
    }
    if (rulesResult.phaseAdvanced && rulesResult.currentPhase > rulesResult.previousPhase)
    {
        BroadcastPhaseAdvance(rulesResult.currentPhase);
    }
    m_guidelineSystem.TickCooldowns(ctx.dt);

#if defined(_DEBUG)
    const auto& stacked = m_sweetsManager->GetByState(SweetsSpace::SweetsState::Stacked);
    for (const auto& sweets : stacked)
    {
        if (!sweets) continue;
        const AABB bounds = sweets->GetAABB();
        ::DebugTrace("[DEBUG] sweetsTop=" + std::to_string(bounds.max.y) + "\n");
    }
#endif

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

        const GuidelineSystem::UpdateInput guidelineInput{
            m_sweetsManager.get(),
            kGuidelineTimeBonusSeconds,
            kGuidelineGlobalCooldownSeconds,
            DebugTrace,
            kLogPrefix
        };
        GuidelineSystem::UpdateResult guidelineResult =
            m_guidelineSystem.Update(guidelineInput, {});
        m_gameplayRules.AddPendingBonuses(std::move(guidelineResult.pendingBonuses));
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
        const float remainingSeconds = std::max(0.0f, m_gameplayRules.GetRemainingCountdownSeconds());
        const float currentHeight = m_gameplayRules.GetCurrentTowerHeight();
        const float peakHeight = m_gameplayRules.GetPeakTowerHeight();
        const int score = m_gameplayRules.ComputeScore(&m_phaseManager);
        uiData.hud.remainingSeconds = remainingSeconds;
        uiData.hud.currentHeight = currentHeight;
        uiData.hud.peakHeight = peakHeight;
        uiData.hud.score = score;
        uiData.hud.sweetsCount = GetSweetsCount();
        uiData.hud.timeUp = isTimeUp;
        uiData.hud.moveSpeedMultiplier = m_playerSpeedMultiplier;
        if (player)
        {
            uiData.hud.moveSpeedUnitsPerSecond = player->GetMoveSpeed();
        }

        uiData.result.remainingSeconds = remainingSeconds;
        uiData.result.currentHeight = currentHeight;
        uiData.result.peakHeight = peakHeight;
        uiData.result.score = score;
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
    m_guidelineSystem.ResetHeight(m_stageData.GetGuidelineHeight(),
                                  m_stageData.GetGuidelineStep());
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
    const float peakHeight = m_gameplayRules.GetPeakTowerHeight();
    const int score = m_gameplayRules.ComputeScore(&m_phaseManager);
    oss << kLogPrefix << "[Stage] cleared peakTop=" << std::fixed << std::setprecision(2)
        << peakHeight << "m score=" << score << "\n";
    ::DebugTrace(oss.str());
}

void SceneGame::OnStageFail()
{
    ToggleGameplaySystems(false);

    std::ostringstream oss;
    const float peakHeight = m_gameplayRules.GetPeakTowerHeight();
    const int score = m_gameplayRules.ComputeScore(&m_phaseManager);
    oss << kLogPrefix << "[Countdown] timeUp peakTop=" << std::fixed << std::setprecision(2)
        << peakHeight << "m score=" << score << "\n";
    ::DebugTrace(oss.str());
}

void SceneGame::BroadcastPhaseAdvance(int newPhase)
{
    std::ostringstream oss;
    const float peakHeight = m_gameplayRules.GetPeakTowerHeight();
    oss << kLogPrefix << "Broadcasting phase " << newPhase
        << " (stackedPeak=" << std::fixed << std::setprecision(2) << peakHeight << ")\n";
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

    m_gameplayRules.ResetSession();
    //m_heightBonusArmed = (m_heightBonusThresholdMeters > 0.0f && m_heightBonusSeconds > 0.0f);
    m_guidelineSystem.ResetSession();
    ResetGuidelineHeight();
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
    oss << kLogPrefix << "[Countdown] start duration=" << m_gameplayRules.GetTotalPlaySeconds() << "s\n";
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
        const float peakHeight = m_gameplayRules.GetPeakTowerHeight();
        const int score = m_gameplayRules.ComputeScore(&m_phaseManager);
        oss << kLogPrefix << "[Result] Confirm accepted peakTop=" << std::fixed << std::setprecision(2)
            << peakHeight << "m score=" << score << "\n";
        ::DebugTrace(oss.str());
        SetStageState(StageState::Playing);
        return;
    }

    if (input.IsTrigger(Core::ActionId::Retry))
    {
        std::ostringstream oss;
        const float peakHeight = m_gameplayRules.GetPeakTowerHeight();
        oss << kLogPrefix << "[Result] Retry input ignored until confirm. peakTop="
            << std::fixed << std::setprecision(2) << peakHeight << "m\n";
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
    const float elapsedSeconds = m_gameplayRules.GetTotalPlaySeconds()
        - m_gameplayRules.GetRemainingCountdownSeconds();
    const float peakHeight = m_gameplayRules.GetPeakTowerHeight();
    oss << kLogPrefix << "[Countdown] Retry requested elapsed="
        << std::fixed << std::setprecision(2) << elapsedSeconds
        << "s peak=" << std::fixed << std::setprecision(2) << peakHeight << "m\n";
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
