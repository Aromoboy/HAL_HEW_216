#include "Transform.h"
#include "SceneGame.h"
#include "Geometory.h"
#include "DrawMatrix.h"
#include "Vector.h"
#include "ShaderList.h"
#include "Defines.h"
#include "Core/Input.h"
#include "Core/UpdateNode.h"
#include "Systems/Physics/PhysicsBinder.h"
#include "Systems/Draw/DrawSystem.h"
#include "BoxColliderComponent.h"
#include "DirectX.h"
#include "BaseCamera.h"
#include "PerspectiveCamera.h"
#include "SweetsManager.h"
#include "Sweets/BaseSweets.h"
#include "Render/Model.h"
#include "Render/ModelBounds.h"
#include "Entities/Player.h"
#include "DebugColliderRenderer.h"
#include "SceneComposer.h"
#include "SceneGameLoader.h"
#include "SceneGameUIOverlay.h"
#include "SceneGameConfigLoader.h"
#include "UIManager.h"
#include "PlayerController.h"
#include "AppGame.h"
#include "Systems/Physics/ColliderBuilder.h"
#include "CameraMath.h"
#include "AnimationCache.h"
#include "AssimpModelLorder.h"
#include "ModelCache.h"
#include "ShaderCache.h"
#include <cassert>
#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <cmath>
#include <filesystem>
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
#include <box2d/box2d.h>
#include <DirectXMath.h>
#include "Physical.h"
#include "ColliderRegistry.h"
#include "TitleScene.h"
#include "SceneResult.h"


#if defined(_WIN32)
#       ifndef NOMINMAX
#               define NOMINMAX
#       endif
#       include <windows.h>
#endif
#include <ScenePowerUp.h>

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
        constexpr bool kEnableStageModelVerboseLogging = false;
        constexpr float kDefaultPlaySeconds = 45.0f;
        constexpr float kCountdownLogIntervalSeconds = 5.0f;
        constexpr float kBaseScorePerMeter = 100.0f;
        constexpr float kScoreToPointsMultiplier = 1.0f;
        constexpr float kGuidelineGlobalCooldownSeconds = 0.15f;
        constexpr float kShadowReceiverTolerance = 0.05f;
        SceneGameConfig::CameraRigSettings ApplyGameplayCameraOverrides(SceneGameConfig::CameraRigSettings settings)
        {
            settings.fixedPosition = false;
            settings.lockTranslation = false;
            settings.lockYawPitch = true;
            return settings;
        }

        bool ReadOsaraColliderDebugFlag()
        {
                const char* value = std::getenv("r.DebugOsaraColliders");
                if (!value)
                {
                        return true;
                }

                return std::atoi(value) != 0;
        }

        bool g_RegisterOsaraColliders = ReadOsaraColliderDebugFlag();

        bool IsFiniteFloat(float value)
        {
            return std::isfinite(value) != 0;
        }

        bool OverlapsShadowReceiverXZ(const AABB& a, const AABB& b)
        {
            return a.min.x <= b.max.x && a.max.x >= b.min.x;
        }

        int ConvertScoreToPoints(int score)
        {
            const float rawPoints = static_cast<float>(score) * kScoreToPointsMultiplier;
            return std::max(0, static_cast<int>(std::lround(rawPoints)));
        }

        std::string NormalizeStageKey(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
                {
                    return static_cast<char>(std::tolower(ch));
                });
            return value;
        }

        std::string BuildStageColliderKey(const StageObject& obj)
        {
            if (!obj.colliderPresetId.empty())
            {
                return NormalizeStageKey(obj.colliderPresetId);
            }

            std::string key = obj.modelPath;
            try
            {
                key = std::filesystem::path(obj.modelPath).stem().string();
            }
            catch (...)
            {
                // fallback to model path as-is
            }

            return NormalizeStageKey(key);
        }

        bool IsCakeStageObject(const StageObject& obj)
        {
            const std::string materialKey = NormalizeStageKey(obj.materialKey);
            if (materialKey == "cake")
            {
                return true;
            }

            const std::string colliderKey = NormalizeStageKey(obj.colliderPresetId);
            if (colliderKey == "cake")
            {
                return true;
            }

            const std::string modelPath = NormalizeStageKey(obj.modelPath);
            return modelPath.find("cake") != std::string::npos;
        }

        float ComputeViewDepth(const float3& cameraPosition, const float3& cameraRotation, const float3& worldPosition)
        {
            DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(
                cameraRotation.x,
                cameraRotation.y,
                cameraRotation.z);

            DirectX::XMVECTOR forward = DirectX::XMVector3TransformNormal(
                DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotationMatrix);
            DirectX::XMVECTOR upVector = DirectX::XMVector3TransformNormal(
                DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotationMatrix);

            float3 focus
            {
                cameraPosition.x + DirectX::XMVectorGetX(forward),
                cameraPosition.y + DirectX::XMVectorGetY(forward),
                cameraPosition.z + DirectX::XMVectorGetZ(forward)
            };

            float3 up
            {
                DirectX::XMVectorGetX(upVector),
                DirectX::XMVectorGetY(upVector),
                DirectX::XMVectorGetZ(upVector)
            };

            DirectX::XMFLOAT4X4 viewMatrix;
            DrawMatrix::CreateViewMatrix(viewMatrix, cameraPosition, focus, up);

            DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&viewMatrix);
            DirectX::XMVECTOR targetWorld = DirectX::XMVectorSet(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f);
            DirectX::XMVECTOR targetView = DirectX::XMVector3TransformCoord(targetWorld, view);
            return std::abs(DirectX::XMVectorGetZ(targetView));
        }

        std::optional<float> ComputeReceiverHeightFromBounds(const AABB& bounds, float receiverY)
        {
            if (!IsFiniteFloat(bounds.min.y) || !IsFiniteFloat(receiverY))
            {
                return std::nullopt;
            }

            float height = bounds.min.y - receiverY;
            if (!IsFiniteFloat(height))
            {
                return std::nullopt;
            }

            return std::max(0.0f, height);
        }

        bool TryFindShadowReceiverYFromStage(const AABB& entityBounds,
            const std::vector<AABB>& stageBounds,
            float* outReceiverY)
        {
            if (!outReceiverY || stageBounds.empty())
            {
                return false;
            }

            float bestReceiver = -std::numeric_limits<float>::infinity();
            bool found = false;
            for (const auto& bounds : stageBounds)
            {
                if (!OverlapsShadowReceiverXZ(entityBounds, bounds))
                {
                    continue;
                }

                const float topY = bounds.max.y;
                if (!IsFiniteFloat(topY))
                {
                    continue;
                }

                if (topY <= entityBounds.min.y + kShadowReceiverTolerance && topY > bestReceiver)
                {
                    bestReceiver = topY;
                    found = true;
                }
            }

            if (found)
            {
                *outReceiverY = bestReceiver;
            }

            return found;
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

}

float3 fRads = { 0.0f, 0.0f, 0.0f };

