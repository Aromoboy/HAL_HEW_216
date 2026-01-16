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
#include "Systems/Physics/CollisionLayer.h"
#include "Systems/Draw/DrawSystem.h"
#include "BoxColliderComponent.h"
#include "DirectX.h"
#include "BaseCamera.h"
#include "PerspectiveCamera.h"
#include "SweetsManager.h"
#include "Sweets/BaseSweets.h"
#include "Sweets/SweetsPrototypeManifest.h"
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
#include "CameraMath.h"
#include "AssimpModelLorder.h"
#include "ModelCache.h"
#include "ShaderCache.h"
#include <cassert>
#include <algorithm>
#include <chrono>
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
        constexpr bool kEnableStageModelVerboseLogging = false;
        constexpr float kDefaultPlaySeconds = 30.0f;
        constexpr float kCountdownLogIntervalSeconds = 5.0f;
        constexpr float kBaseScorePerMeter = 100.0f;
        constexpr float kShadowReceiverTolerance = 0.05f;
        SceneGameConfig::CameraRigSettings ApplyGameplayCameraOverrides(SceneGameConfig::CameraRigSettings settings)
        {
            settings.fixedPosition = false;
            settings.lockTranslation = false;
            settings.lockYawPitch = true;
            return settings;
        }

        bool IsFiniteFloat(float value)
        {
            return std::isfinite(value) != 0;
        }

        float Lerp(float from, float to, float t)
        {
            return from + (to - from) * t;
        }

        float SmoothStep(float edge0, float edge1, float x)
        {
            if (!IsFiniteFloat(edge0) || !IsFiniteFloat(edge1) || !IsFiniteFloat(x) || edge0 == edge1)
            {
                return (x < edge0) ? 0.0f : 1.0f;
            }

            const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        bool OverlapsShadowReceiverXZ(const AABB& a, const AABB& b)
        {
            return a.min.x <= b.max.x && a.max.x >= b.min.x;
        }

        float ResolvePlayerYaw(const Transform& transform)
        {
            constexpr float kYawAxisEpsilon = 1.0e-4f;

            const float absYawZ = std::fabs(transform.rotation.z);
            if (absYawZ > kYawAxisEpsilon)
            {
                return transform.rotation.z;
            }

            return transform.rotation.y;
        }

        float3 ForwardFromYaw(float yaw)
        {
            return float3{ std::cos(yaw), std::sin(yaw), 0.0f };
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
            const SceneGameConfig::ModelImportSettings modelImportSettings = SceneGameConfig::LoadModelImportSettings();
            modelCache.SetLoader(
                std::make_unique<AssimpModelLorder>(std::chrono::milliseconds(modelImportSettings.importTimeoutMs)),
                device,
                textureCache);
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
    composer.Compose(*composition);
    m_playerSpawnTransform = composition->playerSpawnTransform;
    m_stageData = std::move(composition->stageData);
    m_stagePlaneZ = std::nullopt;
    const float stagePlaneZ = m_stageData.GetPlaneZ();
    if (IsFiniteFloat(stagePlaneZ))
    {
        m_stagePlaneZ = stagePlaneZ;
    }
    m_phaseManager = std::move(composition->phaseManager);
    m_playerBaseHorizontalSpeed = composition->playerBaseHorizontalSpeed;
    m_playerBaseVerticalSpeed = composition->playerBaseVerticalSpeed;
    m_playerSpeedMultiplier = composition->playerSpeedMultiplier;
    m_renderPostProcessSettings = composition->renderPostProcessSettings;
    m_lightingSettings = composition->lightingSettings;
    m_blobShadowSettings = composition->blobShadowSettings;
    m_physicsBinder = std::move(composition->physicsBinder);
    m_drawSystem = std::move(composition->drawSystem);
    m_sweetsManager = std::move(composition->sweetsManager);
    m_uiManager = std::move(composition->uiManager);
    m_backgroundRenderer = std::move(composition->backgroundRenderer);
    m_blobShadowRenderer = std::make_unique<BlobShadowRenderer>();
    if (m_blobShadowRenderer)
    {
        m_blobShadowRenderer->SetSettings(m_blobShadowSettings);
    }

    GameplayRulesSystem::Config gameplayRulesConfig{};
    gameplayRulesConfig.defaultPlaySeconds = kDefaultPlaySeconds;
    gameplayRulesConfig.countdownLogIntervalSeconds = kCountdownLogIntervalSeconds;
    gameplayRulesConfig.baseScorePerMeter = kBaseScorePerMeter;
    m_gameplayRules.Configure(gameplayRulesConfig, DebugTrace, kLogPrefix);

    if (m_physicsBinder)
    {
        m_contactHandler.Bind(*m_physicsBinder, m_sweetsManager.get());
    }

    if (m_sweetsManager)
    {
        m_sweetsManager->SetPhaseChangeCallback([this](int newPhase)
        {
            ForwardSweetsPhaseChange(newPhase);
        });
    }

    m_playerColliderMismatchLogged = false;
    SceneGameLoader loader(DebugTrace, kLogPrefix);
    m_renderer = &App::Game::Instance().GetModelRenderer();
    SceneGameLoader::Config loaderConfig{};
    loaderConfig.device = GetDevice();
    loaderConfig.services = this;
    loaderConfig.physicsBinder = m_physicsBinder.get();
    loaderConfig.drawSystem = m_drawSystem.get();
    loaderConfig.playerSpawnTransform = m_playerSpawnTransform;
    loaderConfig.playerBaseHorizontalSpeed = m_playerBaseHorizontalSpeed;
    loaderConfig.playerBaseVerticalSpeed = m_playerBaseVerticalSpeed;
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
    player = std::move(loaderResult.player);
    m_stageObjects = std::move(composition->stageObjects);
    m_stagePhysicsEntities = std::move(composition->stagePhysicsEntities);

    RenderTarget* pRTV = GetDefaultRTV();
    DepthStencil* pDSV = GetDefaultDSV();
    SetRenderTargets(1, &pRTV, pDSV);
    SetDepthTest(true);

    SceneGameConfig::CameraRigSettings cameraSettings = SceneGameConfig::LoadCameraRigSettings();
    m_gameCameraConfig = SceneGameConfig::LoadGameCameraConfig(); // Source of truth: gameplay.cfg [camera.rig].
    m_cameraRigSettings = cameraSettings;
    m_missileSettings = SceneGameConfig::LoadMissileSettings();
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
        if (player)
        {
            m_cameraRig->Attach(player.get());
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
                postSettings.grainEnable ? 1 : 0);

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

    // BGMを再生（ループあり、音量1%）
    if (m_pManager)
    {
        m_pManager->PlaySoundEX("Assets/Audio/BGM_MainGame.wav", true, 0.01f);  // ループ有効
    }
}

void SceneGame::OnExit()
{
    std::ostringstream oss;
    oss << kLogPrefix << "Exit scenePtr=" << this << "\n";
    ::DebugTrace(oss.str());
}


SceneGame::SceneGame(SceneSpace::SceneManager& a_manager)
    : m_contactHandler()
    , m_playerController(DebugTrace, kLogPrefix)
    , m_manager(&a_manager)
{
    m_pManager = std::make_unique<SoundManagerEX>();
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
                                DebugTrace(std::string(kLogPrefix) + "[Fatal] Player physics body unavailable.\n");
                        }
                }
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
        UpdateCameraBoundsForPhase(m_gameplayRules.GetMaxTowerHeight());
        BroadcastPhaseAdvance(rulesResult.currentPhase);
    }

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

    float deltaTime = ctx.timer.DeltaTime();

    m_playerController.UpdatePlayerBounds({
        player.get(),
        m_gameCamera.get(),
        m_sweetsManager.get(),
        ctx.dt
    });
    if (player)
    {
        if (const auto wrapDelta = player->ConsumeWrapDeltaY())
        {
            ApplyPlayerWrapDeltaY(*wrapDelta);
        }
    }

    switch (m_stageState)
    {
    case StageState::Intro:
        break;
    case StageState::Playing:
    {
        if (input)
        {
            PlayerController::SpeedInput speedInput{
                *input,
                player.get(),
                m_playerBaseHorizontalSpeed,
                m_playerBaseVerticalSpeed,
                m_playerSpeedMultiplier
            };
            m_playerSpeedMultiplier = m_playerController.HandleSpeedInput(speedInput).speedMultiplier;

            if (input->IsTrigger(Core::ActionId::FireMissile))
            {
                FireMissile();
            }
        }

        const float elapsedSeconds = m_gameplayRules.GetTotalPlaySeconds()
            - m_gameplayRules.GetRemainingCountdownSeconds();
        const float peakHeight = m_gameplayRules.GetPeakTowerHeight();
        if (input && m_playerController.HandleManualRetryInput(*input, elapsedSeconds, peakHeight))
        {
            SetStageState(StageState::Playing);
        }
        fRads.z += 0.001f;
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
        const float peakHeight = m_gameplayRules.GetPeakTowerHeight();
        const int score = m_gameplayRules.ComputeScore(&m_phaseManager);
        const auto action = input ? m_playerController.HandlePostGameInput(*input, peakHeight, score)
            : PlayerController::PostGameAction::None;
        if (action == PlayerController::PostGameAction::ConfirmRestart)
        {
            SetStageState(StageState::Playing);
            return;
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
        const int score = m_gameplayRules.ComputeScore(&m_phaseManager);
        uiData.hud.remainingSeconds = remainingSeconds;
        uiData.hud.currentHeight = currentHeight;
        uiData.hud.peakHeight = peakHeight;
        uiData.hud.score = score;
        uiData.hud.sweetsCount = GetSweetsCount();
        uiData.hud.timeUp = isTimeUp;
        uiData.hud.moveSpeedMultiplier = m_playerSpeedMultiplier;
        uiData.hud.showFireMissileHint = m_fireMissileHintEnabled;
        if (player)
        {
            uiData.hud.moveSpeedUnitsPerSecond = player->GetMoveSpeed();
        }
        if (g_DebugDrawEnabled && m_sweetsManager)
        {
            SweetsSpace::SweetsContacts::GlueDebugSnapshot glueSnapshot;
            if (m_sweetsManager->GetGlueDebugSnapshot(glueSnapshot))
            {
                uiData.hud.showGlueDebug = true;
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
        uiData.result.sweetsCount = GetSweetsCount();
        uiData.result.timeUp = isTimeUp;
        uiData.result.moveSpeedMultiplier = m_playerSpeedMultiplier;
        uiData.result.showFireMissileHint = m_fireMissileHintEnabled;
        if (player)
        {
            uiData.result.moveSpeedUnitsPerSecond = player->GetMoveSpeed();
        }

        uiData.showHud = g_DebugDrawEnabled && !isTimeUp;
        uiData.showResult = isTimeUp;
        m_uiOverlay->Update(uiData);
    }

    UpdateMissiles(ctx.dt);

    if (input && input->IsKeyPress(VK_RETURN))
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<TitleScene>(*m_manager));
        }

        return;
    }

}

