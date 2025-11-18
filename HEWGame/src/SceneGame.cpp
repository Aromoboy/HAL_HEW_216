#include "SceneGame.h"
#include "Geometory.h"
#include "DrawMatrix.h"
#include "Vector.h"
#include "ShaderList.h"
#include "Defines.h"
#include "DebugConsole.h"
#include "AssimpModelLorder.h"
#include "Core/Input.h"
#include "Systems/PhysicsMachine.h"
#include "Systems/IntegrateSystem.h"
#include "Systems/ContactLiteSystem.h"
#include "GravityDampingSystem.h"
#include "Systems/DrawSystem.h"
#include "AABBCollider.h"
#include "ModelCache.h"
#include "GravityDampingSystem.h"
#include "TextureCache.h"
#include "DirectX.h"
#include "Sweets.h"
#include "SweetsManager.h"



#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <DirectXMath.h>
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

        constexpr const char* kLogPrefix = "[SceneGame] ";
        constexpr const char* kPhysicsConfigPath = "Assets/Config/physics_tuning.cfg";
        constexpr bool kEnableDrawSystemVerboseLogging = false;
        constexpr bool kEnablePhysicsIntegrationLogging = true;
        constexpr const char* kStageConfigPath = "Assets/Config/stage_default.cfg";
        constexpr const char* kSweetsSpawnConfigPath = "Assets/Config/sweets_spawn.cfg";
        constexpr float kDefaultPlaySeconds = 30.0f;
        constexpr float kCountdownLogIntervalSeconds = 5.0f;
        constexpr float kBaseScorePerMeter = 100.0f;
        constexpr BYTE kConfirmInputKey = VK_RETURN;
        constexpr BYTE kRetryInputKey = 'R';
        const HudLayoutConfig kHudLayoutConfig
        {
            512u,
            192u,
            32.0f,
            12.0f,
            28.0f,
            5.0f,
            1.5f,
            2.0f,
            DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            DirectX::XMFLOAT4(1.0f, 0.55f, 0.25f, 1.0f)
        };

        struct PhysicsTuningSettings
        {
            float gravity = 9.8f;
            float damping = 0.98f;
            float penetrationTolerance = 0.01f;
            float restitution = 0.15f;
        };

        std::string TrimWhitespace(const std::string& input)
        {
            const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
            auto begin = input.begin();
            auto end = input.end();
            while (begin != end && isSpace(static_cast<unsigned char>(*begin)))
            {
                ++begin;
            }
            if (begin == end)
            {
                return std::string{};
            }
            do
            {
                --end;
            } while (end != begin && isSpace(static_cast<unsigned char>(*end)));
            return std::string(begin, end + 1);
        }

        PhysicsTuningSettings LoadPhysicsTuningSettings()
        {
            PhysicsTuningSettings settings;
            std::ifstream config(kPhysicsConfigPath);
            if (!config.is_open())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Physics tuning config not found at '" << kPhysicsConfigPath
                    << "'. Using defaults (gravity=" << settings.gravity
                    << ", damping=" << settings.damping
                    << ", penetrationTolerance=" << settings.penetrationTolerance
                    << ", restitution=" << settings.restitution << ").\n";
                DebugTrace(oss.str());
                return settings;
            }

            std::string line;
            size_t lineNumber = 0;
            while (std::getline(config, line))
            {
                ++lineNumber;
                const auto commentPos = line.find_first_of("#;");
                if (commentPos != std::string::npos)
                {
                    line = line.substr(0, commentPos);
                }
                line = TrimWhitespace(line);
                if (line.empty())
                {
                    continue;
                }

                const auto delimiterPos = line.find('=');
                if (delimiterPos == std::string::npos)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Invalid physics tuning line " << lineNumber
                        << " (missing '='): " << line << "\n";
                    DebugTrace(oss.str());
                    continue;
                }

                std::string key = TrimWhitespace(line.substr(0, delimiterPos));
                std::string value = TrimWhitespace(line.substr(delimiterPos + 1));
                if (key.empty() || value.empty())
                {
                    continue;
                }

                try
                {
                    if (key == "gravity")
                    {
                        settings.gravity = std::stof(value);
                    }
                    else if (key == "damping")
                    {
                        settings.damping = std::stof(value);
                    }
                    else if (key == "penetrationTolerance")
                    {
                        settings.penetrationTolerance = std::max(0.0f, std::stof(value));
                    }
                    else if (key == "restitution")
                    {
                        settings.restitution = std::clamp(std::stof(value), 0.0f, 1.0f);
                    }
                    else
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Unknown physics tuning key '" << key << "' on line "
                            << lineNumber << "\n";
                        DebugTrace(oss.str());
                    }
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse value for key '" << key << "' on line "
                        << lineNumber << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                }
            }

            std::ostringstream oss;
            oss << kLogPrefix << "Loaded physics tuning from '" << kPhysicsConfigPath
                << "' (gravity=" << settings.gravity
                << ", damping=" << settings.damping
                << ", penetrationTolerance=" << settings.penetrationTolerance
                << ", restitution=" << settings.restitution << ").\n";
            DebugTrace(oss.str());
            return settings;
        }

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
            // コンセプトアートのコールアウトでは、カメラをプレイヤーの左12.5m、上方7.25m、
            // プレイプレーン後方1.5mに配置し、横方向のヨー角90度、ピッチ角-15度とする（一時的な値）
            float3{ -12.5f, 7.25f, -1.5f },
            45.0f,
            DirectX::XMConvertToRadians(90.0f),
            DirectX::XMConvertToRadians(-15.0f)
        };
}