bool SceneGame::Load(Core::Input& input)
{
    m_inputService = &input;
    std::ostringstream oss;
    oss << kLogPrefix << "Load scenePtr=" << this << "\n";
    ::DebugTrace(oss.str());

    if (m_isComposed)
    {
        return !m_loadFailed;
    }

    ID3D11Device* device = GetDevice();
    if (device)
    {
        TextureCache& textureCache = App::Game::Instance().GetTextureCache();
        Render::ModelCache& modelCache = Render::ModelCache::Instance();
        if (!modelCache.IsConfiguredFor(device, textureCache))
        {
            modelCache.SetLoader(std::make_unique<AssimpModelLorder>(), device, textureCache);
        }

        Render::AnimationCache& animationCache = Render::AnimationCache::Instance();
        if (!animationCache.IsConfiguredFor(device))
        {
            animationCache.SetLoader(std::make_unique<AssimpModelLorder>(), device);
        }
    }

    SceneComposer composer(GetUpdateScheduler());
    auto composition = std::make_unique<SceneComposer::Composition>(composer.ComposeBootstrap());
    if (!composition->stageConfigLoaded)
    {
        DebugTrace(std::string(kLogPrefix) + "Stage configuration failed; scene load aborted.\n");
        m_loadFailed = true;
        return false;
    }
    m_loadFailed = false;
    m_materialTuningSettings = composition->materialTuningSettings;
    Render::ModelCache::Instance().SetMaterialTuningSettings(m_materialTuningSettings);
    composer.Compose(*composition);
    composer.ComposeEffect(*composition, m_manager->GetEffectRenderer(), m_manager->GetEffectManager());
    m_playerSpawnTransform = composition->playerSpawnTransform;
    m_stageData = std::move(composition->stageData);
    m_spawnOriginsPrimaryInitial = m_stageData.GetSpawnPointsPrimary();
    m_spawnOriginsSecondaryInitial = m_stageData.GetSpawnPointsSecondary();
    m_stagePlaneZ = std::nullopt;
    const float stagePlaneZ = m_stageData.GetPlaneZ();
    if (IsFiniteFloat(stagePlaneZ))
    {
        m_stagePlaneZ = stagePlaneZ;
    }
    m_phaseManager = std::move(composition->phaseManager);
    m_playerBaseHorizontalSpeed = composition->playerBaseHorizontalSpeed;
    m_playerBaseVerticalSpeed = composition->playerBaseVerticalSpeed;
    m_playerPickupBoxWidth = composition->playerPickupBoxWidth;
    m_playerPickupBoxHeight = composition->playerPickupBoxHeight;
    m_playerPickupOffset = composition->playerPickupOffset;
    m_playerPickupMaxDistance = composition->playerPickupMaxDistance;
    m_playerPickupDepthThreshold = composition->playerPickupDepthThreshold;
    m_playerSpeedMultiplier = composition->playerSpeedMultiplier;
    m_playerSpeedMultiplierBase = m_playerSpeedMultiplier;
    LoadPlayerPowerUpData();
    m_renderPostProcessSettings = composition->renderPostProcessSettings;
    m_lightingSettings = composition->lightingSettings;
    m_blobShadowSettings = composition->blobShadowSettings;
    m_sweetsOutOfBoundsSettings = composition->sweetsOutOfBoundsSettings;
    m_physicsBinder = std::move(composition->physicsBinder);
    m_drawSystem = std::move(composition->drawSystem);
    m_sweetsManager = std::move(composition->sweetsManager);
    if (m_sweetsManager)
    {
        m_sweetsManager->SetOutOfBoundsSettings(
            m_sweetsOutOfBoundsSettings.outOfBoundsDespawn,
            m_sweetsOutOfBoundsSettings.outOfBoundsRespawn,
            m_sweetsOutOfBoundsSettings.outOfBoundsRespawnDelaySeconds);
        m_sweetsManager->SetGrabRespawnDelaySeconds(
            m_sweetsOutOfBoundsSettings.grabRespawnDelaySeconds);
    }
    m_uiManager = std::move(composition->uiManager);
    m_backgroundRenderer = std::move(composition->backgroundRenderer);
    m_blobShadowRenderer = std::make_unique<BlobShadowRenderer>();
    if (m_blobShadowRenderer)
    {
        m_blobShadowRenderer->SetSettings(m_blobShadowSettings);
    }
    GuidelineSystem::Config guidelineConfig{};
    guidelineConfig.heightMeters = composition->guidelineHeightMeters;
    guidelineConfig.stepMeters = composition->guidelineStepMeters;
    guidelineConfig.thicknessPixels = composition->guidelineThicknessPixels;
    guidelineConfig.widthPaddingWorld = composition->guidelineWidthPaddingWorld;
    guidelineConfig.zOffset = composition->guidelineZOffset;
    guidelineConfig.spinAxis = composition->guidelineSpinAxis;
    guidelineConfig.spinSpeedDegPerSec = composition->guidelineSpinSpeedDegPerSec;
    guidelineConfig.cooldownDurationSeconds = composition->guidelineCooldownDurationSeconds;
    guidelineConfig.cooldownRemainingSeconds = composition->guidelineCooldownRemainingSeconds;
    guidelineConfig.centerX = composition->guidelineCenterX;
    guidelineConfig.widthWorld = composition->guidelineWidthWorld;
    guidelineConfig.thicknessWorld = composition->guidelineThicknessWorld;
    guidelineConfig.milestoneBonusSeconds = m_stageData.GetGuidelineMilestoneBonusSeconds();
    guidelineConfig.milestoneInterval = m_stageData.GetGuidelineMilestoneInterval();
    m_guidelineSystem.Configure(guidelineConfig, std::move(composition->guidelineActor));
    m_effectSystem = std::move(composition->effectSystem);

    GameplayRulesSystem::Config gameplayRulesConfig{};
    const int timerLevel = std::max(0, m_playerPowerUpData.timerLevel);
    gameplayRulesConfig.defaultPlaySeconds = kDefaultPlaySeconds + static_cast<float>(timerLevel) * 5.0f;
    gameplayRulesConfig.countdownLogIntervalSeconds = kCountdownLogIntervalSeconds;
    gameplayRulesConfig.baseScorePerMeter = kBaseScorePerMeter;
    m_gameplayRules.Configure(gameplayRulesConfig, DebugTrace, kLogPrefix);

    {
        std::ostringstream materialLog;
        materialLog << kLogPrefix << "[MaterialTuning] Applied profiles=" << m_materialTuningSettings.profiles.size()
            << " overrides=" << m_materialTuningSettings.overrides.size() << "\n";
        for (const auto& entry : m_materialTuningSettings.profiles)
        {
            const auto& profile = entry.second;
            materialLog << kLogPrefix << "  Profile '" << entry.first
                << "' lightResponse=" << profile.lightResponse
                << " sssIntensity=" << profile.sssIntensity
                << " sssColor=(" << profile.sssColor.x << ',' << profile.sssColor.y << ',' << profile.sssColor.z << ")\n";
        }
        for (const auto& entry : m_materialTuningSettings.overrides)
        {
            const auto& overrideEntry = entry.second;
            materialLog << kLogPrefix << "  Override '" << entry.first << "' -> profile='"
                << overrideEntry.profileName << "'";
            if (overrideEntry.lightResponse)
            {
                materialLog << " lightResponse=" << *overrideEntry.lightResponse;
            }
            if (overrideEntry.sssIntensity)
            {
                materialLog << " sssIntensity=" << *overrideEntry.sssIntensity;
            }
            if (overrideEntry.sssColor)
            {
                materialLog << " sssColor=(" << overrideEntry.sssColor->x << ','
                    << overrideEntry.sssColor->y << ',' << overrideEntry.sssColor->z << ')';
            }
            materialLog << "\n";
        }
        DebugTrace(materialLog.str());
    }

    if (m_physicsBinder)
    {
        m_contactHandler.Bind(*m_physicsBinder, m_sweetsManager.get());
    }

    m_playerColliderMismatchLogged = false;
    SceneGameLoader loader(DebugTrace, kLogPrefix);
    m_renderer = &App::Game::Instance().GetModelRenderer();
    SceneGameLoader::Config loaderConfig{};
    loaderConfig.device = GetDevice();
    loaderConfig.services = this;
    loaderConfig.physicsBinder = m_physicsBinder.get();
    loaderConfig.drawSystem = m_drawSystem.get();
    loaderConfig.sweetsManager = m_sweetsManager.get();
    loaderConfig.playerSpawnTransform = m_playerSpawnTransform;
    loaderConfig.playerBaseHorizontalSpeed = m_playerBaseHorizontalSpeed;
    loaderConfig.playerBaseVerticalSpeed = m_playerBaseVerticalSpeed;
    loaderConfig.playerPickupBoxWidth = m_playerPickupBoxWidth;
    loaderConfig.playerPickupBoxHeight = m_playerPickupBoxHeight;
    loaderConfig.playerPickupOffset = m_playerPickupOffset;
    loaderConfig.playerPickupMaxDistance = m_playerPickupMaxDistance;
    loaderConfig.playerPickupDepthThreshold = m_playerPickupDepthThreshold;
    loaderConfig.playerSpeedMultiplier = m_playerSpeedMultiplier;

    SceneGameLoader::Result loaderResult = loader.Load(loaderConfig);
    m_uiOverlay.reset();
    if (m_uiManager)
    {
        m_uiOverlay = std::make_unique<SceneGameUIOverlay>();
        if (m_uiOverlay && !m_uiOverlay->Initialize(m_uiManager.get()))
        {
            DebugTrace(std::string(kLogPrefix) + "UI overlay failed to bind UI manager; overlay disabled.\n");
            m_uiOverlay.reset();
        }
    }
    m_playerModel = std::move(loaderResult.playerModel);
    m_player = std::move(loaderResult.player);
    m_playerPickupBoxWidth = loaderResult.playerPickupBoxWidth;
    m_playerPickupBoxHeight = loaderResult.playerPickupBoxHeight;
    m_playerPickupOriginOffset = loaderResult.playerPickupOriginOffset;
    m_stageObjects = std::move(composition->stageObjects);
    m_stageObjectsInitial = m_stageObjects;
    m_stagePhysicsEntities = std::move(composition->stagePhysicsEntities);
    if (m_sweetsManager)
    {
        std::optional<AABB> cakeBounds;
        for (const auto& obj : m_stageObjects)
        {
            if (!IsCakeStageObject(obj))
            {
                continue;
            }

            const auto& physicsEntity = obj.physicsEntity;
            if (!physicsEntity)
            {
                continue;
            }

            physicsEntity->SyncColliderBounds();
            const AABB bounds = physicsEntity->GetAABB();
            if (!cakeBounds)
            {
                cakeBounds = bounds;
            }
            else
            {
                cakeBounds->min.x = std::min(cakeBounds->min.x, bounds.min.x);
                cakeBounds->min.y = std::min(cakeBounds->min.y, bounds.min.y);
                cakeBounds->min.z = std::min(cakeBounds->min.z, bounds.min.z);
                cakeBounds->max.x = std::max(cakeBounds->max.x, bounds.max.x);
                cakeBounds->max.y = std::max(cakeBounds->max.y, bounds.max.y);
                cakeBounds->max.z = std::max(cakeBounds->max.z, bounds.max.z);
            }
        }

        m_sweetsManager->SetCakeBounds(cakeBounds);
    }
    ApplyPowerUpEffects(m_playerPowerUpData);

#if defined(_DEBUG)
    {
        std::ostringstream debugFlagLog;
        debugFlagLog << kLogPrefix
            << "[ColliderDebug] Stage colliders r.DebugOsaraColliders="
            << (g_RegisterOsaraColliders ? 1 : 0)
            << " (default=1). Toggle overlay with F5 (r.DebugDrawEnabled) and colliders with F4 (r.DebugColliders).\n";
        DebugTrace(debugFlagLog.str());
    }
#endif

    RenderTarget* pRTV = GetDefaultRTV();
    DepthStencil* pDSV = GetDefaultDSV();
    SetRenderTargets(1, &pRTV, pDSV);
    SetDepthTest(true);

    SceneGameConfig::CameraRigSettings cameraSettings = SceneGameConfig::LoadCameraRigSettings();
    m_gameCameraConfig = SceneGameConfig::LoadGameCameraConfig(); // Source of truth: gameplay.cfg [camera.rig].
    m_cameraRigSettings = cameraSettings;
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
        ConfigureCamera(cameraSettings, m_gameCameraConfig);
        if (m_player)
        {
            m_cameraRig->Attach(m_player.get());
        }
    }

    m_gameplayRules.ResetSession();
    m_isComposed = true;
    return true;
}

void SceneGame::Update(Core::StepTimer& timer, Core::Input& input)
{
    if (m_loadFailed)
    {
        return;
    }
    RootUpdate(timer, input);



    //if (m_pendingRetryReset)
    //{
    //    m_pendingRetryReset = false;
    //    SetStageState(StageState::Playing);
    //}
}

void SceneGame::Draw()
{
    if (m_loadFailed)
    {
        return;
    }

    auto& dxContext = App::Game::Instance().GetDXContext();
    ID3D11DeviceContext* context = dxContext.GetContext();
    RenderTarget* sceneColorRTV = dxContext.GetSceneColorTarget();
    DepthStencil* sceneDepthDSV = dxContext.GetSceneDepthTarget();

    if (context && sceneColorRTV)
    {
        SetRenderTargets(1, &sceneColorRTV, sceneDepthDSV);
    }

    auto activeCamera = GetActiveCamera();
    DrawWorld(activeCamera);

    if (context)
    {
        RenderTarget* defaultRTV = GetDefaultRTV();
        if (defaultRTV)
        {
            SetRenderTargets(1, &defaultRTV, nullptr);
        }

        auto postprocess = ShaderCache::Instance().Get("postprocess");
        if (postprocess)
        {
            ID3D11ShaderResourceView* sceneColorSRV = dxContext.GetSceneColorSRV();
            ID3D11ShaderResourceView* sceneDepthSRV = dxContext.GetSceneDepthSRV();
            float cameraNear = 0.1f;
            float cameraFar = 1000.0f;
            if (auto perspectiveCamera = std::dynamic_pointer_cast<CameraSpace::PerspectiveCamera>(activeCamera))
            {
                cameraNear = perspectiveCamera->GetNearPlane();
                cameraFar = perspectiveCamera->GetFarPlane();
            }
            context->PSSetShaderResources(0, 1, &sceneColorSRV);
            context->PSSetShaderResources(1, 1, &sceneDepthSRV);
            SetSamplerState(SAMPLER_POSTPROCESS);
            const auto& postSettings = m_renderPostProcessSettings;
            DirectX::XMFLOAT4 fogColor = DirectX::XMFLOAT4(
                postSettings.fogColor.x,
                postSettings.fogColor.y,
                postSettings.fogColor.z,
                1.0f);
            float fogStart = postSettings.fogStart;
            float fogRange = postSettings.fogRange;
            float vignetteStrength = postSettings.vignetteStrength;
            float bloomIntensity = postSettings.bloomIntensity;
            const UINT screenWidth = dxContext.GetWidth();
            const UINT screenHeight = dxContext.GetHeight();
            const float invWidth = screenWidth > 0 ? 1.0f / static_cast<float>(screenWidth) : 0.0f;
            const float invHeight = screenHeight > 0 ? 1.0f / static_cast<float>(screenHeight) : 0.0f;
            const DirectX::XMFLOAT2 invScreenSize(invWidth, invHeight);
            float postTime = 0.0f;
            if (const auto* timer = GetTimer())
            {
                postTime = static_cast<float>(timer->FrameIndex()) * timer->DeltaTime();
            }

            if (!postSettings.enableFog)
            {
                fogStart = cameraFar + 1.0f;
                fogRange = 1.0f;
            }
            if (!postSettings.enableVignette)
            {
                vignetteStrength = 0.0f;
            }
            if (!postSettings.enableBloom)
            {
                bloomIntensity = 0.0f;
            }

            BindPostProcessParams(
                postSettings.gamma,
                postSettings.saturation,
                postSettings.contrast,
                fogColor,
                fogStart,
                fogRange,
                cameraNear,
                cameraFar,
                vignetteStrength,
                bloomIntensity,
                postSettings.bloomThreshold,
                invScreenSize,
                postSettings.fxaaEdgeThreshold,
                postSettings.fxaaEdgeThresholdMin,
                postSettings.fxaaSubpixelQuality,
                postSettings.enableFxaa ? 1 : 0,
                postTime,
                postSettings.grainAmount,
                postSettings.grainEnable ? 1 : 0,
                postSettings.grainAnimate ? 1 : 0);

            postprocess->Bind(context);

            context->IASetInputLayout(nullptr);
            context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            context->Draw(3, 0);

            ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
            context->PSSetShaderResources(0, 2, nullSRVs);
        }
    }

    DrawOverlay();
}

