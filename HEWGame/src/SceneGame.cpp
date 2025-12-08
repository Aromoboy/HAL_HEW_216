#include "AABBCollider.h"
#include "Transform.h"
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
#include "SweetsManager.h"
#include "BackgroundRenderer.h"
#include "Sweets/BaseSweets.h"
#include "Entities/Player.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
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
        constexpr float kPlayerSpeedBoostStep = 1.1f;
        constexpr float kMaxPlayerSpeedMultiplier = 3.0f;
        constexpr float kFallbackPlayerMoveSpeed = 15.0f;
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
            DirectX::XMFLOAT4(1.0f, 0.55f, 0.25f, 1.0f),
            3.0f,
            32.0f,
            DirectX::XMFLOAT4(0.95f, 0.95f, 1.0f, 0.8f)
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
                ::DebugTrace(oss.str());
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
                    ::DebugTrace(oss.str());
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
                        ::DebugTrace(oss.str());
                    }
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse value for key '" << key << "' on line "
                        << lineNumber << ": " << ex.what() << "\n";
                    ::DebugTrace(oss.str());
                }
            }

            std::ostringstream oss;
            oss << kLogPrefix << "Loaded physics tuning from '" << kPhysicsConfigPath
                << "' (gravity=" << settings.gravity
                << ", damping=" << settings.damping
                << ", penetrationTolerance=" << settings.penetrationTolerance
                << ", restitution=" << settings.restitution << ").\n";
            ::DebugTrace(oss.str());
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
            //カメラ設定
            float3{ 0.0f, 3.5f, -25.0f },
            45.0f,
            DirectX::XMConvertToRadians(0.0f),
            DirectX::XMConvertToRadians(-10.0f)
        };

        constexpr float kPlatformScreenBottomFraction = 0.08f;
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

        bool TryComputeViewportBoundsOnPlane(
            const CameraSpace::PerspectiveCamera& camera,
            float viewportWidth,
            float viewportHeight,
            float planeZ,
            float& outMinX,
            float& outMaxX,
            float& outMinY,
            float& outMaxY)
        {
            if (!(std::isfinite(viewportWidth) && viewportWidth > 0.0f
                && std::isfinite(viewportHeight) && viewportHeight > 0.0f))
            {
                return false;
            }

            const DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&camera.GetView());
            const DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&camera.GetProjection());

            const std::array<std::pair<float, float>, 4> corners
            {
                std::pair<float, float>{0.0f, 0.0f},
                std::pair<float, float>{viewportWidth, 0.0f},
                std::pair<float, float>{viewportWidth, viewportHeight},
                std::pair<float, float>{0.0f, viewportHeight}
            };

            bool hasBounds = false;
            float minX = 0.0f;
            float maxX = 0.0f;
            float minY = 0.0f;
            float maxY = 0.0f;
            constexpr float kDirectionEpsilon = 1e-5f;

            for (const auto& corner : corners)
            {
                const DirectX::XMVECTOR nearPoint = DirectX::XMVector3Unproject(
                    DirectX::XMVectorSet(corner.first, corner.second, 0.0f, 1.0f),
                    0.0f,
                    0.0f,
                    viewportWidth,
                    viewportHeight,
                    0.0f,
                    1.0f,
                    proj,
                    view,
                    DirectX::XMMatrixIdentity());
                const DirectX::XMVECTOR farPoint = DirectX::XMVector3Unproject(
                    DirectX::XMVectorSet(corner.first, corner.second, 1.0f, 1.0f),
                    0.0f,
                    0.0f,
                    viewportWidth,
                    viewportHeight,
                    0.0f,
                    1.0f,
                    proj,
                    view,
                    DirectX::XMMatrixIdentity());

                const DirectX::XMVECTOR direction = DirectX::XMVectorSubtract(farPoint, nearPoint);
                const float dirZ = DirectX::XMVectorGetZ(direction);
                if (std::fabs(dirZ) < kDirectionEpsilon)
                {
                    continue;
                }

                const float nearZ = DirectX::XMVectorGetZ(nearPoint);
                const float t = (planeZ - nearZ) / dirZ;
                if (t < 0.0f || t > 1.0f)
                {
                    continue;
                }

                const DirectX::XMVECTOR hitPoint = DirectX::XMVectorAdd(nearPoint, DirectX::XMVectorScale(direction, t));
                const float hitX = DirectX::XMVectorGetX(hitPoint);
                const float hitY = DirectX::XMVectorGetY(hitPoint);

                if (!hasBounds)
                {
                    minX = maxX = hitX;
                    minY = maxY = hitY;
                    hasBounds = true;
                }
                else
                {
                    minX = std::min(minX, hitX);
                    maxX = std::max(maxX, hitX);
                    minY = std::min(minY, hitY);
                    maxY = std::max(maxY, hitY);
                }
            }

            if (!hasBounds)
            {
                return false;
            }

            outMinX = minX;
            outMaxX = maxX;
            outMinY = minY;
            outMaxY = maxY;
            return true;
        }

        Transform ComputePlatformTransformForCamera(
            const CameraSpace::PerspectiveCamera& camera,
            float viewportWidth,
            float viewportHeight,
            const Stage::PlatformDescriptor& descriptor,
            float screenFraction,
            float stageCenterX,
            std::size_t platformIndex)
        {
            Transform result{};
            result.position = float3{ stageCenterX + descriptor.center.x, descriptor.center.y, descriptor.center.z };
            result.rotation = float3{ 0.0f, 0.0f, 0.0f };
            float targetWidth = descriptor.scale.x;
            float targetHeight = descriptor.scale.y;
            const float targetDepth = std::max(0.01f, descriptor.depth);

            float viewportMinX = 0.0f;
            float viewportMaxX = 0.0f;
            float viewportMinY = 0.0f;
            float viewportMaxY = 0.0f;
            const bool boundsValid = TryComputeViewportBoundsOnPlane(
                camera,
                viewportWidth,
                viewportHeight,
                descriptor.center.z,
                viewportMinX,
                viewportMaxX,
                viewportMinY,
                viewportMaxY);
            if (boundsValid)
            {
                const float worldWidth = viewportMaxX - viewportMinX;
                const float worldHeight = viewportMaxY - viewportMinY;
                const float widthFraction = std::clamp(descriptor.widthFraction, 0.01f, 1.0f);
                const float heightFraction = std::clamp(descriptor.heightFraction, 0.01f, 1.0f);
                targetWidth = std::max(worldWidth * widthFraction, 0.01f);
                targetHeight = std::max(worldHeight * heightFraction, 0.01f);
            }
            else
            {
                DebugTrace(std::string(kLogPrefix) + "[PlatformAlign] Failed to derive viewport bounds; falling back to config platformScale.\n");
            }

            result.scale = float3{ targetWidth, targetHeight, targetDepth };

            const float sanitizedFraction = std::clamp(screenFraction, 0.0f, 1.0f);
            const float configuredTopY = result.position.y + (result.scale.y * 0.5f);
            float derivedTopY = configuredTopY;
            float derivedPlaneZ = result.position.z;
            bool derivedValid = false;

            if (std::isfinite(viewportWidth) && viewportWidth > 0.0f
                && std::isfinite(viewportHeight) && viewportHeight > 0.0f)
            {
                const DirectX::XMFLOAT4X4& viewMatrix = camera.GetView();
                const DirectX::XMFLOAT4X4& projMatrix = camera.GetProjection();
                const DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&viewMatrix);
                const DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&projMatrix);
                const float pixelX = viewportWidth * 0.5f;
                const float pixelY = viewportHeight * (1.0f - sanitizedFraction);
                const float clampedPixelY = std::clamp(pixelY, 0.0f, viewportHeight);
                const DirectX::XMVECTOR nearPoint = DirectX::XMVector3Unproject(
                    DirectX::XMVectorSet(pixelX, clampedPixelY, 0.0f, 1.0f),
                    0.0f,
                    0.0f,
                    viewportWidth,
                    viewportHeight,
                    0.0f,
                    1.0f,
                    proj,
                    view,
                    DirectX::XMMatrixIdentity());
                const DirectX::XMVECTOR farPoint = DirectX::XMVector3Unproject(
                    DirectX::XMVectorSet(pixelX, clampedPixelY, 1.0f, 1.0f),
                    0.0f,
                    0.0f,
                    viewportWidth,
                    viewportHeight,
                    0.0f,
                    1.0f,
                    proj,
                    view,
                    DirectX::XMMatrixIdentity());
                DirectX::XMVECTOR directionVec = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(farPoint, nearPoint));
                DirectX::XMFLOAT3 direction{};
                DirectX::XMStoreFloat3(&direction, directionVec);
                const float3 cameraPosition = camera.GetPosition();
                constexpr float kDirectionEpsilon = 1e-4f;
                if (std::fabs(direction.x) > kDirectionEpsilon)
                {
                    const float t = (stageCenterX - cameraPosition.x) / direction.x;
                    if (t > 0.0f)
                    {
                        derivedValid = true;
                        derivedTopY = cameraPosition.y + direction.y * t;
                        derivedPlaneZ = cameraPosition.z + direction.z * t;
                    }
                }
            }

            if (derivedValid)
            {
                const float deltaY = derivedTopY - configuredTopY;
                const float deltaZ = derivedPlaneZ - result.position.z;
                result.position.y += deltaY;
                result.position.z += deltaZ;
            }

            std::ostringstream oss;
            oss << kLogPrefix << "[PlatformAlign] fraction=" << sanitizedFraction
                << " viewport=" << viewportWidth << 'x' << viewportHeight
                << " platformIndex=" << platformIndex
                << " boundsValid=" << std::boolalpha << boundsValid
                << " scale=(" << result.scale.x << ',' << result.scale.y << ',' << result.scale.z << ')'
                << " topY=" << derivedTopY
                << " worldZ=" << derivedPlaneZ
                << " applied=" << std::boolalpha << derivedValid
                << " centerY=" << result.position.y
                << " planeZ=" << result.position.z << "\n";
            ::DebugTrace(oss.str());

            return result;
        }
}