Model::AnimeNo no;
float3 fRads = { 0.0f, 0.0f, 0.0f };

SceneGame::SceneGame()
{
    const bool stageLoaded = m_stageData.LoadFromFile(kStageConfigPath);
    m_phaseManager.Initialize(m_stageData.GetPhaseThresholds(), m_stageData.GetPhaseRewardMultipliers());
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Stage data " << (stageLoaded ? "loaded" : "defaulted")
            << " planeZ=" << m_stageData.GetPlaneZ()
            << " thresholds=" << m_stageData.GetPhaseThresholds().size()
            << " multipliers=" << m_stageData.GetPhaseRewardMultipliers().size()
            << " spawnPoints=" << m_stageData.GetSpawnPoints().size()
            << " playerSpeed=" << m_stageData.GetPlayerMoveSpeed() << "\n";
        DebugTrace(oss.str());
    }

    const PhysicsTuningSettings tuning = LoadPhysicsTuningSettings();
    m_physicsMachine = std::make_unique<Systems::PhysicsMachine>();
    m_integrateSystem = std::make_shared<Systems::IntegrateSystem>();
    m_gravitySystem = std::make_shared<Systems::GravityDampingSystem>(tuning.gravity, tuning.damping);
    m_contactSystem = std::make_unique<Systems::ContactLiteSystem>();
    m_drawSystem = std::make_unique<Systems::DrawSystem>();
    m_sweetsManager = std::make_unique<SweetsSpace::SweetsManager>();
    if (m_integrateSystem)
    {
        m_integrateSystem->SetVerboseLoggingEnabled(kEnablePhysicsIntegrationLogging);
    }
    if (m_physicsMachine)
    {
        m_physicsMachine->SetSystem(m_gravitySystem);
        if (m_integrateSystem)
        {
            m_physicsMachine->SetIntegrator(m_integrateSystem);
        }
        if (m_contactSystem)
        {
            const auto& bodies = m_physicsMachine->GetBodies();
            m_contactSystem->SetBodySource(&bodies);
        }
    }

    if (m_contactSystem)
    {
        m_contactSystem->SetPenetrationTolerance(tuning.penetrationTolerance);
        m_contactSystem->SetRestitution(tuning.restitution);
    }

    if (m_drawSystem)
    {
        m_drawSystem->SetDebugLoggingEnabled(kEnableDrawSystemVerboseLogging);
    }

    if (m_sweetsManager)
    {
        m_sweetsManager->SetDrawSystem(m_drawSystem.get());
        m_sweetsManager->SetPhysicsMachine(m_physicsMachine.get());
        m_sweetsManager->SetUpdateScheduler(&GetUpdateScheduler());
        m_sweetsManager->SetSpawnOrigins(m_stageData.GetSpawnPoints());
        const bool spawnConfigLoaded = m_sweetsManager->LoadPhaseSpawnConfig(kSweetsSpawnConfigPath);
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Sweets spawn config " << (spawnConfigLoaded ? "loaded" : "defaulted")
                << " from '" << kSweetsSpawnConfigPath << "'.\n";
            DebugTrace(oss.str());
        }
        m_sweetsManager->SetPhaseChangeCallback([this](int newPhase)
        {
            ForwardSweetsPhaseChange(newPhase);
        });
    }

    m_hudRenderer = std::make_unique<HudRenderer>();
    if (m_hudRenderer && !m_hudRenderer->Initialize(kHudLayoutConfig))
    {
        DebugTrace(std::string(kLogPrefix) + "HUD renderer failed to initialize; overlay disabled.\n");
        m_hudRenderer.reset();
    }
    else if (m_hudRenderer)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "HUD renderer initialized (texture="
            << kHudLayoutConfig.textureWidth << 'x' << kHudLayoutConfig.textureHeight
            << ", margin=" << kHudLayoutConfig.marginPixels
            << ", warningThreshold=" << kHudLayoutConfig.warningThresholdSeconds << "s).\n";
        DebugTrace(oss.str());
    }

    RenderTarget* pRTV = GetDefaultRTV();
    DepthStencil* pDSV = GetDefaultDSV();
    SetRenderTargets(1, &pRTV, pDSV);
    SetDepthTest(true);
    m_pModel = std::make_unique<Model>();
    if (!m_pModel->Load("Assets/Model/LowPolyNature/Models/Flowers_01.fbx", 1.0f, Model::Flip::None))
    {
        no = m_pModel->AddAnimation("Assets/Model/LowPolyNature/Models/BoxsLam.fbx");
    }

    no = m_pModel->AddAnimation("Assets/Model/LowPolyNature/BoxsLam.fbx");

    m_pModel->Play(no, 1, 1.0f);

    InitializeStageAnchors();

    m_cameraAnchor = std::make_shared<SceneSpace::Entity>("PlayerAnchor");
    if (!m_pObjects.empty())
    {
        auto& anchorTransform = m_cameraAnchor->GetTransform();
        anchorTransform.position = m_pObjects.front()->GetPosition();
        anchorTransform.rotation = m_pObjects.front()->GetfRads();
        anchorTransform.scale = m_pObjects.front()->GetScale();
    }

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

    m_pManager = std::make_unique<SoundManagerEX>();

    m_loader = std::make_shared<AssimpModelLorder>();

    Render::ModelCache& cache = Render::ModelCache::Instance();
    ID3D11Device* device = GetDevice();
    if (!device)
    {
        DebugTrace(std::string(kLogPrefix) + "Failed to configure ModelCache: device was null.\n");
    }
    else
    {
        cache.SetLoader(m_loader, device, TextureCache::Instance());
    }

    // 描画テスト用
    m_model = cache.Get("Assets/Model/LowPolyNature/Models/OiiaioooooiaiFin.fbx");

    m_render = std::make_unique<ModelRender>();
    if (m_drawSystem && m_render)
    {
        m_drawSystem->SetRenderer(m_render.get());
    }
    Transform platformTransform{};
    platformTransform.position = float3{ 0.0f, m_stageData.GetPlatformCenterY(), m_stageData.GetPlaneZ() };
    platformTransform.rotation = float3{ 0.0f, 0.0f, 0.0f };
    platformTransform.scale = m_stageData.GetPlatformScale();
    auto platformCollider = std::make_shared<Collision::AABBCollider>(platformTransform, false, 1.0f, float3{ 0.0f, 0.0f, 0.0f });
    m_stagePlatform = std::make_unique<Stage::FoundationPlatform>(
        "StageFoundation",
        platformTransform,
        platformCollider,
        Core::UpdatePhase::PrePhysics,
        -100);
    if (m_stagePlatform)
    {
        RegisterPhysicsBody(*m_stagePlatform);
        std::ostringstream oss;
        const auto& scale = m_stagePlatform->GetTransform().scale;
        const auto& pos = m_stagePlatform->GetTransform().position;
        oss << kLogPrefix << "Stage foundation initialized at ("
            << pos.x << ',' << pos.y << ',' << pos.z << ") scale=("
            << scale.x << ',' << scale.y << ',' << scale.z << ") color=("
            << m_stagePlatform->GetDebugColor().x << ','
            << m_stagePlatform->GetDebugColor().y << ','
            << m_stagePlatform->GetDebugColor().z << ").\n";
        DebugTrace(oss.str());
        if (m_drawSystem)
        {
            m_drawSystem->RegisterRenderable(*m_stagePlatform);
            m_stagePlatformDrawRegistered = true;
            DebugTrace(std::string(kLogPrefix) + "Stage foundation registered with DrawSystem.\n");
        }
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "Failed to allocate stage foundation platform.\n");
    }

    std::shared_ptr<Collision::ICollider> collider;
    Transform trans =
    { float3{0.0f, 0.0f, m_stageData.GetPlaneZ()}, float3{0.0f, 0.0f, 0.0f}, float3{1.0f, 1.0f, 1.0f} };
    collider = std::make_shared<Collision::AABBCollider>(trans, false, 1.0f, float3{ 0.0f, 0.0f, 0.0f });
    player = std::make_unique<SceneSpace::Player>(
        "player",
        float3{ 0.0f, 0.0f, m_stageData.GetPlaneZ() },
        float3{ 0.0f, 0.0f, 0.0f },
        float3{ 1.0f, 1.0f, 1.0f },
        collider,
        float3{ 0.0f, 0.0f, 0.0f },
        float3{ 0.0f, 0.0f, 0.0f },
        1.0f,
        0.98f,
        Core::UpdatePhase::Gameplay,
        0,
        true);
    if (player)
    {
        const float platformTopY = m_stageData.GetPlatformCenterY() + (m_stageData.GetPlatformScale().y * 0.5f);
        player->SetMinimumY(platformTopY);
        player->SetMoveSpeed(m_stageData.GetPlayerMoveSpeed());
        if (m_sweetsManager)
        {
            player->SetSweetsManager(m_sweetsManager.get());
        }
        RegisterPhysicsBody(*player);
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Player physics body registered (kinematic="
                << std::boolalpha << player->IsKinematic()
                << ", weight=" << player->GetPhysical().weight << ").\n";
            DebugTrace(oss.str());
        }
        if (m_drawSystem)
        {
            m_drawSystem->RegisterRenderable(*player);
        }
    }

    m_countdownTimer.Reset();
    m_totalPlaySeconds = kDefaultPlaySeconds;
    m_remainingCountdownSeconds = m_totalPlaySeconds;
    m_lastAnnouncedCountdownSeconds = static_cast<int>(std::ceil(m_totalPlaySeconds));
}