void SceneGame::OnEnter()
{
    std::ostringstream oss;
    oss << kLogPrefix << "Enter scenePtr=" << this << "\n";
    ::DebugTrace(oss.str());

    for (int i = 0; i <= 3; ++i)
    {
        m_countdownTextures[i] = std::make_shared<Texture>();
    }

    m_countdownTextures[0]->Create("Assets/Texture/start_remake.png", true);
    m_countdownTextures[1]->Create("Assets/Texture/1-1.png", true);
    m_countdownTextures[2]->Create("Assets/Texture/2-1.png", true);
    m_countdownTextures[3]->Create("Assets/Texture/3-1.png", true);
    m_finishTexture = std::make_shared<Texture>();
    m_finishTexture->Create("Assets/Texture/Finish_remake.png", true);
    m_whistleTimer = 0.0f;
    m_whistlePlayed = false;
    m_bgmStarted = false;
    m_gameBGMHandle = -1;
}

void SceneGame::OnExit()
{
    //if (m_gameBGMHandle != -1 && SoundManagerEX::GetInstance())
    //{
    //    SoundManagerEX::GetInstance()->StopSound(m_gameBGMHandle);
    //    m_gameBGMHandle = -1;
    //}

    //if (m_inputRegistered)
    //{
    //    if (auto* input = GetInput())
    //    {
    //        Unregister(*input);
    //    }
    //    m_inputRegistered = false;
    //}

    //UnregisterSceneUpdateNode();
    //UnregisterCameraUpdateNode();

    //// CameraRig や Update 登録しているものがあれば必ず解除
    //if (m_cameraRig)
    //{
    //    Unregister(*m_cameraRig);
    //}
   /* if (m_gameBGMHandle != -1 && SoundManagerEX::GetInstance())
    {
        SoundManagerEX::GetInstance()->StopSound(m_gameBGMHandle);
        m_gameBGMHandle = -1;
    }

    if (m_inputRegistered)
    {
        if (auto* input = GetInput())
        {
            Unregister(*input);
        }
        m_inputRegistered = false;
    }

    UnregisterSceneUpdateNode();
    UnregisterCameraUpdateNode();

    if (m_cameraRig)
    {
        Unregister(*m_cameraRig);
    }*/

    // --- Input ---
    if (m_inputRegistered)
    {
        if (auto* input = GetInput())
        {
            Unregister(*input);
        }
        m_inputRegistered = false;
    }

    // --- Scene update ---
    UnregisterSceneUpdateNode();
    UnregisterCameraUpdateNode();

    // --- Physics ---
    if (m_physicsBinder)
    {
        if (auto* pm = m_physicsBinder->GetPhysicsMachine())
        {
            Unregister(*pm);
            if (auto* contact = pm->GetContactDispatchNode())
            {
                Unregister(*contact);
            }
        }
    }
    m_physicsRegistered = false;
    m_contactDispatchRegistered = false;

    // --- Draw ---
    if (m_drawRegistered && m_drawSystem)
    {
        Unregister(*m_drawSystem);
        m_drawRegistered = false;
    }

    // --- Camera ---
    if (m_cameraRigRegistered && m_cameraRig)
    {
        Unregister(*m_cameraRig);
        m_cameraRigRegistered = false;
    }

    // --- Sweets ---
    if (m_sweetsRegistered && m_sweetsManager)
    {
        Unregister(*m_sweetsManager);
        m_sweetsRegistered = false;
    }

    if (m_sweetsLateRegistered && m_sweetsManager)
    {
        if (auto* late = m_sweetsManager->GetLateUpdateNode())
        {
            Unregister(*late);
        }
        m_sweetsLateRegistered = false;
    }

    // --- Player ---
    if (m_player)
    {
        Unregister(*m_player);
    }

    // --- Effect ---
    if (m_effectSystemRegistered && m_effectSystem)
    {
        Unregister(*m_effectSystem);
        m_effectSystemRegistered = false;
    }

    // --- BGM ---
    if (m_gameBGMHandle != -1 && SoundManagerEX::GetInstance())
    {
        SoundManagerEX::GetInstance()->StopSound(m_gameBGMHandle);
        m_gameBGMHandle = -1;
    }



    std::ostringstream oss;
    oss << kLogPrefix << "Exit scenePtr=" << this << "\n";
    ::DebugTrace(oss.str());
}
   



SceneGame::SceneGame(SceneSpace::SceneManager& a_manager)
    : m_contactHandler()
    , m_playerController(DebugTrace, kLogPrefix)
    , m_manager(&a_manager)
{
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
            Unregister(*physicsMachine);
            m_physicsRegistered = false;
        }
    }

    if (m_contactDispatchRegistered && m_physicsBinder)
    {
        if (auto* physicsMachine = m_physicsBinder->GetPhysicsMachine())
        {
            if (auto* contactNode = physicsMachine->GetContactDispatchNode())
            {
                Unregister(*contactNode);
            }
        }
        m_contactDispatchRegistered = false;
    }

    if (m_cameraRigRegistered && m_cameraRig)
    {
        Unregister(*m_cameraRig);
        m_cameraRigRegistered = false;
    }

    if (m_drawRegistered && m_drawSystem)
    {
        Unregister(*m_drawSystem);
        m_drawRegistered = false;
    }

    if (m_sweetsLateRegistered && m_sweetsManager)
    {
        if (auto* lateNode = m_sweetsManager->GetLateUpdateNode())
        {
            Unregister(*lateNode);
        }
        m_sweetsLateRegistered = false;
    }

    if (m_sweetsRegistered && m_sweetsManager)
    {
        Unregister(*m_sweetsManager);
        m_sweetsRegistered = false;
    }

    if (m_player)
    {
        if (m_drawSystem)
        {
            m_drawSystem->UnregisterRenderable(*m_player);
        }
        Unregister(*m_player);
    }

    if (m_drawSystem)
    {
        m_drawSystem->ClearRenderables();
    }
    if (auto* soundMgr = SoundManagerEX::GetInstance())
    {
        soundMgr->StopAllSounds();
    }

}

void SceneGame::ConfigureUpdates()
{
        std::ostringstream configureLog;
        configureLog << kLogPrefix << "ConfigureUpdates begin scenePtr=" << this << "\n";
        ::DebugTrace(configureLog.str());

        if (m_physicsBinder && !m_physicsRegistered)
        {
                if (auto* physicsMachine = m_physicsBinder->GetPhysicsMachine())
                {
                        m_physicsRegistered = Register(*physicsMachine);

                        std::ostringstream oss;
                        oss << kLogPrefix << "ConfigureUpdates registered PhysicsMachine (phase="
                            << static_cast<int>(physicsMachine->GetPhase())
                            << ", priority=" << physicsMachine->GetPriority() << ").\n";
                        ::DebugTrace(oss.str());
                }
        }

        if (m_physicsBinder && !m_contactDispatchRegistered)
        {
                if (auto* physicsMachine = m_physicsBinder->GetPhysicsMachine())
                {
                        if (auto* contactNode = physicsMachine->GetContactDispatchNode())
                        {
                                m_contactDispatchRegistered = Register(*contactNode);
                        }
                }
        }

        if (!m_inputRegistered)
        {
                if (auto* input = GetInput())
                {
                        if (Register(*input))
                        {
                                m_inputRegistered = true;
                        }
                }
        }

        RegisterSceneUpdateNode(Core::UpdatePhase::Gameplay, 0);

        if (m_drawSystem && !m_drawRegistered)
        {
                m_drawRegistered = Register(*m_drawSystem);
        }

        if (m_cameraRig && !m_cameraRigRegistered)
        {
                m_cameraRigRegistered = Register(*m_cameraRig);
        }

        if (m_sweetsManager && !m_sweetsRegistered)
        {
                m_sweetsRegistered = Register(*m_sweetsManager);
        }

        if (m_sweetsManager && !m_sweetsLateRegistered)
        {
                if (auto* lateNode = m_sweetsManager->GetLateUpdateNode())
                {
                        m_sweetsLateRegistered = Register(*lateNode);
                }
        }


        if (m_player)
        {
                Register(*m_player);

                if (m_physicsBinder)
                {
                        m_physicsBinder->RegisterPhysicsBody(*m_player);
                        std::ostringstream oss;
                        const bool isKinematic = m_player->IsKinematic();
                        oss << kLogPrefix << "ConfigureUpdates ensured player body is registered (kinematic="
                            << std::boolalpha << isKinematic << ").\n";
                        ::DebugTrace(oss.str());
                }

                const int playerFixtureCount = m_player->GetBoxColliderComponent()
                    ? m_player->GetBoxColliderComponent()->createdFixtureCount
                    : 0;
                if (!m_player->HasBox2DBody() || playerFixtureCount <= 0)
                {
                        DebugTrace(std::string(kLogPrefix) + "Player Box2D body missing after registration; attempting repair.\n");
                        if (m_physicsBinder)
                        {
                            m_physicsBinder->RegisterPhysicsBody(*m_player);
                        }

                        const int repairedFixtures = m_player->GetBoxColliderComponent()
                            ? m_player->GetBoxColliderComponent()->createdFixtureCount
                            : 0;
                        if (!m_player->HasBox2DBody() || repairedFixtures <= 0)
                        {
                                DebugTrace(std::string(kLogPrefix) + "[Fatal] Player physics body unavailable; pickup input may fail.\n");
                        }
                }
        }

        if (m_effectSystem && !m_effectSystemRegistered)
        {
            m_effectSystemRegistered = Register(*m_effectSystem);
        }

        SetStageState(StageState::Playing);
}