float3 fRads = { 0.0f, 0.0f, 0.0f };

SceneGame::SceneGame()
{
    const bool stageLoaded = m_stageData.LoadFromFile(kStageConfigPath);
    m_phaseManager.Initialize(m_stageData.GetPhaseThresholds(), m_stageData.GetPhaseRewardMultipliers());
    m_playerBaseMoveSpeed = m_stageData.GetPlayerMoveSpeed();
    if (!std::isfinite(m_playerBaseMoveSpeed) || m_playerBaseMoveSpeed <= 0.0f)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Player move speed " << m_playerBaseMoveSpeed
            << " invalid; using fallback " << kFallbackPlayerMoveSpeed << "\n";
        ::DebugTrace(oss.str());
        m_playerBaseMoveSpeed = kFallbackPlayerMoveSpeed;
    }
    m_playerSpeedMultiplier = 1.0f;
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Stage data " << (stageLoaded ? "loaded" : "defaulted")
            << " planeZ=" << m_stageData.GetPlaneZ()
            << " thresholds=" << m_stageData.GetPhaseThresholds().size()
            << " multipliers=" << m_stageData.GetPhaseRewardMultipliers().size()
            << " spawnPoints=" << m_stageData.GetSpawnPoints().size()
            << " platforms=" << m_stageData.GetPlatformCount()
            << " playerSpeed=" << m_playerBaseMoveSpeed
            << " guidelineHeight=" << m_stageData.GetGuidelineHeight()
            << " heightBonusThreshold=" << m_stageData.GetHeightBonusThreshold()
            << " heightBonusSeconds=" << m_stageData.GetHeightBonusSeconds()
            << " background='" << m_stageData.GetBackgroundTexturePath() << "'\n";
        for (std::size_t i = 0; i < m_stageData.GetPlatformCount(); ++i)
        {
            const auto& descriptor = m_stageData.GetPlatform(i);
            oss << kLogPrefix << " PlatformDesc[" << i << "] center=(" << descriptor.center.x << ',' << descriptor.center.y
                << ',' << descriptor.center.z << ") scale=(" << descriptor.scale.x << ',' << descriptor.scale.y << ','
                << descriptor.scale.z << ") depth=" << descriptor.depth
                << " widthFraction=" << descriptor.widthFraction
                << " heightFraction=" << descriptor.heightFraction << "\n";
        }
        ::DebugTrace(oss.str());
    }

    m_guidelineHeightMeters = m_stageData.GetGuidelineHeight();
    if (!std::isfinite(m_guidelineHeightMeters) || m_guidelineHeightMeters <= 0.0f)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Guideline height " << m_guidelineHeightMeters
            << " invalid; overlay guideline disabled.\n";
        ::DebugTrace(oss.str());
        m_guidelineHeightMeters = 0.0f;
    }

    m_heightBonusThresholdMeters = m_stageData.GetHeightBonusThreshold();
    if (!std::isfinite(m_heightBonusThresholdMeters) || m_heightBonusThresholdMeters <= 0.0f)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Height bonus threshold " << m_heightBonusThresholdMeters
            << " invalid; timer bonus disabled.\n";
        ::DebugTrace(oss.str());
        m_heightBonusThresholdMeters = 0.0f;
    }

    m_heightBonusSeconds = m_stageData.GetHeightBonusSeconds();
    if (!std::isfinite(m_heightBonusSeconds))
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Height bonus seconds " << m_heightBonusSeconds
            << " invalid; timer bonus disabled.\n";
        ::DebugTrace(oss.str());
        m_heightBonusSeconds = 0.0f;
    }
    else if (m_heightBonusSeconds <= 0.0f)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Height bonus seconds <= 0; timer bonus disabled.\n";
        ::DebugTrace(oss.str());
        m_heightBonusSeconds = 0.0f;
    }

    m_heightBonusArmed = (m_heightBonusThresholdMeters > 0.0f && m_heightBonusSeconds > 0.0f);

    const std::string backgroundTexture = m_stageData.GetBackgroundTexturePath();
    if (!backgroundTexture.empty())
    {
        BackgroundRendererConfig backgroundConfig{};
        backgroundConfig.texturePath = backgroundTexture;
        m_backgroundRenderer = std::make_unique<BackgroundRenderer>();
        if (!m_backgroundRenderer->Initialize(backgroundConfig))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Background renderer failed for texture '" << backgroundTexture << "'.\n";
            ::DebugTrace(oss.str());
            m_backgroundRenderer.reset();
        }
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "Background texture path empty; background renderer skipped.\n");
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
        const bool spawnConfigLoaded = m_sweetsManager->LoadPhaseSpawnConfig(kSweetsSpawnConfigPath);
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Sweets spawn config " << (spawnConfigLoaded ? "loaded" : "defaulted")
                << " from '" << kSweetsSpawnConfigPath << "'.\n";
            ::DebugTrace(oss.str());
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
        ::DebugTrace(oss.str());
    }

    RenderTarget* pRTV = GetDefaultRTV();
    DepthStencil* pDSV = GetDefaultDSV();
    SetRenderTargets(1, &pRTV, pDSV);
    SetDepthTest(true);
    InitializeStageAnchors();

    m_cameraAnchor = std::make_shared<Entities::Entity>("PlayerAnchor");
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

    m_render = std::make_unique<ModelRender>();
    if (m_drawSystem && m_render)
    {
        m_drawSystem->SetRenderer(m_render.get());
    }
    float platformViewportWidth = static_cast<float>(SCREEN_WIDTH);
    float platformViewportHeight = static_cast<float>(SCREEN_HEIGHT);
    if (RenderTarget* rtv = GetDefaultRTV())
    {
        platformViewportWidth = static_cast<float>(rtv->GetWidth());
        platformViewportHeight = static_cast<float>(rtv->GetHeight());
    }

    const auto& platformDescriptors = m_stageData.GetPlatforms();
    if (platformDescriptors.empty())
    {
        DebugTrace(std::string(kLogPrefix) + "No platform descriptors available; skipping foundation creation.\n");
    }
    else
    {
        m_foundationPlatforms.resize(platformDescriptors.size());
        m_platformUpdateRegistered.assign(platformDescriptors.size(), false);
        m_platformDrawRegistered.assign(platformDescriptors.size(), false);

        for (std::size_t i = 0; i < platformDescriptors.size(); ++i)
        {
            const auto& descriptor = platformDescriptors[i];
            Transform platformTransform = ComputePlatformTransformForCamera(
                *m_gameCamera,
                platformViewportWidth,
                platformViewportHeight,
                descriptor,
                kPlatformScreenBottomFraction,
                kStageCenterX,
                i);
            auto platformCollider = std::make_shared<Collision::AABBCollider>(platformTransform, false, float3{ 1.0f, 1.7f, 1.0f }, float3{ 0.0f, 0.0f, 0.0f }, Collision::CollisionLayer::Floor);

            std::ostringstream tagStream;
            tagStream << "StageFoundation" << i;
            auto platform = std::make_unique<Stage::FoundationPlatform>(
                tagStream.str(),
                platformTransform,
                platformCollider,
                descriptor.color,
                Core::UpdatePhase::PrePhysics,
                -100 - static_cast<int>(i));

            if (!platform)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Failed to allocate foundation platform at index " << i << " (tag='"
                    << tagStream.str() << "').\n";
                ::DebugTrace(oss.str());
                continue;
            }

            m_foundationPlatforms[i] = std::move(platform);
        }

        RefreshPlatformAnchor(true);

        for (std::size_t i = 0; i < m_foundationPlatforms.size(); ++i)
        {
            auto& platform = m_foundationPlatforms[i];
            if (!platform)
            {
                continue;
            }

            RegisterPhysicsBody(*platform);

            const auto& scale = platform->GetTransform().scale;
            const auto& pos = platform->GetTransform().position;
            std::ostringstream oss;
            oss << kLogPrefix << "Stage foundation[" << i << "] initialized at ("
                << pos.x << ',' << pos.y << ',' << pos.z << ") scale=("
                << scale.x << ',' << scale.y << ',' << scale.z << ") color=("
                << platform->GetDebugColor().x << ','
                << platform->GetDebugColor().y << ','
                << platform->GetDebugColor().z << ") tag='" << platform->GetTag() << "'.\n";
            ::DebugTrace(oss.str());

            if (m_drawSystem)
            {
                m_drawSystem->RegisterRenderable(*platform);
                m_platformDrawRegistered[i] = true;
                std::ostringstream drawOss;
                drawOss << kLogPrefix << "Stage foundation[" << i << "] registered with DrawSystem.\n";
                DebugTrace(drawOss.str());
            }
        }

        auto selectPlatform = [&](std::size_t preferredIndex) -> Stage::FoundationPlatform*
        {
            if (preferredIndex < m_foundationPlatforms.size() && m_foundationPlatforms[preferredIndex])
            {
                return m_foundationPlatforms[preferredIndex].get();
            }
            for (auto& platform : m_foundationPlatforms)
            {
                if (platform)
                {
                    return platform.get();
                }
            }
            return nullptr;
        };

        m_platePlatform = selectPlatform(platformDescriptors.size() > 1 ? 1 : 0);
        if (m_platePlatform)
        {
            const auto& pos = m_platePlatform->GetTransform().position;
            const auto& scale = m_platePlatform->GetTransform().scale;
            std::ostringstream oss;
            oss << kLogPrefix << "Plate platform selected tag='" << m_platePlatform->GetTag() << "' pos=("
                << pos.x << ',' << pos.y << ',' << pos.z << ") scale=("
                << scale.x << ',' << scale.y << ',' << scale.z << ").\n";
            DebugTrace(oss.str());
        }
        else
        {
            DebugTrace(std::string(kLogPrefix) + "No plate platform available; player and sweets will use default spawn heights.\n");
        }
    }

    if (m_sweetsManager)
    {
        std::vector<float3> spawnOrigins = m_stageData.GetSpawnPoints();
        if (m_platePlatform)
        {
            const auto& plateTransform = m_platePlatform->GetTransform();
            const float plateTopY = plateTransform.position.y + (plateTransform.scale.y * 0.5f);
            for (auto& origin : spawnOrigins)
            {
                origin.y = plateTopY;
                origin.z = plateTransform.position.z;
            }

            std::ostringstream oss;
            oss << kLogPrefix << "[SpawnOrigin] aligned " << spawnOrigins.size() << " origins to plate '"
                << m_platePlatform->GetTag() << "' topY=" << plateTopY
                << " planeZ=" << plateTransform.position.z << "\n";
            DebugTrace(oss.str());
        }
        m_sweetsManager->SetSpawnOrigins(spawnOrigins);
    }

    const float platePlaneZ = m_platePlatform ? m_platePlatform->GetTransform().position.z : m_stageData.GetPlaneZ();
    std::shared_ptr<Collision::ICollider> collider;
    Transform trans =
    { float3{0.0f, 0.0f, platePlaneZ}, float3{0.0f, 0.0f, 0.0f}, float3{1.0f, 1.0f, 1.0f} };
    collider = std::make_shared<Collision::AABBCollider>(trans, false, float3{ 1.0f, 1.0f, 1.0f }, float3{ 0.0f, 0.0f, 0.0f }, Collision::CollisionLayer::Player);
    player = std::make_unique<Entities::Player>(
        Transform{ float3{ 0.0f, 0.0f, platePlaneZ }, float3{ 0.0f, 0.0f, 0.0f }, float3{ 1.0f, 1.0f, 1.0f } },
        std::shared_ptr<Render::Model>{},
        collider,
        Physics::Physical{ float3{ 0.0f, 0.0f, 0.0f }, float3{ 0.0f, 0.0f, 0.0f }, 1.0f, 0.98f },
        m_physicsMachine.get(),
        m_contactSystem.get(),
        m_sweetsManager.get(),
        Collision::CollisionLayer::Player,
        true,
        Core::UpdatePhase::Gameplay,
        0,
        "player");

    if (player)
    {
        const auto& fallbackPlatform = m_stageData.GetPlatform(0);
        float platformTopY = fallbackPlatform.center.y + (fallbackPlatform.scale.y * 0.5f);
        if (m_platePlatform)
        {
            const auto& platformXform = m_platePlatform->GetTransform();
            platformTopY = platformXform.position.y + (platformXform.scale.y * 0.5f);
        }
        player->SetMinimumY(platformTopY);
        player->SetMoveSpeed(m_playerBaseMoveSpeed);
        RegisterPhysicsBody(*player);
        {
            std::ostringstream oss;
            const bool isKinematic = player->IsKinematic();
            oss << kLogPrefix << "Player physics body registered (kinematic="
                << std::boolalpha << isKinematic
                << ", weight=" << player->GetPhysical().weight << ").\n";
            ::DebugTrace(oss.str());
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
    // 数字テクスチャをロード
    m_numberTexture = new Texture();
    m_numberTexture->Create("Assets/Texture/number.png"); 

    // タイマー表示を初期化
    m_timerDisplay.Init(m_numberTexture);
    m_timerDisplay.SetPosition(700.0f, 50.0f);  // 画面右上
    m_timerDisplay.SetDigitSize(50.0f, 70.0f);   // 数字のサイズ

}

SceneGame::~SceneGame()
{
    if (m_hudRenderer)
    {
        m_hudRenderer->Shutdown();
        m_hudRenderer.reset();
    }

    for (std::size_t i = 0; i < m_foundationPlatforms.size(); ++i)
    {
        if (i < m_platformUpdateRegistered.size() && m_platformUpdateRegistered[i] && m_foundationPlatforms[i])
        {
            Unregister(*m_foundationPlatforms[i]);
            m_platformUpdateRegistered[i] = false;
        }

        if (i < m_platformDrawRegistered.size() && m_platformDrawRegistered[i] && m_drawSystem && m_foundationPlatforms[i])
        {
            m_drawSystem->UnregisterRenderable(*m_foundationPlatforms[i]);
            m_platformDrawRegistered[i] = false;
        }
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
                std::fill(m_platformDrawRegistered.begin(), m_platformDrawRegistered.end(), false);
        }

        if (m_sweetsRegistered && m_sweetsManager)
        {
                GetUpdateScheduler().Unregister(*m_sweetsManager);
                m_sweetsRegistered = false;
        }

        if (m_numberTexture)
        {
            delete m_numberTexture;
            m_numberTexture = nullptr;
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

        for (std::size_t i = 0; i < m_foundationPlatforms.size(); ++i)
        {
                auto* platform = m_foundationPlatforms[i].get();
                if (!platform)
                {
                        continue;
                }

                if (i >= m_platformUpdateRegistered.size())
                {
                        m_platformUpdateRegistered.resize(m_foundationPlatforms.size(), false);
                }
                if (!m_platformUpdateRegistered[i])
                {
                        Register(*platform);
                        m_platformUpdateRegistered[i] = true;
                }

                if (m_drawSystem)
                {
                        if (i >= m_platformDrawRegistered.size())
                        {
                                m_platformDrawRegistered.resize(m_foundationPlatforms.size(), false);
                        }
                        if (!m_platformDrawRegistered[i])
                        {
                                m_drawSystem->RegisterRenderable(*platform);
                                m_platformDrawRegistered[i] = true;
                        }
                }
        }


        if (player)
        {
                Register(*player);

                const bool tracked = m_registeredPhysicsBodies.find(player.get()) != m_registeredPhysicsBodies.end();
                if (!tracked)
                {
                        RegisterPhysicsBody(*player);
                        std::ostringstream oss;
                        const bool isKinematic = player->IsKinematic();
                        oss << kLogPrefix << "ConfigureUpdates ensured player body is registered (kinematic="
                            << std::boolalpha << isKinematic << ").\n";
                        ::DebugTrace(oss.str());
                }
        }

        ResetPlaySession();
}

void SceneGame::Update(Core::UpdateContext& ctx)
{
#if defined(_DEBUG)
        HandleColliderDebugCommands(ctx);
#endif
    RefreshPlatformAnchor(false);
    UpdateCountdown(ctx);
    UpdateTowerHeightTelemetry();

    float viewportWidth = static_cast<float>(SCREEN_WIDTH);
    float viewportHeight = static_cast<float>(SCREEN_HEIGHT);
    bool viewportFromDefault = true;
    bool boundsIssueThisFrame = false;
    if (RenderTarget* rtv = GetDefaultRTV())
    {
        viewportWidth = static_cast<float>(rtv->GetWidth());
        viewportHeight = static_cast<float>(rtv->GetHeight());
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

        // World bounds on X
        const float minX = camPos.x - halfWidth;
        const float maxX = camPos.x + halfWidth;

        // Log bounds if changed
        const bool changed =
            !m_cachedPlayerBoundsValid ||
            HasMeaningfulDifference(m_cachedPlayerMinX, minX) ||
            HasMeaningfulDifference(m_cachedPlayerMaxX, maxX);

        if (changed)
        {
            std::ostringstream oss;
            oss << "[SceneGame] [PlayerBounds] camX=" << camPos.x
                << " d=" << d
                << " fovX=" << DirectX::XMConvertToDegrees(fovX)
                << " halfWidth=" << halfWidth
                << " => minX=" << minX
                << " maxX=" << maxX << "\n";
            ::DebugTrace(oss.str());
        }

        // Apply to player
        if (player)
        {
            player->SetScreenBounds(minX, maxX, std::nullopt);
        }

        // Apply to sweets manager
        if (m_sweetsManager)
            m_sweetsManager->SetHorizontalBounds(minX, maxX);

        // Cache
        m_cachedPlayerMinX = minX;
        m_cachedPlayerMaxX = maxX;
        m_cachedPlayerBoundsValid = true;
    }


if (m_timeUp)
{
    ProcessPostGameInput(ctx);
    return;
}

    Core::Input& input = ctx.input;
    if (player)
    {
        const auto adjustMoveSpeed = [&](float requestedMultiplier, const char* reason)
        {
            if (!(m_playerBaseMoveSpeed > 0.0f))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[SpeedBoost] " << reason
                    << " ignored; base speed invalid (" << m_playerBaseMoveSpeed << ").\n";
                ::DebugTrace(oss.str());
                return;
            }

            const float currentMultiplier = m_playerSpeedMultiplier;
            const float clampedMultiplier = std::clamp(requestedMultiplier, 0.0f, kMaxPlayerSpeedMultiplier);
            const bool clamped = std::fabs(clampedMultiplier - requestedMultiplier) > std::numeric_limits<float>::epsilon();
            if (std::fabs(currentMultiplier - clampedMultiplier) <= std::numeric_limits<float>::epsilon())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[SpeedBoost] " << reason
                    << " requestedMultiplier=" << requestedMultiplier
                    << " has no effect (current=" << currentMultiplier << ").\n";
                ::DebugTrace(oss.str());
                return;
            }

            const float beforeSpeed = player->GetMoveSpeed();
            const float newMultiplier = clampedMultiplier;
            const float newSpeed = m_playerBaseMoveSpeed * newMultiplier;
            m_playerSpeedMultiplier = newMultiplier;
            player->SetMoveSpeed(newSpeed);

            std::ostringstream oss;
            oss << kLogPrefix << "[SpeedBoost] " << reason
                << " base=" << m_playerBaseMoveSpeed
                << " prevMultiplier=" << currentMultiplier
                << " newMultiplier=" << newMultiplier
                << " prevSpeed=" << beforeSpeed
                << " newSpeed=" << newSpeed
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
        ::DebugTrace(oss.str());
}
#endif

void SceneGame::Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera)
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
                  hudData.moveSpeedMultiplier = m_playerSpeedMultiplier;
                  if (player)
                  {
                          hudData.moveSpeedUnitsPerSecond = player->GetMoveSpeed();
                  }

                  float guidelineClip = 0.0f;
                  if (TryGetGuidelineClipY(*activeCamera, guidelineClip))
                  {
                          hudData.guidelineVisible = true;
                        hudData.guidelineClipY = guidelineClip;
                }

                m_hudRenderer->Draw(hudData);
        }

        m_timerDisplay.Draw(m_remainingCountdownSeconds);

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
    ::DebugTrace(oss.str());

    RefreshPlatformAnchor(true);
}