void SceneGame::FireMissile()
{
    if (!player || !m_physicsBinder || !m_drawSystem)
    {
        return;
    }

    if (!m_missileModel)
    {
        const auto* descriptor = SweetsSpace::SweetsPrototypeManifest::Instance().Find("Missile");
        const std::string modelPath = descriptor ? descriptor->modelPath : std::string("Assets/Model/Missile.fbx");
        m_missileModel = Render::ModelCache::Instance().Get(modelPath);
        if (!m_missileModel)
        {
            DebugTrace(std::string(kLogPrefix) + "Missile model load failed; firing skipped.\n");
            return;
        }
    }

    const Transform playerTransform = player->GetRenderTransform();
    constexpr float kSpawnOffset = 1.5f;
    const float yaw = ResolvePlayerYaw(playerTransform) + m_playerSpawnTransform.missileYawOffsetRadians;
    const float3 forward = ForwardFromYaw(yaw);
    constexpr float kMissileSpeed = 40.0f;
    constexpr float kMissileLifetimeSeconds = 6.0f;

    const float missileYaw = std::atan2(forward.y, forward.x) + m_missileSettings.rotationOffset;
    Transform missileTransform{};
    missileTransform.position = playerTransform.position;
    missileTransform.rotation = float3{ 0.0f, 0.0f, missileYaw };
    missileTransform.scale = m_missileSettings.scale;
    const float3 spawnLocalOffset = m_playerSpawnTransform.missileSpawnOffset;
    const float cosYaw = std::cos(yaw);
    const float sinYaw = std::sin(yaw);
    const float3 spawnWorldOffset{
        spawnLocalOffset.x * cosYaw - spawnLocalOffset.y * sinYaw,
        spawnLocalOffset.x * sinYaw + spawnLocalOffset.y * cosYaw,
        spawnLocalOffset.z
    };
    missileTransform.position.x += spawnWorldOffset.x;
    missileTransform.position.y += spawnWorldOffset.y;
    missileTransform.position.z += spawnWorldOffset.z;

    Physics::BoxColliderComponent collider{};
    collider.AddBox(m_missileSettings.halfWidth, m_missileSettings.halfHeight);

    Physics::Box2DShapeConfig shapeConfig{};
    shapeConfig.type = Physics::Box2DShapeConfig::Type::Box;
    shapeConfig.bodyType = b2_dynamicBody;
    shapeConfig.fixedRotation = m_missileSettings.fixedRotation;
    shapeConfig.allowRotation = !m_missileSettings.fixedRotation;
    shapeConfig.linearDamping = m_missileSettings.linearDamping;
    shapeConfig.angularDamping = m_missileSettings.angularDamping;
    shapeConfig.density = m_missileSettings.density;
    shapeConfig.friction = m_missileSettings.friction;
    shapeConfig.restitution = m_missileSettings.restitution;

    auto missile = std::make_shared<Entities::PhysicalEntity>(
        missileTransform,
        shapeConfig,
        m_missileSettings.collisionLayer.value_or(Collision::CollisionLayer::Default),
        m_missileModel,
        nullptr,
        Core::UpdatePhase::Gameplay,
        0,
        "projectile_missile",
        false);
    missile->SetBoxColliderComponent(collider);

    m_physicsBinder->RegisterPhysicsBody(*missile);
    missile->SetGravityScale(m_missileSettings.gravityScale);
    missile->SetLinearVelocity(float3{ forward.x * kMissileSpeed, forward.y * kMissileSpeed, 0.0f });

    m_drawSystem->RegisterRenderable(*missile);

#if defined(_DEBUG)
    if (g_DebugDrawEnabled)
    {
        constexpr float kForwardDebugLength = 3.0f;
        const DirectX::XMFLOAT4 debugColor(1.0f, 0.2f, 0.2f, 0.9f);
        const DirectX::XMFLOAT3 lineStart(playerTransform.position.x, playerTransform.position.y, playerTransform.position.z);
        const DirectX::XMFLOAT3 lineEnd(
            lineStart.x + spawnWorldOffset.x + forward.x * kForwardDebugLength,
            lineStart.y + spawnWorldOffset.y + forward.y * kForwardDebugLength,
            lineStart.z + spawnWorldOffset.z + forward.z * kForwardDebugLength);
        Geometory::AddLine(lineStart, lineEnd, debugColor);
    }
#endif

    m_activeMissiles.push_back(MissileInstance{ std::move(missile), kMissileLifetimeSeconds });
    m_fireMissileHintEnabled = true;
}