void SceneGame::Update(const Core::UpdateContext& ctx)
{
#if defined(_DEBUG)
    HandleColliderDebugCommands(ctx);
#endif
    Core::Input* input = GetInput();
    
    if (m_physicsBinder)
    {
        m_physicsBinder->ValidatePlayerPhysicsRegistration();
    }
    /*const bool isPlaying = (m_stageState == StageState::Playing) && m_gameStarted;
    const GameplayRulesSystem::UpdateInput rulesInput{
        isPlaying,
        ctx.dt,
        m_sweetsManager.get(),
        &m_phaseManager,
        m_clearPhaseCount
    };
    const GameplayRulesSystem::UpdateResult rulesResult = m_gameplayRules.Update(rulesInput);
    if (rulesResult.timeUp && m_stageState == StageState::Playing)
    {
        if (auto* soundMgr = SoundManagerEX::GetInstance())
        {
            soundMgr->PlaySoundEX("Assets/Audio/SE_Finish.wav", false, 0.7f);
        }
        SetStageState(StageState::TimeUp);
        OnStageResult();
        m_manager->RequestSceneChange(std::make_unique<SceneSpace::SceneResult>(*m_manager, (int)m_gameplayRules.GetCurrentTowerHeight(), m_gameplayRules.ComputeScore(&m_phaseManager)));
        return;



    }
    if (rulesResult.phaseAdvanced && rulesResult.currentPhase > rulesResult.previousPhase)
    {
        UpdateCameraBoundsForPhase(m_gameplayRules.GetMaxTowerHeight());
        BroadcastPhaseAdvance(rulesResult.currentPhase);
    }*/
    
    m_guidelineSystem.TickCooldowns(ctx.dt);

#if defined(_DEBUG)
    if (m_sweetsManager)
    {
        const auto& stacked = m_sweetsManager->GetByState(SweetsSpace::SweetsState::Stacked);
        for (const auto& sweets : stacked)
        {
            if (!sweets) continue;
            const AABB bounds = sweets->GetAABB();
            ::DebugTrace("[DEBUG] sweetsTop=" + std::to_string(bounds.max.y) + "\n");
        }
    }
#endif

    const auto& platform = m_stageData.GetPlatform(0);
    const float stageFloorY = platform.center.y;
    float deltaTime = ctx.timer.DeltaTime();
    for (auto& obj : m_stageObjects)
    {
        if (obj.spinEnabled)
        {
            obj.currentRotation += obj.spinSpeed * deltaTime;
        }
    }

    for (auto& obj : m_stageObjects)
    {
        //Osara以外のモデルスキップ
        if (!IsOsara(obj))
        {
            continue;
        }

        obj.swaiTime += deltaTime;

        obj.transform.position.y
            = obj.baseY + std::sin(obj.swaiTime) * obj.OsaraSwayingpower.y;

        //当たり判定も動かす
        if (obj.physicsEntity)
        {
            obj.physicsEntity->SetPositionViaPhysics(obj.transform.position);
        }
    }

    const float guidelineMaxY = m_guidelineSystem.GetHeightMeters()
        + m_stageData.GetGuidelinePlayerMaxYOffset();
    m_playerController.UpdatePlayerBounds({
        m_player.get(),
        m_gameCamera.get(),
        m_sweetsManager.get(),
        ctx.dt,
        stageFloorY,
        guidelineMaxY
    });

    switch (m_stageState)
    {
    case StageState::Intro:
        break;
    case StageState::Playing:
    {
        Core::Input* input = GetInput();

        // カウントダウン
        if (!m_gameStarted)
        {
            m_startCountdownTimer += ctx.dt;

            if (m_startCountdownTimer >= 1.5f)
            {
                m_startCountdownTimer = 0.0f;

                if (m_startCountdownNumber > 1)
                {
                    m_startCountdownNumber--;
                    if (auto* sm = SoundManagerEX::GetInstance())
                        sm->PlaySoundEX("Assets/Audio/SE_CountDown.wav", false, 0.7f);
                }
                else if (m_startCountdownNumber == 1)
                {
                    m_startCountdownNumber = 0;
                    m_showStartImage = true;

                    if (!m_whistlePlayed)
                    {
                        if (auto* sm = SoundManagerEX::GetInstance())
                            sm->PlaySoundEX("Assets/Audio/SE_Start.wav", false, 0.7f);

                        m_whistlePlayed = true;
                        m_whistleTimer = 0.0f;
                    }
                }
                else if (m_showStartImage)
                {
                    m_showStartImage = false;
                    m_gameStarted = true;
                    ToggleGameplaySystems(true);
                }
            }

            return;
        }

        const GameplayRulesSystem::UpdateInput rulesInput{
            m_gameStarted,
            ctx.dt,
            m_sweetsManager.get(),
            &m_phaseManager,
            m_clearPhaseCount
        };
        const GameplayRulesSystem::UpdateResult rulesResult =
            m_gameplayRules.Update(rulesInput);
        m_realtimeScore = m_gameplayRules.GetCurrentScore();

        if (rulesResult.timeUp)
        {
            if (auto* sm = SoundManagerEX::GetInstance())
                sm->PlaySoundEX("Assets/Audio/SE_Finish.wav", false, 0.7f);

            SetStageState(StageState::TimeUp);
            EnsureResultSaved();
            OnStageResult();
            m_manager->RequestSceneChange(std::make_unique<SceneSpace::SceneResult>(*m_manager, (int)m_gameplayRules.GetCurrentTowerHeight(), m_gameplayRules.ComputeScore(&m_phaseManager)));
            return;
        }

        if (rulesResult.phaseAdvanced && rulesResult.currentPhase > rulesResult.previousPhase)
        {
            UpdateCameraBoundsForPhase(m_gameplayRules.GetMaxTowerHeight());
            BroadcastPhaseAdvance(rulesResult.currentPhase);
        }

        if (m_whistlePlayed && !m_bgmStarted)
        {
            m_whistleTimer += ctx.dt;
            if (m_whistleTimer >= 0.8f)
            {
                if (auto* sm = SoundManagerEX::GetInstance())
                    m_gameBGMHandle =
                    sm->PlaySoundEX("Assets/Audio/BGM_MainGame.wav", true, 0.6f);

                m_bgmStarted = true;
            }
        }

// --- タイムアップ判定 ---

        if (input)
        {
            PlayerController::SpeedInput speedInput{
                *input,
                m_player.get(),
                m_playerBaseHorizontalSpeed,
                m_playerBaseVerticalSpeed,
                m_playerSpeedMultiplier
            };
            m_playerSpeedMultiplier = m_playerController.HandleSpeedInput(speedInput).speedMultiplier;
            if (m_playerSpeedMultiplier < m_playerSpeedMultiplierBase)
            {
                m_playerSpeedMultiplier = m_playerSpeedMultiplierBase;
                m_player->SetSpeedMultiplier(m_playerSpeedMultiplier);
            }
        }

        const float elapsedSeconds = m_gameplayRules.GetTotalPlaySeconds()
            - m_gameplayRules.GetRemainingCountdownSeconds();
        const float peakHeight = m_gameplayRules.GetPeakTowerHeight();
        if (input && m_playerController.HandleManualRetryInput(*input, elapsedSeconds, peakHeight))
        {
            m_pendingRetryReset = true;
        }
        fRads.z += 0.001f;

        const GuidelineSystem::UpdateInput guidelineInput{
            m_sweetsManager.get(),
            m_stageData.GetGuidelineTimeBonusSeconds(),
            kGuidelineGlobalCooldownSeconds,
            DebugTrace,
            kLogPrefix
        };
        GuidelineSystem::UpdateResult guidelineResult =
            m_guidelineSystem.Update(guidelineInput, {});
        m_gameplayRules.AddPendingBonuses(std::move(guidelineResult.pendingBonuses));
        if (guidelineResult.guidelineAdvanced)
        {
          //  const int scorePerPhase = 200;
            
          //  m_isScoreCounting = true;
         //   m_scoreToAddRemaining += scorePerPhase;

            if (auto* soundMgr = SoundManagerEX::GetInstance())
            {
                soundMgr->PlaySoundEX("Assets/Audio/SE_PhaseUp.wav", false, 0.8f);
            }

            const float guidelineDelta = guidelineResult.guidelineHeightMeters - m_lastGuidelineHeight;
            m_lastGuidelineHeight = guidelineResult.guidelineHeightMeters;
            UpdateCameraBoundsForPhase(guidelineResult.guidelineHeightMeters);

            if (guidelineDelta != 0.0f)
            {
                m_isOsaraCounting = true;
                m_OsaraToAddRemaining += guidelineDelta;

                if (m_sweetsManager)
                {
                    m_sweetsManager->OffsetSpawnOriginsY(guidelineDelta);
                }

                for (auto& obj : m_stageObjects)
                {
                    if (!IsOsara(obj))
                    {
                        continue;
                    }


                //    obj.baseY += guidelineDelta;
                //    obj.transform.position.y += guidelineDelta;

                    if (obj.physicsEntity)
                    {
                        obj.physicsEntity->SetPositionViaPhysics(obj.transform.position);
                    }
                }


            }
        }
        //+200 スコア入った時徐々に増える
        //if (m_isScoreCounting)
        //{
        //    const float addInterval = 0.01f;
        //    m_scoreAddTimer += ctx.dt;

        //    while (m_scoreAddTimer >= addInterval && m_scoreToAddRemaining > 0)
        //    {
        //        m_scoreAddTimer -= addInterval;
        //        m_realtimeScore += 1;
        //        m_scoreToAddRemaining -= 1;
        //    }
        //    if (m_scoreToAddRemaining <= 0)
        //    {
        //        m_isScoreCounting = false;
        //        m_scoreAddTimer = 0.0;
        //    }
        //}
        UpdateOsaraGradualMove(ctx);
        break;
    }
    case StageState::TimeUp:
        SetStageState(StageState::Finish);
        m_finishTimer = 0.0f;   // Finish演出用タイマー初期化
        break;
    case StageState::Finish:
        m_finishTimer += ctx.dt;
        // FINISHを表示してからリザルトへ
        if (m_finishTimer >= 2.0f)
        {
            SetStageState(StageState::ShowingResult);
        }
        break;
    case StageState::Cleared:
        SetStageState(StageState::ShowingResult);
        break;
    case StageState::ShowingResult:
        break;
    }

    for (auto& obj : m_stageObjects)
    {
        if (obj.spinEnabled)
        {
            obj.currentRotation += obj.spinSpeed * deltaTime;
        }
    }

    if (m_stageState == StageState::ShowingResult)
    {
        const float peakHeight = m_gameplayRules.GetPeakTowerHeight();
//       const int score = m_gameplayRules.ComputeScore(&m_phaseManager,m_lastApprovePeakHeight);
//        const int score = m_gameplayRules.ComputeScore(&m_phaseManager);
        const int score = m_gameplayRules.GetCurrentScore();
        const auto action = input ? m_playerController.HandlePostGameMenuInput(*input, peakHeight, score, m_resultState)
            : PlayerController::PostGameAction::None;
        switch (action)
        {
        case PlayerController::PostGameAction::RetryGame:
            SetStageState(StageState::Playing);
            return;
        case PlayerController::PostGameAction::GoToPowerUp:
            if (m_manager && EnsureResultSaved())
            {
                m_manager->RequestSceneChange(
                    std::make_unique<TitleScene>(*m_manager));
            }
            return;
        case PlayerController::PostGameAction::GoToTitle:
            if (m_manager && EnsureResultSaved())
            {
                m_manager->RequestSceneChange(
                    std::make_unique<TitleScene>(*m_manager));
            }
            return;

        default:
            break;
        }
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
        const int score = m_gameplayRules.GetCurrentScore();
//        const int score = m_gameplayRules.ComputeScore(&m_phaseManager);
//        const int score = m_gameplayRules.ComputeScore(&m_phaseManager, m_lastApprovePeakHeight);
        uiData.hud.remainingSeconds = remainingSeconds;
        uiData.hud.currentHeight = currentHeight;
        uiData.hud.peakHeight = peakHeight;
        uiData.hud.score = score;
    //    uiData.hud.score = m_realtimeScore;
        uiData.hud.sweetsCount = GetSweetsCount();
        uiData.hud.timeUp = isTimeUp;
        uiData.hud.moveSpeedMultiplier = m_playerSpeedMultiplier;
        if (m_player)
        {
            uiData.hud.moveSpeedUnitsPerSecond = m_player->GetMoveSpeed();
        }
        if (g_DebugDrawEnabled && m_sweetsManager)
        {
            SweetsSpace::SweetsContacts::GlueDebugSnapshot glueSnapshot;
            if (m_sweetsManager->GetGlueDebugSnapshot(glueSnapshot))
            {
                uiData.hud.showGlueDebug = true;
                uiData.hud.glueActiveCreamAnchors = glueSnapshot.activeCreamAnchors;
                uiData.hud.glueActiveFruitGlue = glueSnapshot.activeFruitGlue;
                uiData.hud.glueFrozenFruit = glueSnapshot.frozenFruit;
                uiData.hud.glueCreatePerSec = glueSnapshot.createPerSec;
                uiData.hud.glueRejectPerSec = glueSnapshot.rejectPerSec;
                uiData.hud.glueCleanupPerSec = glueSnapshot.cleanupPerSec;
            }
        }

        uiData.result.remainingSeconds = remainingSeconds;
        uiData.result.currentHeight = currentHeight;
        uiData.result.peakHeight = peakHeight;
        uiData.result.score = score;
      //  uiData.hud.score = m_realtimeScore;
        uiData.result.sweetsCount = GetSweetsCount();
        uiData.result.timeUp = isTimeUp;
        uiData.result.moveSpeedMultiplier = m_playerSpeedMultiplier;
        if (m_player)
        {
            uiData.result.moveSpeedUnitsPerSecond = m_player->GetMoveSpeed();
        }

        uiData.showHud = g_DebugDrawEnabled && !isTimeUp;
        uiData.showResult = isTimeUp;
        m_uiOverlay->Update(uiData);
    }

    if (input && input->IsKeyPress(VK_RETURN))
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
    m_lastGuidelineHeight = m_stageData.GetGuidelineHeight();
}