void SceneGame::RefreshPlatformAnchor(bool forceRecompute)
{
    if (m_foundationPlatforms.empty() || !m_gameCamera)
    {
        return;
    }

    float viewportWidth = static_cast<float>(SCREEN_WIDTH);
    float viewportHeight = static_cast<float>(SCREEN_HEIGHT);
    if (RenderTarget* rtv = GetDefaultRTV())
    {
        viewportWidth = static_cast<float>(rtv->GetWidth());
        viewportHeight = static_cast<float>(rtv->GetHeight());
    }
    else if (forceRecompute && !m_platformAnchorInitialized)
    {
        DebugTrace(std::string(kLogPrefix) + "[PlatformAlign] DefaultRTV missing; falling back to constant screen size.\n");
    }

    const float3 cameraPos = m_gameCamera->GetPosition();
    const float3 cameraRot = m_gameCamera->GetRotation();
    const float cameraFov = m_gameCamera->GetFieldOfViewDegrees();
    const float cameraAspect = m_gameCamera->GetAspectRatio();
    const float3 rigOffset = m_cameraRig ? m_cameraRig->GetOffset() : float3{ 0.0f, 0.0f, 0.0f };

    const bool cameraChanged = forceRecompute
        || !m_platformAnchorInitialized
        || HasMeaningfulDifference(m_cachedViewportWidth, viewportWidth)
        || HasMeaningfulDifference(m_cachedViewportHeight, viewportHeight)
        || HasMeaningfulDifference(m_cachedCameraPosition, cameraPos)
        || HasMeaningfulDifference(m_cachedCameraRotation, cameraRot)
        || HasMeaningfulDifference(m_cachedCameraRigOffset, rigOffset)
        || HasMeaningfulDifference(m_cachedCameraFovDegrees, cameraFov)
        || HasMeaningfulDifference(m_cachedCameraAspect, cameraAspect);

    if (!cameraChanged)
    {
        return;
    }

    const auto& platformDescriptors = m_stageData.GetPlatforms();
    const std::size_t platformCount = std::min(m_foundationPlatforms.size(), platformDescriptors.size());
    if (platformCount == 0)
    {
        return;
    }

    for (std::size_t i = 0; i < platformCount; ++i)
    {
        auto& platform = m_foundationPlatforms[i];
        if (!platform)
        {
            continue;
        }

        const auto& descriptor = platformDescriptors[i];
        Transform updated = ComputePlatformTransformForCamera(
            *m_gameCamera,
            viewportWidth,
            viewportHeight,
            descriptor,
            kPlatformScreenBottomFraction,
            kStageCenterX,
            i);
        platform->SetAnchorTransform(updated);
    }

    m_platformAnchorInitialized = true;

    m_cachedViewportWidth = viewportWidth;
    m_cachedViewportHeight = viewportHeight;
    m_cachedCameraPosition = cameraPos;
    m_cachedCameraRotation = cameraRot;
    m_cachedCameraRigOffset = rigOffset;
    m_cachedCameraFovDegrees = cameraFov;
    m_cachedCameraAspect = cameraAspect;
}