void SceneGame::UpdateMissiles(float deltaSeconds)
{
    if (m_activeMissiles.empty())
    {
        return;
    }

    if (deltaSeconds < 0.0f)
    {
        deltaSeconds = 0.0f;
    }

    float screenMinX = 0.0f;
    float screenMaxX = 0.0f;
    float screenMinY = 0.0f;
    float screenMaxY = 0.0f;
    const bool hasScreenBounds = m_playerController.TryGetCachedPlayerBounds(
        screenMinX,
        screenMaxX,
        screenMinY,
        screenMaxY);

    auto shouldRemove = [&](MissileInstance& instance)
    {
        instance.remainingSeconds -= deltaSeconds;
        bool outOfBounds = false;
        if (hasScreenBounds && instance.entity)
        {
            const AABB bounds = instance.entity->GetAABB();
            outOfBounds =
                bounds.max.x < screenMinX ||
                bounds.min.x > screenMaxX ||
                bounds.max.y < screenMinY ||
                bounds.min.y > screenMaxY;
        }

        if (instance.remainingSeconds > 0.0f && instance.entity && !outOfBounds)
        {
            return false;
        }

        if (instance.entity)
        {
            if (m_drawSystem)
            {
                m_drawSystem->UnregisterRenderable(*instance.entity);
            }
            if (m_physicsBinder)
            {
                m_physicsBinder->UnregisterPhysicsBody(instance.entity.get());
            }
        }

        return true;
    };

    m_activeMissiles.erase(
        std::remove_if(m_activeMissiles.begin(), m_activeMissiles.end(), shouldRemove),
        m_activeMissiles.end());
}