SceneGame::~SceneGame()
{
    if (m_hudRenderer)
    {
        m_hudRenderer->Shutdown();
        m_hudRenderer.reset();
    }
    if (m_stagePlatformRegistered && m_stagePlatform)
    {
        Unregister(*m_stagePlatform);
        m_stagePlatformRegistered = false;
    }
    if (m_stagePlatformDrawRegistered && m_stagePlatform && m_drawSystem)
    {
        m_drawSystem->UnregisterRenderable(*m_stagePlatform);
        m_stagePlatformDrawRegistered = false;
    }

        UnregisterAllPhysicsBodies();
        if (m_inputRegistered)
        {
                if (auto* input = GetInput())
                {
                        Unregister(*input);
                }
                m_inputRegistered = false;
        }

        if (m_physicsRegistered && m_physicsMachine)
        {
                GetUpdateScheduler().Unregister(*m_physicsMachine);
                m_physicsRegistered = false;
        }

        if (m_contactRegistered && m_contactSystem)
        {
                GetUpdateScheduler().Unregister(*m_contactSystem);
                m_contactRegistered = false;
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
                m_stagePlatformDrawRegistered = false;
        }

        if (m_sweetsRegistered && m_sweetsManager)
        {
                GetUpdateScheduler().Unregister(*m_sweetsManager);
                m_sweetsRegistered = false;
        }
}