void SceneGame::RegisterPhysicsBody(Entities::PhysicalEntity& entity)
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
        ::DebugTrace(oss.str());
        return;
    }

    m_physicsMachine->RegisterBody(entity);
}

void SceneGame::UnregisterPhysicsBody(Entities::PhysicalEntity* entity)
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
        ::DebugTrace(oss.str());
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

    std::vector<Entities::PhysicalEntity*> bodies(m_registeredPhysicsBodies.begin(), m_registeredPhysicsBodies.end());
    for (Entities::PhysicalEntity* body : bodies)
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
        ::DebugTrace(oss.str());
        return;
    }

    for (const auto& spawn : spawnPoints)
    {
        m_pObjects.push_back(std::make_shared<TestObject>("OiiaioooooiaiFin", spawn, float3{ 1.0f, 1.0f, 1.0f }, float3{ 0.0f, 0.0f, 0.0f }));
    }

    std::ostringstream oss;
    oss << kLogPrefix << "InitializeStageAnchors loaded " << m_pObjects.size() << " stage anchors.\n";
    ::DebugTrace(oss.str());
}

void SceneGame::BroadcastPhaseAdvance(int newPhase)
{
    std::ostringstream oss;
    oss << kLogPrefix << "Broadcasting phase " << newPhase
        << " (stackedPeak=" << std::fixed << std::setprecision(2) << m_peakTowerHeight << ")\n";
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
        if (m_playerBaseMoveSpeed > 0.0f)
        {
            m_playerSpeedMultiplier = 1.0f;
            player->SetMoveSpeed(m_playerBaseMoveSpeed);
        }
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
    m_pendingTimeBonuses.clear();
    m_heightBonusArmed = (m_heightBonusThresholdMeters > 0.0f && m_heightBonusSeconds > 0.0f);
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
    ::DebugTrace(oss.str());
}