#if defined(_DEBUG)
void SceneGame::HandleColliderDebugCommands(const Core::UpdateContext& ctx)
{
        (void)ctx;
        if (!m_player)
        {
                return;
        }

        Core::Input* input = GetInput();
        if (!input)
        {
                return;
        }
        if (input->IsTrigger(Core::ActionId::DebugToggle))
        {
                g_DebugDrawEnabled = !g_DebugDrawEnabled;
                std::ostringstream oss;
                oss << kLogPrefix << "[DebugDraw] r.DebugDrawEnabled="
                        << (g_DebugDrawEnabled ? 1 : 0) << " (F5).\n";
                DebugTrace(oss.str());

                const GuidelineSystem::UpdateInput guidelineInput{
                        m_sweetsManager.get(),
                        m_stageData.GetGuidelineTimeBonusSeconds(),
                        kGuidelineGlobalCooldownSeconds,
                        DebugTrace,
                        kLogPrefix
                };
                const float triggerHeight = m_guidelineSystem.GetHeightMeters();
                GuidelineSystem::UpdateResult guidelineResult =
                        m_guidelineSystem.DebugForceAdvance(guidelineInput, {}, triggerHeight);
                m_gameplayRules.AddPendingBonuses(std::move(guidelineResult.pendingBonuses));
                if (guidelineResult.guidelineAdvanced)
                {
                    if (auto* soundMgr = SoundManagerEX::GetInstance())
                    {
                        soundMgr->PlaySoundEX("Assets/Sound/SE_PhaseUp.wav", false, 0.8f);
                    }
                        const float guidelineDelta = guidelineResult.guidelineHeightMeters - m_lastGuidelineHeight;
                        m_lastGuidelineHeight = guidelineResult.guidelineHeightMeters;
                        UpdateCameraBoundsForPhase(guidelineResult.guidelineHeightMeters);
                        if (guidelineDelta != 0.0f)
                        {
                                if (m_sweetsManager)
                                {
                                        m_sweetsManager->OffsetSpawnOriginsY(guidelineDelta);
                                }

                                for (auto& obj : m_stageObjects)
                                {
                                        if (!IsOsara(obj))
                                        {
                                                continue;
                                        }

                                        obj.baseY += guidelineDelta;
                                        obj.transform.position.y += guidelineDelta;

                                        if (obj.physicsEntity)
                                        {
                                                obj.physicsEntity->SetPositionViaPhysics(obj.transform.position);
                                        }
                                }
                        }
                }
                std::ostringstream cheatLog;
                cheatLog << kLogPrefix << "[Cheat] forced guideline crossing (F5)"
                        << " height=" << std::fixed << std::setprecision(2)
                        << guidelineResult.guidelineHeightMeters << "m"
                        << " phaseUpCount=" << m_gameplayRules.GetPhaseUpCount() << "\n";
                DebugTrace(cheatLog.str());
        }

        if (input->IsTrigger(VK_F4))
        {
                g_DrawColliders = !g_DrawColliders;
                std::ostringstream oss;
                oss << kLogPrefix << "[ColliderDebug] r.DebugColliders="
                        << (g_DrawColliders ? 1 : 0) << " (F4).\n";
                DebugTrace(oss.str());
        }

        if (input->IsTrigger(VK_F6))
        {
                g_DrawColliderBounds = !g_DrawColliderBounds;
                std::ostringstream oss;
                oss << kLogPrefix << "[ColliderDebug] r.DebugColliderBounds="
                        << (g_DrawColliderBounds ? 1 : 0) << " (F6).\n";
                DebugTrace(oss.str());
        }
        constexpr float kDebugNudge = 0.25f;
        float3 delta{ 0.0f, 0.0f, 0.0f };

        if (input->IsTrigger(Core::ActionId::ColliderNudgeUp))
        {
                delta.y += kDebugNudge;
        }
        if (input->IsTrigger(Core::ActionId::ColliderNudgeDown))
        {
                delta.y -= kDebugNudge;
        }

        if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f)
        {
                return;
        }

        Entities::TranslatePhysicalEntity(*m_player, delta);

        const auto& transform = m_player->GetTransform();

        m_player->SyncColliderBounds();
        const AABB bounds = m_player->GetAABB();
        std::ostringstream oss;
        oss << kLogPrefix << "[ColliderDebug] Nudged player by (" << delta.x << ',' << delta.y << ',' << delta.z << ")"
            << " newPos=(" << transform.position.x << ',' << transform.position.y << ',' << transform.position.z << ")"
            << " aabbMin=(" << bounds.min.x << ',' << bounds.min.y << ',' << bounds.min.z << ")"
            << " aabbMax=(" << bounds.max.x << ',' << bounds.max.y << ',' << bounds.max.z << ")\n";
        ::DebugTrace(oss.str());
}
#endif

#if defined(_DEBUG)
void SceneGame::DrawPhysicsDebugOverlay(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
    if (!g_DebugDrawEnabled)
    {
        return;
    }

    if (m_cameraRig)
    {
        const auto clampMinY = m_cameraRig->GetClampMinY();
        const auto clampMaxY = m_cameraRig->GetClampMaxY();
       /* std::ostringstream oss;
        oss << kLogPrefix << "[DebugOverlay] cameraRig comfortTopY=" << m_cameraRig->GetComfortZoneTopY()
            << " comfortBottomY=" << m_cameraRig->GetComfortZoneBottomY()
            << " followSpeedY=" << m_cameraRig->GetFollowSpeedY()
            << " clampMinY=" << (clampMinY ? std::to_string(*clampMinY) : std::string("none"))
            << " clampMaxY=" << (clampMaxY ? std::to_string(*clampMaxY) : std::string("none"))
            << "\n";
        ::DebugTrace(oss.str());*/
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

    if (g_DrawColliders)
    {
        drawEntity(m_player.get());

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

        if (m_player)
        {
            //プレイヤーの描画用トランスドームを取得
            const Transform tf = m_player->GetRenderTransform();
            const float3 basePos = tf.position;
            //掴み枠の位置
            constexpr float kPickupForwardOffset = 0.9f;

            //掴み判定のボックスの半分サイズ
            const float halfW = m_playerPickupBoxWidth  * 0.5f;
            const float halfH = m_playerPickupBoxHeight * 0.5f;

            //プレイヤーの向き
            const float facingSign =
                m_player->GetFacingSign();
 
            //掴み判定ボックスの中心座標を計算
            //X方向のみ向きに応じて反転
            float3 center = {
               // basePos.x + m_playerPickupOffset.localOffset.x * facingSign,
                basePos.x + kPickupForwardOffset * facingSign,
                basePos.y + m_playerPickupOffset.localOffset.y,
                basePos.z
            };

            DirectX::XMFLOAT3 p0 = { center.x - halfW, center.y - halfH, center.z };
            DirectX::XMFLOAT3 p1 = { center.x + halfW, center.y - halfH, center.z };
            DirectX::XMFLOAT3 p2 = { center.x + halfW, center.y + halfH, center.z };
            DirectX::XMFLOAT3 p3 = { center.x - halfW, center.y + halfH, center.z };

            //赤い線
            DirectX::XMFLOAT4 pickupColor(1.0f, 0.0f, 0.0f, 1.0f);

            Geometory::AddLine(p0, p1, pickupColor);
            Geometory::AddLine(p1, p2, pickupColor);
            Geometory::AddLine(p2, p3, pickupColor);
            Geometory::AddLine(p3, p0, pickupColor);
        }

        if (g_RegisterOsaraColliders)
        {
            for (const auto& entity : m_stagePhysicsEntities)
            {
                drawEntity(entity.get());
            }
        }
    }

    if (activeCamera)
    {
        auto perspectiveCamera = std::dynamic_pointer_cast<CameraSpace::PerspectiveCamera>(activeCamera);
        if (perspectiveCamera && m_player)
        {
            const float comfortZoneTopPercentY = std::clamp(m_gameCameraConfig.comfortZoneTopPercentY, 0.0f, 1.0f);
            const float comfortZoneBottomPercentY = std::clamp(m_gameCameraConfig.comfortZoneBottomPercentY, 0.0f, 1.0f);
            float3 targetPosition = m_player->GetTransform().position;

            const DirectX::XMFLOAT4X4& view = activeCamera->GetView();
            DirectX::XMMATRIX viewMatrix = DirectX::XMLoadFloat4x4(&view);
            DirectX::XMVECTOR targetWorld = DirectX::XMVectorSet(targetPosition.x, targetPosition.y, targetPosition.z, 1.0f);
            DirectX::XMVECTOR targetView = DirectX::XMVector3TransformCoord(targetWorld, viewMatrix);
            const float viewZ = std::abs(DirectX::XMVectorGetZ(targetView));

            const float fovRadians = DirectX::XMConvertToRadians(perspectiveCamera->GetFieldOfViewDegrees());
            const float visibleHeight = 2.0f * std::tan(0.5f * fovRadians) * viewZ;
            const float visibleWidth = visibleHeight * perspectiveCamera->GetAspectRatio();
            const float comfortTopHeight = visibleHeight * comfortZoneTopPercentY;
            const float comfortBottomHeight = visibleHeight * comfortZoneBottomPercentY;

            if (IsFiniteFloat(visibleHeight) && IsFiniteFloat(visibleWidth) && visibleHeight > 0.0f)
            {
                const float halfWidth = visibleWidth * 0.5f;
                const float depth = viewZ;

                DirectX::XMMATRIX inverseView = DirectX::XMMatrixInverse(nullptr, viewMatrix);
                const auto toWorld = [&](float x, float y)
                {
                    DirectX::XMVECTOR viewPoint = DirectX::XMVectorSet(x, y, depth, 1.0f);
                    DirectX::XMVECTOR worldPoint = DirectX::XMVector3TransformCoord(viewPoint, inverseView);
                    DirectX::XMFLOAT3 worldValue;
                    DirectX::XMStoreFloat3(&worldValue, worldPoint);
                    return worldValue;
                };

                const DirectX::XMFLOAT4 zoneColor(0.2f, 1.0f, 0.2f, 0.85f);
                DirectX::XMFLOAT3 leftTop = toWorld(-halfWidth, comfortTopHeight);
                DirectX::XMFLOAT3 rightTop = toWorld(halfWidth, comfortTopHeight);
                DirectX::XMFLOAT3 leftBottom = toWorld(-halfWidth, -comfortBottomHeight);
                DirectX::XMFLOAT3 rightBottom = toWorld(halfWidth, -comfortBottomHeight);

                Geometory::AddLine(leftTop, rightTop, zoneColor);
                Geometory::AddLine(leftBottom, rightBottom, zoneColor);
                Geometory::AddLine(leftTop, leftBottom, zoneColor);
                Geometory::AddLine(rightTop, rightBottom, zoneColor);
            }
        }
    }

    if (m_sweetsManager)
    {
        SweetsSpace::SweetsContacts::GlueDebugSnapshot glueSnapshot;
        if (m_sweetsManager->GetGlueDebugSnapshot(glueSnapshot))
        {
            const float stagePlaneZ = (m_stagePlaneZ && IsFiniteFloat(*m_stagePlaneZ)) ? *m_stagePlaneZ : 0.0f;
            const DirectX::XMFLOAT4 anchorColor(1.0f, 0.85f, 0.2f, 0.95f);
            const DirectX::XMFLOAT4 linkColor(0.2f, 1.0f, 0.3f, 0.85f);
            const Color creamColor = MakeColor(0.2f, 0.6f, 1.0f, 0.9f);
            const Color fruitColor = MakeColor(0.8f, 0.3f, 1.0f, 0.9f);

            const auto drawCross = [&](const b2Vec2& point, float size, float z)
            {
                const DirectX::XMFLOAT3 center(point.x, point.y, z);
                Geometory::AddLine(
                    DirectX::XMFLOAT3(center.x - size, center.y, center.z),
                    DirectX::XMFLOAT3(center.x + size, center.y, center.z),
                    anchorColor);
                Geometory::AddLine(
                    DirectX::XMFLOAT3(center.x, center.y - size, center.z),
                    DirectX::XMFLOAT3(center.x, center.y + size, center.z),
                    anchorColor);
            };

            for (const b2Vec2& anchor : glueSnapshot.creamAnchors)
            {
                drawCross(anchor, 0.15f, stagePlaneZ);
            }

            for (const auto& link : glueSnapshot.fruitLinks)
            {
                float z = stagePlaneZ;
                if (link.cream)
                {
                    const float3 creamPos = link.cream->GetTransform().position;
                    z = creamPos.z;
                    DebugColliderRenderer::DrawCircle(float2{ creamPos.x, creamPos.y }, 0.35f, creamPos.z, creamColor);
                }
                if (link.fruit)
                {
                    const float3 fruitPos = link.fruit->GetTransform().position;
                    z = fruitPos.z;
                    DebugColliderRenderer::DrawCircle(float2{ fruitPos.x, fruitPos.y }, 0.28f, fruitPos.z,
                        link.fruitFrozen ? fruitColor : MakeColor(1.0f, 0.5f, 0.5f, 0.9f));
                }
                if (link.cream && link.fruit)
                {
                    const float3 creamPos = link.cream->GetTransform().position;
                    const float3 fruitPos = link.fruit->GetTransform().position;
                    Geometory::AddLine(
                        DirectX::XMFLOAT3(creamPos.x, creamPos.y, creamPos.z),
                        DirectX::XMFLOAT3(fruitPos.x, fruitPos.y, fruitPos.z),
                        linkColor);
                }
                drawCross(link.anchorWorld, 0.12f, z);
            }
        }
    }

    Geometory::DrawLines();
}
#endif

void SceneGame::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
        DrawWorld(activeCamera);
}