void SceneGame::ConfigureUpdates()
{
        if (auto* input = GetInput())
        {
                Register(*input);
                m_inputRegistered = true;
        }

        if (m_physicsMachine && !m_physicsRegistered)
        {
                GetUpdateScheduler().Register(*m_physicsMachine);
                m_physicsRegistered = true;
        }

        if (m_contactSystem && !m_contactRegistered)
        {
                GetUpdateScheduler().Register(*m_contactSystem);
                m_contactRegistered = true;
        }

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

        if (m_stagePlatform && !m_stagePlatformRegistered)
        {
                Register(*m_stagePlatform);
                m_stagePlatformRegistered = true;
        }
        if (m_stagePlatform && m_drawSystem && !m_stagePlatformDrawRegistered)
        {
                m_drawSystem->RegisterRenderable(*m_stagePlatform);
                m_stagePlatformDrawRegistered = true;
        }

        if (player)
        {
                Register(*player);

                const bool tracked = m_registeredPhysicsBodies.find(player.get()) != m_registeredPhysicsBodies.end();
                if (!tracked)
                {
                        RegisterPhysicsBody(*player);
                        std::ostringstream oss;
                        oss << kLogPrefix << "ConfigureUpdates ensured player body is registered (kinematic="
                            << std::boolalpha << player->IsKinematic() << ").\n";
                        DebugTrace(oss.str());
                }
        }

        ResetPlaySession();
}