void SceneGame::UpdateCountdown(Core::UpdateContext& ctx)
{
    if (!m_isPlaying)
    {
        return;
    }

    if (!m_pendingTimeBonuses.empty())
    {
        while (!m_pendingTimeBonuses.empty())
        {
            const PendingTimeBonus bonus = m_pendingTimeBonuses.front();
            m_pendingTimeBonuses.pop_front();
            if (!(bonus.seconds > 0.0f))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[Countdown] Ignored non-positive bonus event seconds="
                    << bonus.seconds << " queue=" << m_pendingTimeBonuses.size() << "\n";
                ::DebugTrace(oss.str());
                continue;
            }

            m_remainingCountdownSeconds = std::max(0.0f, m_remainingCountdownSeconds);
            m_remainingCountdownSeconds += bonus.seconds;
            m_totalPlaySeconds += bonus.seconds;

            std::ostringstream oss;
            oss << kLogPrefix << "[Countdown] bonusApplied +=" << std::fixed << std::setprecision(2)
                << bonus.seconds << "s height=" << std::fixed << std::setprecision(2)
                << bonus.triggerHeight << "m remaining=" << std::fixed << std::setprecision(2)
                << m_remainingCountdownSeconds << "s queue=" << m_pendingTimeBonuses.size() << "\n";
            ::DebugTrace(oss.str());
        }
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
            ::DebugTrace(oss.str());
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
        ::DebugTrace(oss.str());
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
        ::DebugTrace(oss.str());
    }

    if (m_heightBonusThresholdMeters > 0.0f && m_heightBonusSeconds > 0.0f)
    {
        if (instantHeight >= m_heightBonusThresholdMeters)
        {
            if (m_heightBonusArmed)
            {
                m_heightBonusArmed = false;
                m_pendingTimeBonuses.push_back(PendingTimeBonus{ m_heightBonusSeconds, instantHeight });
                std::ostringstream oss;
                oss << kLogPrefix << "[TowerBonus] thresholdCrossed height=" << std::fixed << std::setprecision(2)
                    << instantHeight << "m threshold=" << std::fixed << std::setprecision(2)
                    << m_heightBonusThresholdMeters << "m bonus=" << std::fixed << std::setprecision(2)
                    << m_heightBonusSeconds << "s queue=" << m_pendingTimeBonuses.size() << "\n";
                ::DebugTrace(oss.str());
            }
        }
        else if (!m_heightBonusArmed)
        {
            m_heightBonusArmed = true;
            std::ostringstream oss;
            oss << kLogPrefix << "[TowerBonus] rearmed height=" << std::fixed << std::setprecision(2)
                << instantHeight << "m threshold=" << std::fixed << std::setprecision(2)
                << m_heightBonusThresholdMeters << "m\n";
            ::DebugTrace(oss.str());
        }
    }
}