void SceneGame::DrawWorld(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
    // 描画確認用の数値
    if (!activeCamera)
    {
        DebugTrace(std::string(kLogPrefix) + "Draw skipped: no active camera registered.\n");
        return;
    }

    // scrollOffsetUsedForBackground is in world units (meters). pixelsPerUnit is derived from the active camera.
    float scrollOffsetUsedForBackground = 0.0f;
    float pixelsPerUnit = 1.0f;

    auto baseCamera = std::dynamic_pointer_cast<CameraSpace::BaseCamera>(activeCamera);
    assert(baseCamera && "SceneGame background requires a BaseCamera.");
    if (!baseCamera)
    {
        DebugTrace(std::string(kLogPrefix) + "Background skipped: active camera is not a BaseCamera.\n");
    }
    else
    {
        scrollOffsetUsedForBackground = baseCamera->GetPosition().y - m_backgroundScrollOriginY;
        if (auto perspectiveCamera = std::dynamic_pointer_cast<CameraSpace::PerspectiveCamera>(activeCamera))
        {
            RenderTarget* defaultRTV = GetDefaultRTV();
            if (defaultRTV)
            {
                const float viewportHeight = static_cast<float>(defaultRTV->GetHeight());
                if (viewportHeight > 0.0f)
                {
                    float3 planeSample{ 0.0f, 0.0f, 0.0f };
                    if (m_player)
                    {
                        planeSample = m_player->GetTransform().position;
                    }
                    if (m_stagePlaneZ && IsFiniteFloat(*m_stagePlaneZ))
                    {
                        planeSample.z = *m_stagePlaneZ;
                    }
                    const float viewZ = ComputeViewDepth(baseCamera->GetPosition(), baseCamera->GetRotation(), planeSample);
                    if (IsFiniteFloat(viewZ) && viewZ > 0.0f)
                    {
                        const float fovRadians = DirectX::XMConvertToRadians(perspectiveCamera->GetFieldOfViewDegrees());
                        const float visibleHeightWorld = 2.0f * std::tan(0.5f * fovRadians) * viewZ;
                        if (IsFiniteFloat(visibleHeightWorld) && visibleHeightWorld > 0.0f)
                        {
                            pixelsPerUnit = viewportHeight / visibleHeightWorld;
                        }
                    }
                }
            }
        }
        if (m_backgroundRenderer)
        {
            m_backgroundRenderer->Draw(scrollOffsetUsedForBackground, pixelsPerUnit);
        }
    }

        {
                ::LightingSettings lighting{};
                lighting.key.diffuse = m_lightingSettings.keyDiffuse;
                lighting.key.dir = m_lightingSettings.keyDir;
                lighting.key.wrap = m_lightingSettings.keyWrap;
                lighting.fill.diffuse = m_lightingSettings.fillDiffuse;
                lighting.fill.dir = m_lightingSettings.fillDir;
                lighting.fill.wrap = m_lightingSettings.fillWrap;
                if (!m_lightingSettings.keyEnable)
                {
                        lighting.key.diffuse = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
                }
                if (!m_lightingSettings.fillEnable)
                {
                        lighting.fill.diffuse = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
                }
                lighting.hemi.sky = m_lightingSettings.hemiSky;
                lighting.hemi.ground = m_lightingSettings.hemiGround;
                lighting.rim.color = m_lightingSettings.rimColor;
                lighting.rim.params = m_lightingSettings.rimParams;
                lighting.rim.enable = m_lightingSettings.rimEnable ? 1.0f : 0.0f;
                ShaderList::SetLightingMode(m_lightingSettings.mode);
                ShaderList::SetLighting(lighting);

                if (auto baseCamera = std::dynamic_pointer_cast<CameraSpace::BaseCamera>(activeCamera))
                {
                        const float3& cameraPos = baseCamera->GetPosition();
                        ShaderList::SetCameraPos(DirectX::XMFLOAT3(cameraPos.x, cameraPos.y, cameraPos.z));
                }
        }

        if (m_renderer)
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

                    m_renderer->Draw(*obj.model, renderTransform, activeCamera.get(), nullptr);
                }
                else
                {
                    DebugTrace(std::string(kLogPrefix) + "モデルがnull\n");
                }
            }
        }
        else
        {
            DebugTrace(std::string(kLogPrefix) + "m_rendererがnull\n");
        }

        if (m_blobShadowRenderer && m_blobShadowSettings.enable)
        {
            std::vector<AABB> stageBounds;
            stageBounds.reserve(m_stagePhysicsEntities.size());
            for (const auto& stageEntity : m_stagePhysicsEntities)
            {
                if (!stageEntity)
                {
                    continue;
                }

                stageEntity->SyncColliderBounds();
                stageBounds.push_back(stageEntity->GetAABB());
            }

            std::optional<float> receiverZ = m_stagePlaneZ;
            if (m_blobShadowSettings.groundZ)
            {
                receiverZ = m_blobShadowSettings.groundZ;
            }

            const auto resolveShadowReceiver = [&](Entities::PhysicalEntity& entity)
            {
                entity.SyncColliderBounds();
                const AABB& bounds = entity.GetAABB();
                float receiverY = m_blobShadowSettings.groundY;
                TryFindShadowReceiverYFromStage(bounds, stageBounds, &receiverY);
                return std::make_pair(receiverY, ComputeReceiverHeightFromBounds(bounds, receiverY));
            };

            if (m_player)
            {
                const auto [receiverY, receiverHeight] = resolveShadowReceiver(*m_player);
                m_blobShadowRenderer->DrawShadow(*m_player, *activeCamera, receiverY, receiverZ, receiverHeight);
            }
            if (m_sweetsManager)
            {
                const auto drawList = [&](const SweetsSpace::SweetsManager::SweetsList& list)
                {
                    for (const auto& sweets : list)
                    {
                        if (sweets)
                        {
                            const auto [receiverY, receiverHeight] = resolveShadowReceiver(*sweets);
                            m_blobShadowRenderer->DrawShadow(*sweets, *activeCamera, receiverY, receiverZ, receiverHeight);
                        }
                    }
                };

                drawList(m_sweetsManager->GetByState(SweetsSpace::SweetsState::Active));
                drawList(m_sweetsManager->GetByState(SweetsSpace::SweetsState::Stacked));
                drawList(m_sweetsManager->GetByState(SweetsSpace::SweetsState::Ground));
            }
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
        DrawPhysicsDebugOverlay(activeCamera);
#endif

}

void SceneGame::DrawOverlay()
{
    if (m_uiOverlay)
    {
        m_uiOverlay->Draw();
    }

    if (!m_gameStarted && m_startCountdownNumber >= 0)
    {
        Sprite::ScopedPassDefaults guard;

        Sprite::SetTexture(m_countdownTextures[m_startCountdownNumber].get());

        // 画面中央に表示（サイズは調整）
        Sprite::SetOffset({ 0.0f, 0.0f }); 
        Sprite::SetSize({ 1.0f, 1.0f });   
        Sprite::SetColor({ 1,1,1,1 });
        Sprite::Draw();
    }

    // FINISH表示
    if (m_stageState == StageState::Finish)
    {
        Sprite::ScopedPassDefaults guard;

        Sprite::SetTexture(m_finishTexture.get());

        // 画面中央に表示
        Sprite::SetOffset({ 0.0f, 0.0f });
        Sprite::SetSize({ 1.0f, 1.0f }); 
        Sprite::SetColor({ 1,1,1,1 });
        Sprite::Draw();
    }

#if defined(_DEBUG)
#endif
}