void SceneGame::Update(Core::UpdateContext& ctx)
{
#if defined(_DEBUG)
        HandleColliderDebugCommands(ctx);
#endif
UpdateCountdown(ctx);
UpdateTowerHeightTelemetry();

if (m_timeUp)
{
    ProcessPostGameInput(ctx);
    return;
}

HandleManualRetryInput(ctx);
        fRads.z += 0.001f * RAD;

        if (m_cameraAnchor && !m_pObjects.empty())
        {
                auto& anchorTransform = m_cameraAnchor->GetTransform();
                anchorTransform.position = m_pObjects.front()->GetPosition();
                anchorTransform.rotation = m_pObjects.front()->GetfRads();
                anchorTransform.scale = m_pObjects.front()->GetScale();
        }

        const float stackedHeight = m_maxTowerHeight;
        if (m_phaseManager.UpdateHeight(stackedHeight))
        {
            BroadcastPhaseAdvance(m_phaseManager.GetCurrentPhase());
        }
}

#if defined(_DEBUG)
void SceneGame::HandleColliderDebugCommands(Core::UpdateContext& ctx)
{
        if (!player)
        {
                return;
        }

        Core::Input& input = ctx.input;
        constexpr float kDebugNudge = 0.25f;
        float3 delta{ 0.0f, 0.0f, 0.0f };

        if (input.IsKeyTrigger(VK_F2))
        {
                delta.y += kDebugNudge;
        }
        if (input.IsKeyTrigger(VK_F3))
        {
                delta.y -= kDebugNudge;
        }

        if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f)
        {
                return;
        }

        SceneSpace::TranslatePhysicalEntity(*player, delta);

        const auto& transform = player->GetTransform();

        Collision::ICollider* collider = player->GetCollider();
        if (!collider)
        {
                DebugTrace(std::string(kLogPrefix) + "[ColliderDebug] Player collider missing; cannot log extents.\n");
                return;
        }

        const AABB bounds = collider->GetAABB();
        std::ostringstream oss;
        oss << kLogPrefix << "[ColliderDebug] Nudged player by (" << delta.x << ',' << delta.y << ',' << delta.z << ")"
            << " newPos=(" << transform.position.x << ',' << transform.position.y << ',' << transform.position.z << ")"
            << " aabbMin=(" << bounds.min.x << ',' << bounds.min.y << ',' << bounds.min.z << ")"
            << " aabbMax=(" << bounds.max.x << ',' << bounds.max.y << ',' << bounds.max.z << ")\n";
        DebugTrace(oss.str());
}
#endif

void SceneGame::Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera)
{
        // 描画確認用の数値
        if (!activeCamera)
        {
                DebugTrace(std::string(kLogPrefix) + "Draw skipped: no active camera registered.\n");
                return;
        }

        Geometory::SetView(activeCamera->GetView());
        Geometory::SetProjection(activeCamera->GetProjection());

        if (!m_drawSystem)
        {
                DebugTrace(std::string(kLogPrefix) + "Draw skipped: draw system missing.\n");
                return;
        }

        m_drawSystem->RenderQueued(activeCamera);

        if (m_hudRenderer)
        {
                HudDisplayData hudData{};
                hudData.remainingSeconds = std::max(0.0f, m_remainingCountdownSeconds);
                hudData.currentHeight = m_currentTowerHeight;
                hudData.peakHeight = m_peakTowerHeight;
                hudData.score = ComputeScore();
                hudData.timeUp = m_timeUp;
                m_hudRenderer->Draw(hudData);
        }

#if defined(_DEBUG)
        RenderRunTelemetryOverlay();
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
// BaseCamera::RecalculateViewは保護されたメソッドです。GetView はトランスフォームが変更された際に遅延評価で再計算
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
    DebugTrace(oss.str());
}