void SceneGame::ProcessPostGameInput(Core::UpdateContext& ctx)
{
    if (!m_timeUp)
    {
        return;
    }

    Core::Input& input = ctx.input;
    if (input.IsTrigger(Core::ActionId::Confirm))
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[Result] Confirm accepted peakTop=" << std::fixed << std::setprecision(2)
            << m_peakTowerHeight << "m score=" << ComputeScore() << "\n";
        ::DebugTrace(oss.str());
        ResetPlaySession();
        return;
    }

    if (input.IsTrigger(Core::ActionId::Retry))
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[Result] Retry input ignored until confirm. peakTop="
            << std::fixed << std::setprecision(2) << m_peakTowerHeight << "m\n";
        ::DebugTrace(oss.str());
    }
}

void SceneGame::HandleManualRetryInput(Core::UpdateContext& ctx)
{
    if (!m_isPlaying)
    {
        return;
    }

    if (!ctx.input.IsTrigger(Core::ActionId::Retry))
    {
        return;
    }

    std::ostringstream oss;
    oss << kLogPrefix << "[Countdown] Retry requested elapsed="
        << std::fixed << std::setprecision(2) << (m_totalPlaySeconds - m_remainingCountdownSeconds)
        << "s peak=" << std::fixed << std::setprecision(2) << m_peakTowerHeight << "m\n";
    ::DebugTrace(oss.str());
    ResetPlaySession();
}