void SceneGame::ClearMissiles()
{
    if (m_activeMissiles.empty())
    {
        return;
    }

    for (auto& instance : m_activeMissiles)
    {
        if (!instance.entity)
        {
            continue;
        }

        if (m_drawSystem)
        {
            m_drawSystem->UnregisterRenderable(*instance.entity);
        }
        if (m_physicsBinder)
        {
            m_physicsBinder->UnregisterPhysicsBody(instance.entity.get());
        }
    }

    m_activeMissiles.clear();
}


#if defined(_DEBUG)
void SceneGame::HandleColliderDebugCommands(const Core::UpdateContext& ctx)
{
        (void)ctx;
        if (!player)
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

        for (const auto& entity : m_stagePhysicsEntities)
        {
            drawEntity(entity.get());
        }
    }

    if (activeCamera)
    {
        auto perspectiveCamera = std::dynamic_pointer_cast<CameraSpace::PerspectiveCamera>(activeCamera);
        if (perspectiveCamera && player)
        {
            const float comfortZoneTopPercentY = std::clamp(m_gameCameraConfig.comfortZoneTopPercentY, 0.0f, 1.0f);
            const float comfortZoneBottomPercentY = std::clamp(m_gameCameraConfig.comfortZoneBottomPercentY, 0.0f, 1.0f);
            float3 targetPosition = player->GetTransform().position;

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
            const DirectX::XMFLOAT4 linkColor(0.2f, 1.0f, 0.3f, 0.85f);
            const Color creamColor = MakeColor(0.2f, 0.6f, 1.0f, 0.9f);
            const Color fruitColor = MakeColor(0.8f, 0.3f, 1.0f, 0.9f);

            for (const auto& link : glueSnapshot.fruitLinks)
            {
                float z = stagePlaneZ;
                float3 creamPos{ 0.0f, 0.0f, 0.0f };
                float3 fruitPos{ 0.0f, 0.0f, 0.0f };
                const bool hasCream = (link.cream != nullptr);
                const bool hasFruit = (link.fruit != nullptr);
                if (hasCream)
                {
                    creamPos = link.cream->GetTransform().position;
                    z = creamPos.z;
                    DebugColliderRenderer::DrawCircle(float2{ creamPos.x, creamPos.y }, 0.35f, creamPos.z, creamColor);
                }
                if (hasFruit)
                {
                    fruitPos = link.fruit->GetTransform().position;
                    z = fruitPos.z;
                    DebugColliderRenderer::DrawCircle(float2{ fruitPos.x, fruitPos.y }, 0.28f, fruitPos.z,
                        link.fruitFrozen ? fruitColor : MakeColor(1.0f, 0.5f, 0.5f, 0.9f));
                }
                if (hasCream && hasFruit)
                {
                    Geometory::AddLine(
                        DirectX::XMFLOAT3(creamPos.x, creamPos.y, creamPos.z),
                        DirectX::XMFLOAT3(fruitPos.x, fruitPos.y, fruitPos.z),
                        linkColor);
                }
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
    std::optional<float> visibleHeightWorld;

    auto baseCamera = std::dynamic_pointer_cast<CameraSpace::BaseCamera>(activeCamera);
    assert(baseCamera && "SceneGame background requires a BaseCamera.");
    if (!baseCamera)
    {
        DebugTrace(std::string(kLogPrefix) + "Background skipped: active camera is not a BaseCamera.\n");
    }
    else
    {
        UpdateBackgroundScrollOriginForWrap(baseCamera->GetPosition().y);
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
                    if (player)
                    {
                        planeSample = player->GetTransform().position;
                    }
                    if (m_stagePlaneZ && IsFiniteFloat(*m_stagePlaneZ))
                    {
                        planeSample.z = *m_stagePlaneZ;
                    }
                    const float viewZ = ComputeViewDepth(baseCamera->GetPosition(), baseCamera->GetRotation(), planeSample);
                    if (IsFiniteFloat(viewZ) && viewZ > 0.0f)
                    {
                        const float fovRadians = DirectX::XMConvertToRadians(perspectiveCamera->GetFieldOfViewDegrees());
                        const float computedVisibleHeightWorld = 2.0f * std::tan(0.5f * fovRadians) * viewZ;
                        if (IsFiniteFloat(computedVisibleHeightWorld) && computedVisibleHeightWorld > 0.0f)
                        {
                            visibleHeightWorld = computedVisibleHeightWorld;
                            pixelsPerUnit = viewportHeight / computedVisibleHeightWorld;
                        }
                    }
                }
            }
        }
        if (m_backgroundRenderer)
        {
            std::optional<float> maxParallaxOffset;
            if (visibleHeightWorld && IsFiniteFloat(pixelsPerUnit) && pixelsPerUnit > 0.0f)
            {
                const std::optional<float> textureHeightPixels = m_backgroundRenderer->GetMinLayerTextureHeightPixels();
                if (textureHeightPixels && IsFiniteFloat(*textureHeightPixels))
                {
                    const float tileHeightWorld = *textureHeightPixels / pixelsPerUnit;
                    const float computedMaxParallaxOffset = 0.5f * (tileHeightWorld - *visibleHeightWorld);
                    if (IsFiniteFloat(computedMaxParallaxOffset) && computedMaxParallaxOffset > 0.0f)
                    {
                        maxParallaxOffset = computedMaxParallaxOffset;
                    }
                }
            }

            if (!maxParallaxOffset)
            {
                const std::optional<float> safeScrollY = m_stageData.GetBackgroundSafeScrollY();
                if (safeScrollY && IsFiniteFloat(*safeScrollY) && *safeScrollY > 0.0f)
                {
                    maxParallaxOffset = *safeScrollY;
                }
            }

            if (maxParallaxOffset)
            {
                const float clampedMaxOffset = std::max(0.0f, *maxParallaxOffset);
                const std::optional<float> safeWindow = m_stageData.GetBackgroundSafeWindowY();
                const std::optional<float> softness = m_stageData.GetBackgroundSoftnessY();
                const float absOffset = std::fabs(scrollOffsetUsedForBackground);
                const float sign = scrollOffsetUsedForBackground < 0.0f ? -1.0f : 1.0f;
                float softenedOffset = scrollOffsetUsedForBackground;

                if (clampedMaxOffset > 0.0f && absOffset > 0.0f)
                {
                    if (softness && IsFiniteFloat(*softness) && *softness > 0.0f)
                    {
                        const float clampedSoftness = std::clamp(*softness, 0.0f, clampedMaxOffset);
                        const float softStart = std::max(0.0f, clampedMaxOffset - clampedSoftness);
                        if (absOffset > softStart)
                        {
                            const float t = SmoothStep(softStart, clampedMaxOffset, absOffset);
                            const float eased = Lerp(softStart, clampedMaxOffset, t);
                            softenedOffset = sign * eased;
                        }
                    }

                    softenedOffset = std::clamp(softenedOffset, -clampedMaxOffset, clampedMaxOffset);

                    if (safeWindow && IsFiniteFloat(*safeWindow) && *safeWindow > 0.0f)
                    {
                        const float clampedWindow = std::clamp(*safeWindow, 0.0f, clampedMaxOffset);
                        if (clampedWindow > 0.0f)
                        {
                            const float windowStart = std::max(0.0f, clampedMaxOffset - clampedWindow);
                            const float easedAbs = std::fabs(softenedOffset);
                            if (easedAbs > windowStart)
                            {
                                const float t = SmoothStep(windowStart, clampedMaxOffset, easedAbs);
                                const float pulled = Lerp(easedAbs, 0.0f, t);
                                softenedOffset = (softenedOffset < 0.0f ? -1.0f : 1.0f) * pulled;
                            }
                        }
                    }
                }

                scrollOffsetUsedForBackground = softenedOffset;
            }

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

                    m_renderer->Draw(*obj.model, obj.transform, activeCamera.get());
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

            if (player)
            {
                const auto [receiverY, receiverHeight] = resolveShadowReceiver(*player);
                m_blobShadowRenderer->DrawShadow(*player, *activeCamera, receiverY, receiverZ, receiverHeight);
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
    const bool wrapY = resolvedSettings.wrapY;
    const bool shouldClampY = !wrapY && (visualMinY || visualMaxY);

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
    if (player)
    {
        targetPosition = player->GetTransform().position;
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
    if ((shouldClampY || wrapY) && halfViewHeightWorld && IsFiniteFloat(*halfViewHeightWorld))
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

    const bool canWrapY = wrapY && minScrollY && maxScrollY;
    if (wrapY && !canWrapY)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Camera wrapY enabled but bounds missing; wrap disabled for this session.\n";
        ::DebugTrace(oss.str());
    }

    m_cameraWrapMinY = canWrapY ? minScrollY : std::nullopt;
    m_cameraWrapMaxY = canWrapY ? maxScrollY : std::nullopt;

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
        m_cameraRig->SetWrapY(canWrapY ? minScrollY : std::nullopt, canWrapY ? maxScrollY : std::nullopt);
        if (!wrapY)
        {
            m_cameraRig->SetClampY(shouldClampY ? minScrollY : std::nullopt,
                                   shouldClampY ? maxScrollY : std::nullopt);
        }
        else
        {
            m_cameraRig->SetClampY(std::nullopt, std::nullopt);
        }

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
    m_backgroundCameraLastY = m_gameCamera->GetPosition().y;
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

    const bool wrapY = m_cameraRigSettings.wrapY;
    const bool shouldClampY = !wrapY && (visualMinY || visualMaxY);
    std::optional<float> halfViewHeightWorld = std::nullopt;
    if (m_gameCamera && player)
    {
        float3 planeSample = player->GetTransform().position;
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
    if ((shouldClampY || wrapY) && halfViewHeightWorld && IsFiniteFloat(*halfViewHeightWorld))
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

    const bool canWrapY = wrapY && minScrollY && maxScrollY;
    if (wrapY && !canWrapY)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Camera wrapY enabled but bounds missing after phase update; wrap disabled.\n";
        ::DebugTrace(oss.str());
    }

    m_cameraWrapMinY = canWrapY ? minScrollY : std::nullopt;
    m_cameraWrapMaxY = canWrapY ? maxScrollY : std::nullopt;

    m_cameraRig->SetWrapY(canWrapY ? minScrollY : std::nullopt,
                          canWrapY ? maxScrollY : std::nullopt);
    if (!wrapY)
    {
        m_cameraRig->SetClampY(shouldClampY ? minScrollY : std::nullopt,
                               shouldClampY ? maxScrollY : std::nullopt);
    }
    else
    {
        m_cameraRig->SetClampY(std::nullopt, std::nullopt);
    }

    if (wrapY && m_gameCamera)
    {
        UpdateBackgroundScrollOriginForWrap(m_gameCamera->GetPosition().y);
    }
}

void SceneGame::UpdateBackgroundScrollOriginForWrap(float cameraY)
{
    if (!m_cameraRigSettings.wrapY || !m_cameraWrapMinY || !m_cameraWrapMaxY)
    {
        m_backgroundCameraLastY = cameraY;
        return;
    }

    if (!IsFiniteFloat(*m_cameraWrapMinY) || !IsFiniteFloat(*m_cameraWrapMaxY))
    {
        m_backgroundCameraLastY = cameraY;
        return;
    }

    const float minY = std::min(*m_cameraWrapMinY, *m_cameraWrapMaxY);
    const float maxY = std::max(*m_cameraWrapMinY, *m_cameraWrapMaxY);
    const float wrapRange = maxY - minY;
    if (!IsFiniteFloat(wrapRange) || wrapRange <= 0.0f)
    {
        m_backgroundCameraLastY = cameraY;
        return;
    }

    if (m_backgroundCameraLastY && IsFiniteFloat(*m_backgroundCameraLastY))
    {
        const float delta = cameraY - *m_backgroundCameraLastY;
        if (std::fabs(delta) > wrapRange * 0.5f)
        {
            m_backgroundScrollOriginY += (delta > 0.0f ? wrapRange : -wrapRange);
        }
    }

    m_backgroundCameraLastY = cameraY;
}

void SceneGame::ApplyPlayerWrapDeltaY(float wrapDeltaY)
{
    if (!m_gameCamera || !IsFiniteFloat(wrapDeltaY)
        || std::fabs(wrapDeltaY) <= std::numeric_limits<float>::epsilon())
    {
        return;
    }

    float3 cameraPosition = m_gameCamera->GetPosition();
    if (!IsFiniteFloat(cameraPosition.y))
    {
        return;
    }

    cameraPosition.y += wrapDeltaY;
    m_gameCamera->SetPosition(cameraPosition);

    if (m_cameraRig && player)
    {
        const float targetY = player->GetTransform().position.y;
        if (IsFiniteFloat(targetY))
        {
            m_cameraRig->ApplyExternalWrapDeltaY(wrapDeltaY, targetY);
        }
    }

    m_backgroundScrollOriginY += wrapDeltaY;
    m_backgroundCameraLastY = cameraPosition.y;
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
        m_sweetsManager->ForceSpawnRefresh();
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "ResetPlaySession could not clear sweets: manager missing.\n");
    }

    ClearMissiles();

    if (player)
    {
        if (m_playerBaseHorizontalSpeed > 0.0f || m_playerBaseVerticalSpeed > 0.0f)
        {
            m_playerSpeedMultiplier = 1.0f;
            player->SetBaseMoveSpeed(m_playerBaseHorizontalSpeed, m_playerBaseVerticalSpeed);
            player->SetSpeedMultiplier(m_playerSpeedMultiplier);
        }
    }

    if (player && m_cameraRig)
    {
        m_cameraRig->ResetVerticalScroll(player->GetTransform().position);
    }
    else if (!m_cameraRig)
    {
        DebugTrace(std::string(kLogPrefix) + "ResetPlaySession skipped camera rig realignment: camera rig missing.\n");
    }

    m_gameplayRules.ResetSession();
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
    (void)stageData;
}