void SceneGame::RegisterPhysicsBody(SceneSpace::PhysicalEntity& entity)
{
    if (!m_physicsMachine)
    {
        DebugTrace(std::string(kLogPrefix) + "RegisterPhysicsBody skipped: physics machine missing.\n");
        return;
    }

    const auto insertResult = m_registeredPhysicsBodies.insert(&entity);
    if (!insertResult.second)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "RegisterPhysicsBody ignored duplicate entity=" << &entity << "\n";
        DebugTrace(oss.str());
        return;
    }

    m_physicsMachine->RegisterBody(entity);
}

void SceneGame::UnregisterPhysicsBody(SceneSpace::PhysicalEntity* entity)
{
    if (!entity)
    {
        DebugTrace(std::string(kLogPrefix) + "UnregisterPhysicsBody received null entity pointer.\n");
        return;
    }

    const auto tracked = m_registeredPhysicsBodies.find(entity);
    if (tracked == m_registeredPhysicsBodies.end())
    {
        std::ostringstream oss;
        oss << kLogPrefix << "UnregisterPhysicsBody called for untracked entity=" << entity << "\n";
        DebugTrace(oss.str());
    }
    else
    {
        m_registeredPhysicsBodies.erase(tracked);
    }

    if (m_physicsMachine)
    {
        m_physicsMachine->UnregisterBody(*entity);
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "Physics machine missing during UnregisterPhysicsBody.\n");
    }
}

void SceneGame::UnregisterAllPhysicsBodies()
{
    if (m_registeredPhysicsBodies.empty())
    {
        return;
    }

    std::vector<SceneSpace::PhysicalEntity*> bodies(m_registeredPhysicsBodies.begin(), m_registeredPhysicsBodies.end());
    for (SceneSpace::PhysicalEntity* body : bodies)
    {
        UnregisterPhysicsBody(body);
    }
}

void SceneGame::InitializeStageAnchors()
{
    m_pObjects.clear();
    const auto& spawnPoints = m_stageData.GetSpawnPoints();
    if (spawnPoints.empty())
    {
        const float3 fallback{ 0.0f, 0.0f, m_stageData.GetPlaneZ() };
        m_pObjects.push_back(std::make_shared<TestObject>("OiiaioooooiaiFin", fallback, float3{ 1.0f, 1.0f, 1.0f }, float3{ 0.0f, 0.0f, 0.0f }));
        std::ostringstream oss;
        oss << kLogPrefix << "InitializeStageAnchors fallback anchor at z=" << fallback.z << "\n";
        DebugTrace(oss.str());
        return;
    }

    for (const auto& spawn : spawnPoints)
    {
        m_pObjects.push_back(std::make_shared<TestObject>("OiiaioooooiaiFin", spawn, float3{ 1.0f, 1.0f, 1.0f }, float3{ 0.0f, 0.0f, 0.0f }));
    }

    std::ostringstream oss;
    oss << kLogPrefix << "InitializeStageAnchors loaded " << m_pObjects.size() << " stage anchors.\n";
    DebugTrace(oss.str());
}