Systems::IPhysicsBinder* SceneGame::GetPhysicsBinder()
{
    return m_physicsBinder.get();
}

Render::ModelRenderer* SceneGame::GetRenderer()
{
    return m_renderer;
}

Core::UpdateScheduler& SceneGame::GetScheduler()
{
    return GetUpdateScheduler();
}

Core::Input* SceneGame::GetInput()
{
    Core::Input* sceneInput = SceneSpace::Scene::GetInput();
    return sceneInput ? sceneInput : m_inputService;
}

Core::StepTimer* SceneGame::GetTimer()
{
    return SceneSpace::Scene::GetTimer();
}

void SceneGame::ConfigureCamera(const SceneGameConfig::CameraRigSettings& settings,
    const SceneGameConfig::GameCameraConfig& gameCameraConfig)
{
    if (settings.mode != SceneGameConfig::CameraRigMode::Follow)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Gameplay ignores CameraRigMode=" << static_cast<int>(settings.mode)
            << " (Fixed/Cinematic should live in non-gameplay scenes).\n";
        ::DebugTrace(oss.str());
    }

    SceneGameConfig::CameraRigSettings resolvedSettings = ApplyGameplayCameraOverrides(settings);

    // Clamp bounds come from stage config (StageData / gameplay.cfg), not decoration objects.
    std::optional<float> visualMinY = settings.clamp.minY;
    std::optional<float> visualMaxY = settings.clamp.maxY;
    const bool shouldClampY = visualMinY || visualMaxY;

    if (m_gameCamera)
    {
        m_gameCamera->SetFieldOfViewDegrees(resolvedSettings.fovDegrees);
    }

    const float yawRadians = DirectX::XMConvertToRadians(resolvedSettings.yawDegrees);
    const float pitchRadians = DirectX::XMConvertToRadians(resolvedSettings.pitchDegrees);
    const float followSpeed = resolvedSettings.fixedPosition ? 0.0f : resolvedSettings.followSpeed;
    const float followSpeedY = resolvedSettings.fixedPosition ? 0.0f : gameCameraConfig.followSpeedY;

    float3 targetPosition{ 0.0f, 0.0f, 0.0f };
    bool hasTarget = false;
    if (m_player)
    {
        targetPosition = m_player->GetTransform().position;
        if (!resolvedSettings.fixedPosition)
        {
            targetPosition.y += gameCameraConfig.offsetY;
        }
        hasTarget = true;
    }

    float3 cameraRotation{ pitchRadians, yawRadians, 0.0f };
    float3 cameraPosition = resolvedSettings.offset;
    if (!resolvedSettings.fixedPosition && hasTarget)
    {
        DirectX::XMVECTOR anchorPos = CameraSpace::LoadFloat3(targetPosition);
        DirectX::XMVECTOR rotatedOffset = CameraSpace::RotateOffset(resolvedSettings.offset, cameraRotation.y, cameraRotation.x);
        DirectX::XMVECTOR desiredPos = DirectX::XMVectorAdd(anchorPos, rotatedOffset);
        cameraPosition = CameraSpace::StoreFloat3(desiredPos);
    }

    float comfortZoneTopY = 0.0f;
    float comfortZoneBottomY = 0.0f;
    std::optional<float> halfViewHeightWorld = std::nullopt;
    if (!resolvedSettings.fixedPosition && hasTarget)
    {
        const float comfortZoneTopPercentY = std::clamp(gameCameraConfig.comfortZoneTopPercentY, 0.0f, 1.0f);
        const float comfortZoneBottomPercentY = std::clamp(gameCameraConfig.comfortZoneBottomPercentY, 0.0f, 1.0f);
        float3 planeSample = targetPosition;
        if (m_stagePlaneZ && IsFiniteFloat(*m_stagePlaneZ))
        {
            planeSample.z = *m_stagePlaneZ;
        }
        const float viewDepth = ComputeViewDepth(cameraPosition, cameraRotation, planeSample);
        if (IsFiniteFloat(viewDepth) && viewDepth > 0.0f)
        {
            const float fovRadians = DirectX::XMConvertToRadians(resolvedSettings.fovDegrees);
            const float visibleHeight = 2.0f * std::tan(0.5f * fovRadians) * viewDepth;
            if (IsFiniteFloat(visibleHeight) && visibleHeight > 0.0f)
            {
                halfViewHeightWorld = 0.5f * visibleHeight;
                comfortZoneTopY = visibleHeight * comfortZoneTopPercentY;
                comfortZoneBottomY = visibleHeight * comfortZoneBottomPercentY;
            }
        }
    }

    std::optional<float> minScrollY = visualMinY;
    std::optional<float> maxScrollY = visualMaxY;
    if (shouldClampY && halfViewHeightWorld && IsFiniteFloat(*halfViewHeightWorld))
    {
        if (minScrollY)
        {
            minScrollY = *minScrollY + *halfViewHeightWorld;
        }
        if (maxScrollY)
        {
            maxScrollY = *maxScrollY - *halfViewHeightWorld;
        }
    }

    if (minScrollY && maxScrollY && *minScrollY > *maxScrollY)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Camera scroll clamp range invalid after view adjustment (minY=" << *minScrollY
            << ", maxY=" << *maxScrollY << "). Using visual bounds instead.\n";
        ::DebugTrace(oss.str());
        minScrollY = visualMinY;
        maxScrollY = visualMaxY;
    }

    if (m_cameraRig)
    {
        m_cameraRig->SetOffset(resolvedSettings.offset);
        m_cameraRig->SetYaw(yawRadians);
        m_cameraRig->SetPitch(pitchRadians);
        m_cameraRig->SetFollowSpeed(resolvedSettings.followSpeed);
        m_cameraRig->SetFollowSpeedY(followSpeedY);
        m_cameraRig->SetTranslationLocked(false);
        m_cameraRig->SetYawPitchLocked(true);
        m_cameraRig->SetFollowPositionAxes(false, true, false);
        m_cameraRig->SetInheritRotation(false);
        m_cameraRig->SetComfortZoneY(comfortZoneTopY, comfortZoneBottomY);
        m_cameraRig->SetClampY(shouldClampY ? minScrollY : std::nullopt, shouldClampY ? maxScrollY : std::nullopt);

        const bool followAxesOk = !m_cameraRig->IsFollowingPositionX()
            && m_cameraRig->IsFollowingPositionY()
            && !m_cameraRig->IsFollowingPositionZ();
        const bool flagsOk = followAxesOk
            && !m_cameraRig->IsTranslationLocked()
            && m_cameraRig->IsYawPitchLocked()
            && !m_cameraRig->IsInheritingRotation();
        const float expectedFollowSpeedY = followSpeedY;
        const bool speedOk = std::abs(m_cameraRig->GetFollowSpeedY() - followSpeedY) <= 0.001f;

        if (!flagsOk || !speedOk)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Gameplay camera rig flags invalid. followAxesOk=" << std::boolalpha
                << followAxesOk << " translationLocked=" << m_cameraRig->IsTranslationLocked()
                << " yawPitchLocked=" << m_cameraRig->IsYawPitchLocked()
                << " inheritRotation=" << m_cameraRig->IsInheritingRotation()
                << " followSpeedY=" << m_cameraRig->GetFollowSpeedY()
                << " expectedFollowSpeedY=" << expectedFollowSpeedY << "\n";
            ::DebugTrace(oss.str());
            assert(false && "Gameplay camera rig flags are invalid.");
        }
    }

    if (!m_gameCamera)
    {
        DebugTrace(std::string(kLogPrefix) + "ConfigureCamera skipped: gameplay camera missing.\n");
        return;
    }

    if (!resolvedSettings.fixedPosition && hasTarget)
    {
        if (m_cameraRig)
        {
            targetPosition.y = m_cameraRig->ClampScrollY(targetPosition.y);
            m_cameraRig->ResetVerticalScroll(targetPosition);
        }

        DirectX::XMVECTOR anchorPos = CameraSpace::LoadFloat3(targetPosition);
        DirectX::XMVECTOR rotatedOffset = CameraSpace::RotateOffset(resolvedSettings.offset, cameraRotation.y, cameraRotation.x);
        DirectX::XMVECTOR desiredPos = DirectX::XMVectorAdd(anchorPos, rotatedOffset);
        cameraPosition = CameraSpace::StoreFloat3(desiredPos);
    }

    m_gameCamera->SetPosition(cameraPosition);
    m_backgroundScrollOriginY = m_gameCamera->GetPosition().y;
    m_gameCamera->SetRotation(cameraRotation);
// BaseCamera::RecalculateViewを直接呼び出す代わりに、パブリック API 経由でビューの更新を強制
// BaseCamera::RecalculateViewは保護されたメソッドです GetView はトランスフォームが変更された際に遅延評価で再計算
    (void)m_gameCamera->GetView();

    static bool loggedCameraSettings = false;
    if (!loggedCameraSettings)
    {
        std::ostringstream settingsLog;
        settingsLog << kLogPrefix << "Camera settings: fixed=" << std::boolalpha << resolvedSettings.fixedPosition
            << ", translationLocked=" << resolvedSettings.lockTranslation
            << ", followSpeed=" << resolvedSettings.followSpeed << "\n";
        ::DebugTrace(settingsLog.str());
        loggedCameraSettings = true;
    }

    std::ostringstream oss;
    oss << kLogPrefix << "Configured camera prototype: fixed=" << std::boolalpha << resolvedSettings.fixedPosition
        << ", fov=" << resolvedSettings.fovDegrees
        << ", followSpeed=" << resolvedSettings.followSpeed
        << ", offset=" << resolvedSettings.offset.x << ',' << resolvedSettings.offset.y << ',' << resolvedSettings.offset.z
        << ", yawDegrees=" << resolvedSettings.yawDegrees << ", pitchDegrees=" << resolvedSettings.pitchDegrees << "\n";
    if (resolvedSettings.fixedPosition)
    {
        oss << kLogPrefix << "[CameraStatic] Final position (" << cameraPosition.x << ',' << cameraPosition.y << ','
            << cameraPosition.z << ") yaw=" << resolvedSettings.yawDegrees << "deg pitch=" << resolvedSettings.pitchDegrees << "deg\n";
    }
    ::DebugTrace(oss.str());

}