bool SceneGame::TryGetGuidelineClipY(const CameraSpace::ICamera& camera, float& outClipY) const
{
    if (m_guidelineHeightMeters <= 0.0f)
    {
        if (!m_guidelineInvalidLogged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Guideline] Disabled because height=" << m_guidelineHeightMeters << "\n";
            ::DebugTrace(oss.str());
            m_guidelineInvalidLogged = true;
        }
        return false;
    }

    DirectX::XMFLOAT4X4 viewMatrix = camera.GetView();
    DirectX::XMFLOAT4X4 projMatrix = camera.GetProjection();
    const DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&viewMatrix);
    const DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&projMatrix);
    const DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);
    const DirectX::XMVECTOR worldPos = DirectX::XMVectorSet(0.0f, m_guidelineHeightMeters, m_stageData.GetPlaneZ(), 1.0f);
    const DirectX::XMVECTOR clipPos = DirectX::XMVector4Transform(worldPos, viewProj);
    const float clipW = DirectX::XMVectorGetW(clipPos);
    if (!std::isfinite(clipW) || std::fabs(clipW) <= std::numeric_limits<float>::epsilon())
    {
        if (!m_guidelineInvalidLogged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Guideline] Invalid clip W=" << clipW
                << " threshold=" << m_guidelineHeightMeters
                << " planeZ=" << m_stageData.GetPlaneZ() << "\n";
            ::DebugTrace(oss.str());
            m_guidelineInvalidLogged = true;
        }
        return false;
    }

    const float clipY = DirectX::XMVectorGetY(clipPos) / clipW;
    if (!std::isfinite(clipY))
    {
        if (!m_guidelineInvalidLogged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Guideline] Non-finite clipY derived from threshold="
                << m_guidelineHeightMeters << " clipW=" << clipW << "\n";
            ::DebugTrace(oss.str());
            m_guidelineInvalidLogged = true;
        }
        return false;
    }

    outClipY = std::clamp(clipY, -1.0f, 1.0f);
    if ((clipY < -1.0f || clipY > 1.0f))
    {
        if (!m_guidelineClampLogged)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Guideline] Clamped clipY from " << clipY
                << " to " << outClipY << " threshold=" << m_guidelineHeightMeters
                << " planeZ=" << m_stageData.GetPlaneZ() << "\n";
            ::DebugTrace(oss.str());
            m_guidelineClampLogged = true;
        }
    }
    else
    {
        m_guidelineClampLogged = false;
    }

    m_guidelineInvalidLogged = false;
    return true;
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
    if (player)
    {
        for (auto it : player->GetCarriedSweets())
        {
            const auto carried = it;
            carryLine << (carried ? carried->GetTag() : "<unknown>");
        }
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
        ::DebugTrace(oss.str());
        m_lastScoreLogValue = score;
        m_lastScoreLogMultiplier = phaseMultiplier;
    }

    return score;
}