void SceneGame::BroadcastPhaseAdvance(int newPhase)
{
    std::ostringstream oss;
    oss << kLogPrefix << "Broadcasting phase " << newPhase
        << " (stackedPeak=" << std::fixed << std::setprecision(2) << m_peakTowerHeight << ")\n";
    DebugTrace(oss.str());

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
    DebugTrace(oss.str());

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
    }

    InitializeStageAnchors();
    if (m_cameraAnchor && !m_pObjects.empty())
    {
        auto& anchorTransform = m_cameraAnchor->GetTransform();
        anchorTransform.position = m_pObjects.front()->GetPosition();
        anchorTransform.rotation = m_pObjects.front()->GetfRads();
        anchorTransform.scale = m_pObjects.front()->GetScale();
    }
    else if (!m_cameraAnchor)
    {
        DebugTrace(std::string(kLogPrefix) + "ResetPlaySession skipped anchor realignment: camera anchor missing.\n");
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "ResetPlaySession found no stage anchors to realign.\n");
    }

    m_countdownTimer.Reset();
    m_totalPlaySeconds = kDefaultPlaySeconds;
    m_remainingCountdownSeconds = m_totalPlaySeconds;
    m_lastAnnouncedCountdownSeconds = static_cast<int>(std::ceil(m_totalPlaySeconds));
    m_isPlaying = true;
    m_timeUp = false;
    m_currentTowerHeight = 0.0f;
    m_peakTowerHeight = 0.0f;
    m_maxTowerHeight = 0.0f;
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
    oss << kLogPrefix << "[Countdown] start duration=" << m_totalPlaySeconds << "s\n";
    DebugTrace(oss.str());
}

void SceneGame::UpdateCountdown(Core::UpdateContext& ctx)
{
    if (!m_isPlaying)
    {
        return;
    }

    const float delta = m_countdownTimer.Tick(ctx.dt);
    if (!(delta > 0.0f))
    {
        return;
    }

    const float previousRemaining = m_remainingCountdownSeconds;
    m_remainingCountdownSeconds = std::max(0.0f, m_remainingCountdownSeconds - delta);

    const int previousSeconds = static_cast<int>(std::ceil(previousRemaining));
    const int currentSeconds = static_cast<int>(std::ceil(m_remainingCountdownSeconds));
    if (currentSeconds < previousSeconds)
    {
        const int intervalSeconds = std::max(1, static_cast<int>(kCountdownLogIntervalSeconds));
        const bool shouldLogInterval = (currentSeconds % intervalSeconds == 0);
        const bool shouldLogWarning = currentSeconds <= 3;
        if ((shouldLogInterval || shouldLogWarning) && currentSeconds != m_lastAnnouncedCountdownSeconds)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Countdown] remaining=" << std::fixed << std::setprecision(2)
                << m_remainingCountdownSeconds << "s\n";
            DebugTrace(oss.str());
            m_lastAnnouncedCountdownSeconds = currentSeconds;
        }
    }

    if (m_remainingCountdownSeconds <= 0.0f && !m_timeUp)
    {
        m_timeUp = true;
        m_isPlaying = false;
        ToggleGameplaySystems(false);
        std::ostringstream oss;
        oss << kLogPrefix << "[Countdown] timeUp peakTop=" << std::fixed << std::setprecision(2)
            << m_peakTowerHeight << "m score=" << ComputeScore() << "\n";
        DebugTrace(oss.str());
    }
}

void SceneGame::UpdateTowerHeightTelemetry()
{
    float instantHeight = 0.0f;
    if (m_sweetsManager)
    {
        instantHeight = m_sweetsManager->GetHighestStackedHeight();
    }

    m_currentTowerHeight = instantHeight;
    m_maxTowerHeight = std::max(m_maxTowerHeight, instantHeight);

    if (instantHeight > m_peakTowerHeight)
    {
        m_peakTowerHeight = instantHeight;
        std::ostringstream oss;
        oss << kLogPrefix << "[Tower] newPeakTop=" << std::fixed << std::setprecision(2)
            << m_peakTowerHeight << "m remaining=" << std::fixed << std::setprecision(2)
            << m_remainingCountdownSeconds << "s\n";
        DebugTrace(oss.str());
    }
}

void SceneGame::ProcessPostGameInput(Core::UpdateContext& ctx)
{
    if (!m_timeUp)
    {
        return;
    }

    Core::Input& input = ctx.input;
    if (input.IsKeyTrigger(kConfirmInputKey))
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[Result] Confirm accepted peakTop=" << std::fixed << std::setprecision(2)
            << m_peakTowerHeight << "m score=" << ComputeScore() << "\n";
        DebugTrace(oss.str());
        ResetPlaySession();
        return;
    }

    if (input.IsKeyTrigger(kRetryInputKey))
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[Result] Retry input ignored until confirm. peakTop="
            << std::fixed << std::setprecision(2) << m_peakTowerHeight << "m\n";
        DebugTrace(oss.str());
    }
}