void SceneGame::UpdateCameraBoundsForPhase(float heightMeters)
{
    if (!m_cameraRig)
    {
        return;
    }

    std::optional<float> visualMinY = m_cameraRigSettings.clamp.minY;
    std::optional<float> visualMaxY = m_cameraRigSettings.clamp.maxY;
    if (IsFiniteFloat(heightMeters) && heightMeters > 0.0f)
    {
        if (visualMaxY)
        {
            visualMaxY = std::max(*visualMaxY, heightMeters);
        }
        else
        {
            visualMaxY = heightMeters;
        }
    }

    const bool shouldClampY = visualMinY || visualMaxY;
    std::optional<float> halfViewHeightWorld = std::nullopt;
    if (m_gameCamera && m_player)
    {
        float3 planeSample = m_player->GetTransform().position;
        if (m_stagePlaneZ && IsFiniteFloat(*m_stagePlaneZ))
        {
            planeSample.z = *m_stagePlaneZ;
        }

        const float3 cameraPosition = m_gameCamera->GetPosition();
        const float3 cameraRotation = m_gameCamera->GetRotation();
        const float viewDepth = ComputeViewDepth(cameraPosition, cameraRotation, planeSample);
        if (IsFiniteFloat(viewDepth) && viewDepth > 0.0f)
        {
            const float fovRadians = DirectX::XMConvertToRadians(m_gameCamera->GetFieldOfViewDegrees());
            const float visibleHeight = 2.0f * std::tan(0.5f * fovRadians) * viewDepth;
            if (IsFiniteFloat(visibleHeight) && visibleHeight > 0.0f)
            {
                halfViewHeightWorld = 0.5f * visibleHeight;
            }
        }
    }

    std::optional<float> minScrollY = visualMinY;
    std::optional<float> maxScrollY = visualMaxY;
    if (shouldClampY && halfViewHeightWorld && IsFiniteFloat(*halfViewHeightWorld))
    {
        if (minScrollY)
        {
            minScrollY = *minScrollY + *halfViewHeightWorld;
        }
        if (maxScrollY)
        {
            maxScrollY = *maxScrollY - *halfViewHeightWorld;
        }
    }

    if (minScrollY && maxScrollY && *minScrollY > *maxScrollY)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Camera scroll clamp range invalid after phase update (minY=" << *minScrollY
            << ", maxY=" << *maxScrollY << "). Using visual bounds instead.\n";
        ::DebugTrace(oss.str());
        minScrollY = visualMinY;
        maxScrollY = visualMaxY;
    }

    m_cameraRig->SetClampY(shouldClampY ? minScrollY : std::nullopt,
                           shouldClampY ? maxScrollY : std::nullopt);
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
    case StageState::ShowingResult:
        OnStageResult();
        break;
    default:
        break;
    }
}

void SceneGame::OnStageStart()
{
    ResetPlaySession();

    m_whistlePlayed = false;
    m_whistleTimer = 0.0f;
    m_bgmStarted = false;

    m_resultFinalized = false;
    m_resultSaved = false;

    m_startCountdownNumber = 3;
    m_startCountdownTimer = 0.0f;
    m_gameStarted = false;

    ToggleGameplaySystems(false);
}

void SceneGame::OnStageClear()
{
    ToggleGameplaySystems(false);

    if (auto* soundMgr = SoundManagerEX::GetInstance())
    {
        soundMgr->PlaySoundEX("Assets/Audio/SE_ResultPop.wav", false, 0.5f);
    }

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

void SceneGame::OnStageResult()
{
    if (m_resultFinalized)
    {
        return;
    }

    m_resultFinalized = true;
    const int score = m_gameplayRules.ComputeScore(&m_phaseManager);
    const int earnedPoints = ConvertScoreToPoints(score);
    const int currentPoints = std::max(0, m_playerPowerUpData.nPoints);
    const int updatedPoints = currentPoints + earnedPoints;

    PlayerData::Data updatedEntry = m_playerPowerUpData;
    updatedEntry.nPoints = updatedPoints;

    std::vector<PlayerData::Data> entries = m_playerData.GetEntries();
    if (entries.empty())
    {
        entries.push_back(updatedEntry);
    }
    else
    {
        entries.front() = updatedEntry;
    }
    m_playerData.SetEntries(entries);
    m_resultSaved = m_playerData.SaveData();
    if (!m_resultSaved)
    {
        DebugTrace(std::string(kLogPrefix) + "[PlayerData] Failed to save points.\n");
    }
    m_playerPowerUpData = updatedEntry;

    std::ostringstream oss;
    oss << kLogPrefix << "[Result] score=" << score
        << " earnedPoints=" << earnedPoints
        << " totalPoints=" << updatedPoints << "\n";
    ::DebugTrace(oss.str());
}

bool SceneGame::EnsureResultSaved()
{
    if (!m_resultFinalized)
    {
        OnStageResult();
        return m_resultSaved;
    }

    if (m_resultSaved)
    {
        return true;
    }

    m_resultSaved = m_playerData.SaveData();
    if (!m_resultSaved)
    {
        DebugTrace(std::string(kLogPrefix) + "[PlayerData] Failed to save points.\n");
    }

    return m_resultSaved;
}

void SceneGame::BroadcastPhaseAdvance(int newPhase)
{
    std::ostringstream oss;
    const float peakHeight = m_gameplayRules.GetPeakTowerHeight();
    oss << kLogPrefix << "Broadcasting phase " << newPhase
        << " (stackedPeak=" << std::fixed << std::setprecision(2) << peakHeight << ")\n";
    ::DebugTrace(oss.str());
    // BroadcastPhaseAdvance は、他のセッション制御が開始される前にここで終了すべき
}

void SceneGame::ResetStageObjectsToInitial()
{
    if (m_stageObjectsInitial.empty())
    {
        return;
    }

    if (m_stageObjects.size() != m_stageObjectsInitial.size())
    {
        m_stageObjects = m_stageObjectsInitial;
    }
    else
    {
        for (size_t index = 0; index < m_stageObjects.size(); ++index)
        {
            auto& obj = m_stageObjects[index];
            const auto& initial = m_stageObjectsInitial[index];
            obj.transform = initial.transform;
            obj.baseY = initial.baseY;
            obj.swaiTime = initial.swaiTime;
            obj.currentRotation = initial.currentRotation;
        }
    }

    for (auto& obj : m_stageObjects)
    {
        if (obj.physicsEntity)
        {
            obj.physicsEntity->SetPositionViaPhysics(obj.transform.position);
        }
    }
}

void SceneGame::ResetPlaySession()
{
    if (m_physicsBinder)
    {
        m_physicsBinder->Enable(false);
        m_physicsBinder->ResetContactListenerCaches();
    }

    if (m_sweetsManager)
    {
        m_sweetsManager->ClearAllSweets();
        m_sweetsManager->SetSpawnOrigins(m_spawnOriginsPrimaryInitial, m_spawnOriginsSecondaryInitial);
        m_sweetsManager->ForceSpawnRefresh();
        m_sweetsManager->SeedInitialSpawns();
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "ResetPlaySession could not clear sweets: manager missing.\n");
    }

    ResetStageObjectsToInitial();

    {
        std::ostringstream resetLog;
        resetLog << kLogPrefix << "[RetryReset] restored stage objects + spawn origins\n";
        ::DebugTrace(resetLog.str());
    }

    if (m_player)
    {
        m_player->ClearCarriedSweets();
        if (m_playerBaseHorizontalSpeed > 0.0f || m_playerBaseVerticalSpeed > 0.0f)
        {
            m_playerSpeedMultiplier = m_playerSpeedMultiplierBase;
            m_player->SetBaseMoveSpeed(m_playerBaseHorizontalSpeed, m_playerBaseVerticalSpeed);
            m_player->SetSpeedMultiplier(m_playerSpeedMultiplier);
        }

        m_player->SetPickupArea(m_playerPickupBoxWidth, m_playerPickupBoxHeight, m_playerPickupOffset);
        m_player->SetPickupMaxDistance(m_playerPickupMaxDistance);
        m_player->SetPickupDepthThreshold(m_playerPickupDepthThreshold);
        m_player->SetPickupOriginOffset(m_playerPickupOriginOffset);

        const Transform& spawnTransform = m_playerSpawnTransform.transform;
        m_player->SetTransformViaPhysics(spawnTransform.position, spawnTransform.rotation);
        m_player->SetScale(spawnTransform.scale);
        m_player->SetLinearVelocity({ 0.0f, 0.0f, 0.0f });
        m_player->SetAngularVelocity(0.0f);
    }

    if (m_player && m_cameraRig)
    {
        m_cameraRig->ResetVerticalScroll(m_playerSpawnTransform.transform.position);
    }
    else if (!m_cameraRig)
    {
        DebugTrace(std::string(kLogPrefix) + "ResetPlaySession skipped camera rig realignment: camera rig missing.\n");
    }

    m_gameplayRules.ResetSession();
    m_realtimeScore = m_gameplayRules.GetCurrentScore();
    m_guidelineSystem.ResetSession();
    ResetGuidelineHeight();
    ToggleGameplaySystems(true);
    m_phaseManager.Reset();

    std::ostringstream oss;
    oss << kLogPrefix << "[Countdown] start duration=" << m_gameplayRules.GetTotalPlaySeconds() << "s\n";
    ::DebugTrace(oss.str());
}

void SceneGame::ApplyPowerUpEffects(const PlayerData::Data& data)
{
    const float speedMultiplier = std::max(0.0f, m_playerSpeedMultiplierBase * (1.0f + data.fSpeed));
    m_playerSpeedMultiplierBase = speedMultiplier;
    m_playerSpeedMultiplier = speedMultiplier;
    if (m_player)
    {
        m_player->SetSpeedMultiplier(speedMultiplier);
    }

    m_gameplayRules.SetScoreMultiplier(data.fPtMultipl);

    if (m_player)
    {
        const int carryBonus = std::max(0, data.nReplenishCount);
        m_player->SetMaxCarry(1 + carryBonus);
    }

    if (m_sweetsManager)
    {
        const float spawnMultiplier = std::max(0.01f, 1.0f + data.fReplenishSpeed);
        m_sweetsManager->SetSpawnIntervalMultiplier(spawnMultiplier);
        m_sweetsManager->SetSpawnCountBonus(std::max(0, data.nReplenishCount));
        m_sweetsManager->SetCreamEnabled(data.creamEnabled);
    }

}

void SceneGame::LoadPlayerPowerUpData()
{
    m_playerPowerUpLoaded = m_playerData.LoadData();
    if (m_playerPowerUpLoaded)
    {
        const auto& entries = m_playerData.GetEntries();
        if (!entries.empty())
        {
            m_playerPowerUpData = entries.front();
        }
        else
        {
            m_playerPowerUpLoaded = false;
            DebugTrace(std::string(kLogPrefix) + "[PlayerData] No entries found; using defaults.\n");
        }
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "[PlayerData] Load failed; using defaults.\n");
    }
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
    if (m_player)
    {
        m_player->Enable(enabled);
    }
}

int SceneGame::GetSweetsCount() const
{
    if (m_sweetsManager)
        return m_sweetsManager->GetSweetsCount();
    return 0;
}



//Osara探す？やつ
bool SceneGame::IsOsara(const StageObject& obj)
{
    return obj.modelPath.find("osara") != std::string::npos;
}

void SceneGame::UpdateOsaraGradualMove(const Core::UpdateContext& ctx)
{
    if (!m_isOsaraCounting || m_OsaraToAddRemaining <= 0.0f)
    {
        return;
    }
    //徐々に上に移動したい

    if (m_isOsaraCounting && m_OsaraToAddRemaining > 0.0f)
    {
        float deltaY = osaraMoveSpeed * ctx.dt;

        if (deltaY > m_OsaraToAddRemaining)
        {
            deltaY = m_OsaraToAddRemaining;
        }
        //  float moveY = guidelineDelta - deltaY;

        for (auto& obj : m_stageObjects)
        {
            if (!IsOsara(obj)) continue;

            obj.baseY += deltaY;
            obj.transform.position.y += deltaY;

            if (obj.physicsEntity)
            {
                obj.physicsEntity->SetPositionViaPhysics(obj.transform.position);
            }
        }
        m_OsaraToAddRemaining -= deltaY;

        if (m_OsaraToAddRemaining <= 0.0f)
        {
            m_isOsaraCounting = false;
        }
    }
}