void SceneGame::HandleManualRetryInput(Core::UpdateContext& ctx)
{
    if (!m_isPlaying)
    {
        return;
    }

    if (!ctx.input.IsKeyTrigger(kRetryInputKey))
    {
        return;
    }

    std::ostringstream oss;
    oss << kLogPrefix << "[Countdown] Retry requested elapsed="
        << std::fixed << std::setprecision(2) << (m_totalPlaySeconds - m_remainingCountdownSeconds)
        << "s peak=" << std::fixed << std::setprecision(2) << m_peakTowerHeight << "m\n";
    DebugTrace(oss.str());
    ResetPlaySession();
}

void SceneGame::ToggleGameplaySystems(bool enabled)
{
    if (m_physicsMachine)
    {
        m_physicsMachine->Enable(enabled);
    }
    if (m_contactSystem)
    {
        m_contactSystem->Enable(enabled);
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

#if defined(_DEBUG)
void SceneGame::RenderRunTelemetryOverlay() const
{
#if defined(_WIN32)
    constexpr int kOverlayBaseRow = 4;
    const auto writeLine = [](int row, const std::string& text)
    {
        DebugConsole::SetDrawPos(1, row);
        std::string padded = text;
        if (padded.size() < 80)
        {
            padded.append(80 - padded.size(), ' ');
        }
        std::cout << padded;
    };

    std::ostringstream timerLine;
    timerLine << "[SceneGame] Timer: " << std::fixed << std::setprecision(1)
        << m_remainingCountdownSeconds << "s (state=" << (m_timeUp ? "Result" : "Playing") << ')';
    writeLine(kOverlayBaseRow, timerLine.str());

    std::ostringstream heightLine;
    heightLine << "[SceneGame] Tower height: live=" << std::fixed << std::setprecision(2)
        << m_currentTowerHeight << "m peak=" << std::fixed << std::setprecision(2) << m_peakTowerHeight << "m";
    writeLine(kOverlayBaseRow + 1, heightLine.str());

    std::ostringstream scoreLine;
    scoreLine << "[SceneGame] Score: " << ComputeScore() << " | Confirm=Enter Retry=R";
    if (m_timeUp)
    {
        scoreLine << " (waiting for confirm)";
    }
    writeLine(kOverlayBaseRow + 2, scoreLine.str());

    std::ostringstream carryLine;
    carryLine << "[SceneGame] Player carry: ";
    if (player && player->IsCarryingSweets())
    {
        const auto carried = player->GetCarriedSweets();
        carryLine << (carried ? carried->GetTag() : "<unknown>");
    }
    else
    {
        carryLine << "empty";
    }
    writeLine(kOverlayBaseRow + 3, carryLine.str());

    std::cout.flush();
#else
    (void)this;
#endif
}
#endif

int SceneGame::ComputeScore() const
{
    const float rawPhaseMultiplier = m_phaseManager.GetRewardMultiplier();
    const float phaseMultiplier = (std::isfinite(rawPhaseMultiplier) && rawPhaseMultiplier > 0.0f)
        ? rawPhaseMultiplier
        : 1.0f;
    const float totalMultiplier = kBaseScorePerMeter * phaseMultiplier;
    const int score = static_cast<int>(std::lround(m_peakTowerHeight * totalMultiplier));

    const bool multiplierChanged = !std::isfinite(m_lastScoreLogMultiplier)
        || std::fabs(m_lastScoreLogMultiplier - phaseMultiplier) > std::numeric_limits<float>::epsilon();
    if (score != m_lastScoreLogValue || multiplierChanged)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[Score] peak=" << std::fixed << std::setprecision(2) << m_peakTowerHeight
            << "m phase=" << m_phaseManager.GetCurrentPhase()
            << " phaseMultiplier=" << phaseMultiplier
            << " basePerMeter=" << kBaseScorePerMeter
            << " totalMultiplier=" << totalMultiplier
            << " score=" << score << "\n";
        DebugTrace(oss.str());
        m_lastScoreLogValue = score;
        m_lastScoreLogMultiplier = phaseMultiplier;
    }

    return score;
}
