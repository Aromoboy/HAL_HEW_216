#include "SceneComposer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>

#include "BackgroundRenderer.h"
#include "ConfigSectionReader.h"
#include "Core/UpdateScheduler.h"
#include "GameplayConfig.h"
#include "ModelCache.h"
#include "SceneGameConfigLoader.h"
#include "SweetsManager.h"
#include "SweetsPrototypeManifest.h"
#include "Systems/Draw/DrawSystem.h"
#include "Systems/Physics/PhysicsBinder.h"
#include "UIManager.h"

#include <DirectXMath.h>
#if defined(_WIN32)
#       ifndef NOMINMAX
#               define NOMINMAX
#       endif
#       include <windows.h>
#endif

namespace
{
    constexpr const char* kLogPrefix = "[SceneComposer] ";

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

    constexpr const char* kGameplayConfigPath = GameplayConfig::kConfigPath;
    constexpr float kFallbackPlayerMoveSpeed = 10.0f;
    constexpr float kDefaultPickupMaxDistance = 0.5f;
    constexpr float kDefaultPickupDepthThreshold = std::numeric_limits<float>::infinity();
    constexpr float kGuidelineAdvanceCooldownSeconds = 0.25f;
    constexpr float kGuidelineThicknessMeters = 0.05f;

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

    const ResultLayoutConfig kResultLayoutConfig
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
}

SceneComposer::SceneComposer(Core::UpdateScheduler& scheduler)
    : m_scheduler(scheduler)
{
}

SceneComposer::Composition SceneComposer::ComposeBootstrap()
{
    Composition result{};

    ComposeConfig(result);
    if (!result.stageConfigLoaded)
    {
        return result;
    }
    ComposeRules(result);
    ComposePhysics(result);
    ComposeUI(result);

    return result;
}

void SceneComposer::Compose(Composition& result)
{
    ComposeLayout(result);
}

void SceneComposer::ComposeConfig(Composition& result)
{
    result.playerSpawnTransform = SceneGameConfig::LoadPlayerTransformConfig();
    result.renderPostProcessSettings = SceneGameConfig::LoadRenderPostProcessSettings();
    result.lightingSettings = SceneGameConfig::LoadLightingSettings();
    result.blobShadowSettings = SceneGameConfig::LoadBlobShadowSettings();
    result.materialTuningSettings = SceneGameConfig::LoadMaterialTuningSettings();
    result.sweetsOutOfBoundsSettings = SceneGameConfig::LoadSweetsOutOfBoundsSettings();
    result.sweetsSpawnLimitSettings = SceneGameConfig::LoadSweetsSpawnLimitSettings();

    const char* stageConfigSectionUsed = GameplayConfig::kSectionStageChecklist;
    bool stageLoaded = result.stageData.LoadFromFile(kGameplayConfigPath, stageConfigSectionUsed);
    result.stageConfigLoaded = stageLoaded;
    if (!stageLoaded)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[StageData] Failed to load stage config section '" << stageConfigSectionUsed
            << "' from '" << kGameplayConfigPath << "'. Aborting scene configuration.\n";
        DebugTrace(oss.str());
        return;
    }
    result.playerSpawnTransform.transform.position.z += result.stageData.GetPlaneZ();
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[PlayerTransform] Applied planeZ offset=" << result.stageData.GetPlaneZ()
            << " finalPosition=(" << result.playerSpawnTransform.transform.position.x << ','
            << result.playerSpawnTransform.transform.position.y << ','
            << result.playerSpawnTransform.transform.position.z << ")\n";
        DebugTrace(oss.str());
    }
    result.phaseManager.Initialize(
        result.stageData.GetPhaseThresholds(),
        result.stageData.GetPhaseRewardMultipliers(),
        result.stageData.GetPhaseMiniThresholds(),
        result.stageData.GetPhaseRewardMiniMultipliers());
    result.playerBaseHorizontalSpeed = result.stageData.GetPlayerHorizontalSpeedWorld();
    result.playerBaseVerticalSpeed = result.stageData.GetPlayerVerticalSpeedWorld();
    result.playerPickupBoxWidth = result.stageData.GetPickupBoxWidthWorld();
    result.playerPickupBoxHeight = result.stageData.GetPickupBoxHeightWorld();
    result.playerPickupOffset = result.stageData.GetPickupOffset();
    result.playerPickupMaxDistance = result.stageData.GetPickupMaxDistance();
    result.playerPickupDepthThreshold = result.stageData.GetPickupDepthThreshold();

    const auto sanitizePositive = [](float value, float fallback, const char* label)
    {
        if (std::isfinite(value) && value > 0.0f)
        {
            return value;
        }

        std::ostringstream oss;
        oss << kLogPrefix << label << " " << value
            << " invalid; using fallback " << fallback << "\n";
        DebugTrace(oss.str());
        return fallback;
    };

    const auto sanitizeDepthThreshold = [](float value, float fallback, const char* label)
    {
        if (std::isfinite(value))
        {
            if (value >= 0.0f)
            {
                return value;
            }
        }
        else if (std::isinf(value))
        {
            return value;
        }

        std::ostringstream oss;
        oss << kLogPrefix << label << " " << value
            << " invalid; using fallback " << fallback << "\n";
        DebugTrace(oss.str());
        return fallback;
    };

    result.playerBaseHorizontalSpeed = sanitizePositive(result.playerBaseHorizontalSpeed, kFallbackPlayerMoveSpeed, "Player horizontal speed");
    result.playerBaseVerticalSpeed = sanitizePositive(result.playerBaseVerticalSpeed, result.playerBaseHorizontalSpeed, "Player vertical speed");
    const float pickupFallback = std::max(result.stageData.GetCellSizeWorld(), 1.0f);
    result.playerPickupBoxWidth = sanitizePositive(result.playerPickupBoxWidth, pickupFallback, "Player pickup width");
    result.playerPickupBoxHeight = sanitizePositive(result.playerPickupBoxHeight, pickupFallback, "Player pickup height");
    result.playerPickupMaxDistance = sanitizePositive(result.playerPickupMaxDistance, kDefaultPickupMaxDistance, "Player pickup max distance");
    result.playerPickupDepthThreshold = sanitizeDepthThreshold(result.playerPickupDepthThreshold, kDefaultPickupDepthThreshold, "Player pickup depth threshold");
    result.playerSpeedMultiplier = 1.0f;

    {
        const auto pickupOffsetModeName = [](Stage::PickupOffsetMode mode)
        {
            switch (mode)
            {
            case Stage::PickupOffsetMode::None: return "none";
            case Stage::PickupOffsetMode::FacingDirection: return "facing-direction";
            case Stage::PickupOffsetMode::WorldX: return "world-x";
            default: return "unknown";
            }
        };
        std::ostringstream oss;
        oss << kLogPrefix << "Stage data " << (stageLoaded ? "loaded" : "defaulted")
            << " from '" << kGameplayConfigPath << "' section='" << stageConfigSectionUsed << "'"
            << " planeZ=" << result.stageData.GetPlaneZ()
            << " thresholds=" << result.stageData.GetPhaseThresholds().size()
            << " multipliers=" << result.stageData.GetPhaseRewardMultipliers().size()
            << " spawnPointsPrimary=" << result.stageData.GetSpawnPointsPrimary().size()
            << " spawnPointsSecondary=" << result.stageData.GetSpawnPointsSecondary().size()
            << " platforms=" << result.stageData.GetPlatformCount()
            << " playerSpeedXY=(" << result.playerBaseHorizontalSpeed << ',' << result.playerBaseVerticalSpeed << ')'
            << " pickupBox=(" << result.playerPickupBoxWidth << ',' << result.playerPickupBoxHeight << ')'
            << " pickupMaxDistance=" << result.playerPickupMaxDistance
            << " pickupDepthThreshold=" << result.playerPickupDepthThreshold
            << " pickupOffsetMode=" << pickupOffsetModeName(result.playerPickupOffset.mode)
            << " pickupOffsetLocalWorld=(" << result.playerPickupOffset.localOffset.x << ','
            << result.playerPickupOffset.localOffset.y << ',' << result.playerPickupOffset.localOffset.z << ')'
            << " guidelineHeight=" << result.stageData.GetGuidelineHeight()
            << " guidelineStep=" << result.stageData.GetGuidelineStep()
            << " guidelineThicknessPixels=" << result.stageData.GetGuidelineThicknessPixels()
            << " guidelineWidthPaddingWorld=" << result.stageData.GetGuidelineWidthPaddingWorld()
            << " guidelineZOffset=" << result.stageData.GetGuidelineZOffset()
            << " guidelineSpinAxis=(" << result.stageData.GetGuidelineSpinAxis().x << ','
            << result.stageData.GetGuidelineSpinAxis().y << ',' << result.stageData.GetGuidelineSpinAxis().z << ')'
            << " guidelineSpinSpeedDegPerSec=" << result.stageData.GetGuidelineSpinSpeedDegPerSec()
            << " guidelineModelPath='" << result.stageData.GetGuidelineModelPath() << "'"
            << " guidelineMaterialKey='" << result.stageData.GetGuidelineMaterialKey() << "'"
            << " heightBonusThreshold=" << result.stageData.GetHeightBonusThreshold()
            << " heightBonusSeconds=" << result.stageData.GetHeightBonusSeconds()
            << " background='" << result.stageData.GetBackgroundTexturePath() << "'\n";
        for (std::size_t i = 0; i < result.stageData.GetPlatformCount(); ++i)
        {
            const auto& descriptor = result.stageData.GetPlatform(i);
            oss << kLogPrefix << " PlatformDesc[" << i << "] center=(" << descriptor.center.x << ',' << descriptor.center.y
                << ',' << descriptor.center.z << ") scale=(" << descriptor.scale.x << ',' << descriptor.scale.y << ','
                << descriptor.scale.z << ") depth=" << descriptor.depth
                << " worldScale=(" << descriptor.widthWorld << ',' << descriptor.heightWorld << ',' << descriptor.depthWorld
                << ")"
                << " widthFraction=" << descriptor.widthFraction << "\n";
        }
        DebugTrace(oss.str());
    }

    auto& manifest = SweetsSpace::SweetsPrototypeManifest::Instance();
    const bool manifestLoaded = manifest.ReloadFromCsv();
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Manifest config " << (manifestLoaded ? "applied" : "missing; using defaults")
            << " path='" << SweetsSpace::SweetsPrototypeManifest::kDefaultConfigPath << "'\n";
        const auto prototypes = manifest.Enumerate();
        oss << kLogPrefix << " Manifest entries=" << prototypes.size() << "\n";
        for (const auto& entry : prototypes)
        {
            oss << kLogPrefix << "  Entry " << SweetsSpace::ToString(entry.type) << " key='"
                << entry.descriptor.key << "' model='" << entry.descriptor.modelPath << "'\n";
        }
        DebugTrace(oss.str());
    }

    Render::ModelCache& cache = Render::ModelCache::Instance();
    // シーンのアセット読み込み開始前に、モデルキャッシュの内容のみをクリアする
    // （ローダーやデバイスは維持して再設定漏れを防止）
    cache.ClearModels();

    const bool modelTexturesEnabled = SceneGameConfig::LoadModelTextureLoadingSetting();
    cache.SetTextureLoadingEnabled(modelTexturesEnabled);

    {
        std::ostringstream oss;
        oss << kLogPrefix << "Render settings: loadModelTextures=" << std::boolalpha << modelTexturesEnabled << "\n";
        DebugTrace(oss.str());
    }

    // AnimationCache は SceneGame で SetLoader 済みのため、ここでは触らない
}

void SceneComposer::ComposeRules(Composition& result)
{
    result.guidelineHeightMeters = result.stageData.GetGuidelineHeight();
    result.guidelineStepMeters = result.stageData.GetGuidelineStep();
    result.guidelineThicknessPixels = result.stageData.GetGuidelineThicknessPixels();
    result.guidelineWidthPaddingWorld = result.stageData.GetGuidelineWidthPaddingWorld();
    result.guidelineZOffset = result.stageData.GetGuidelineZOffset();
    result.guidelineSpinAxis = result.stageData.GetGuidelineSpinAxis();
    result.guidelineSpinSpeedDegPerSec = result.stageData.GetGuidelineSpinSpeedDegPerSec();
    result.guidelineCooldownDurationSeconds = kGuidelineAdvanceCooldownSeconds;
    result.guidelineCooldownRemainingSeconds = 0.0f;
    result.guidelineCenterX = 0.0f;
    result.guidelineWidthWorld = 0.0f;
    result.guidelineThicknessWorld = result.stageData.GetGuidelineThicknessWorld();
    if (!std::isfinite(result.guidelineThicknessWorld) || result.guidelineThicknessWorld <= 0.0f)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Guideline thickness world " << result.guidelineThicknessWorld
            << " invalid; using fallback " << kGuidelineThicknessMeters << "\n";
        DebugTrace(oss.str());
        result.guidelineThicknessWorld = kGuidelineThicknessMeters;
    }

    {
        bool boundsValid = false;
        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        for (std::size_t i = 0; i < result.stageData.GetPlatformCount(); ++i)
        {
            const auto& descriptor = result.stageData.GetPlatform(i);
            if (!std::isfinite(descriptor.center.x) || !std::isfinite(descriptor.widthWorld) || descriptor.widthWorld <= 0.0f)
            {
                continue;
            }

            const float halfWidth = descriptor.widthWorld * 0.5f;
            if (!std::isfinite(halfWidth) || halfWidth <= 0.0f)
            {
                continue;
            }

            minX = std::min(minX, descriptor.center.x - halfWidth);
            maxX = std::max(maxX, descriptor.center.x + halfWidth);
            boundsValid = true;
        }

        if (boundsValid)
        {
            result.guidelineCenterX = (minX + maxX) * 0.5f;
            result.guidelineWidthWorld = maxX - minX;
        }

        const float widthPadding = std::max(0.0f, result.guidelineWidthPaddingWorld);
        float paddedWidth = result.guidelineWidthWorld + (widthPadding * 2.0f);
        if (!std::isfinite(paddedWidth) || paddedWidth <= 0.0f)
        {
            paddedWidth = 0.01f;
        }
        result.guidelineWidthWorld = paddedWidth;
    }

    result.heightBonusThresholdMeters = result.stageData.GetHeightBonusThreshold();
    if (!std::isfinite(result.heightBonusThresholdMeters) || result.heightBonusThresholdMeters <= 0.0f)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Height bonus threshold " << result.heightBonusThresholdMeters
            << " invalid; timer bonus disabled.\n";
        DebugTrace(oss.str());
        result.heightBonusThresholdMeters = 0.0f;
    }

    result.heightBonusSeconds = result.stageData.GetHeightBonusSeconds();
    if (!std::isfinite(result.heightBonusSeconds))
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Height bonus seconds " << result.heightBonusSeconds
            << " invalid; timer bonus disabled.\n";
        DebugTrace(oss.str());
        result.heightBonusSeconds = 0.0f;
    }
    else if (result.heightBonusSeconds <= 0.0f)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Height bonus seconds <= 0; timer bonus disabled.\n";
        DebugTrace(oss.str());
        result.heightBonusSeconds = 0.0f;
    }
}

void SceneComposer::ComposePhysics(Composition& result)
{
    const SceneGameConfig::PhysicsTuningSettings tuning = SceneGameConfig::LoadPhysicsTuningSettings();
    Systems::PhysicsTuningParameters physicsParameters{};
    physicsParameters.gravity = tuning.gravity;
    physicsParameters.subStepCount = tuning.subStepCount;
    physicsParameters.defaultLinearDamping = tuning.linearDamping;
    physicsParameters.defaultAngularDamping = tuning.angularDamping;
    physicsParameters.defaultFriction = tuning.defaultFriction;
    physicsParameters.defaultRestitution = tuning.defaultRestitution;
    physicsParameters.restitutionThreshold = tuning.restitutionThreshold;
    physicsParameters.hitEventThreshold = tuning.hitEventThreshold;
    physicsParameters.maxStepDt = tuning.maxStepDt;
    physicsParameters.maxAccumulatedSteps = tuning.maxAccumulatedSteps;
    physicsParameters.continuousCollisionForSweets = tuning.continuousCollisionForSweets;
    physicsParameters.continuousCollisionForSmallDynamics = tuning.continuousCollisionForSmallDynamics;
    physicsParameters.smallDynamicContinuousThreshold = tuning.smallDynamicContinuousThreshold;
    physicsParameters.continuousCollisionForPlayer = tuning.continuousCollisionForPlayer;
    physicsParameters.creamGlueLinearHertz = tuning.creamGlueLinearHertz;
    physicsParameters.creamGlueLinearDampingRatio = tuning.creamGlueLinearDampingRatio;
    physicsParameters.creamGlueAngularHertz = tuning.creamGlueAngularHertz;
    physicsParameters.creamGlueAngularDampingRatio = tuning.creamGlueAngularDampingRatio;
    physicsParameters.creamGlueCollideConnected = tuning.creamGlueCollideConnected;
    physicsParameters.creamGluePushOutEpsilon = tuning.creamGluePushOutEpsilon;
    physicsParameters.creamGlueMaxPushOut = tuning.creamGlueMaxPushOut;
    physicsParameters.creamGlueMaxSpeed = tuning.creamGlueMaxSpeed;
    physicsParameters.creamGlueBreakForce = tuning.creamGlueBreakForce;
    physicsParameters.creamGlueBreakImpulse = tuning.creamGlueBreakImpulse;
    result.physicsBinder = std::make_unique<Systems::PhysicsBinder>(physicsParameters);
    Systems::PhysicsMachine* physicsMachine = result.physicsBinder ? result.physicsBinder->GetPhysicsMachine() : nullptr;
    result.drawSystem = std::make_unique<Systems::DrawSystem>();
    if (result.drawSystem)
    {
        result.drawSystem->SetStagePlaneZ(result.stageData.GetPlaneZ());
    }
    if (result.drawSystem && result.guidelineHeightMeters > 0.0f)
    {
        std::shared_ptr<Render::Model> guidelineModel;
        const std::string& guidelineModelPath = result.stageData.GetGuidelineModelPath();
        const std::string& guidelineMaterialKey = result.stageData.GetGuidelineMaterialKey();
        if (!guidelineModelPath.empty())
        {
            guidelineModel = Render::ModelCache::Instance().Get(guidelineModelPath);
            if (!guidelineModel)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Guideline model load failed path='" << guidelineModelPath
                    << "'; fallback quad will be used.\n";
                DebugTrace(oss.str());
            }
            else
            {
                if (!guidelineMaterialKey.empty())
                {
                    guidelineModel->SetMaterialKey(guidelineMaterialKey);
                }
                std::ostringstream oss;
                oss << kLogPrefix << "Guideline model loaded path='" << guidelineModelPath
                    << "' materialKey='" << guidelineMaterialKey << "'\n";
                DebugTrace(oss.str());
            }
        }
        else
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Guideline model path empty; fallback quad will be used.\n";
            DebugTrace(oss.str());
        }

        if (guidelineModel)
        {
            result.guidelineActor = std::make_unique<GuidelineActor>(guidelineModel);
        }
        else
        {
            result.guidelineActor = std::make_unique<GuidelineActor>();
            if (result.guidelineActor && !guidelineMaterialKey.empty())
            {
                const auto model = result.guidelineActor->GetModel();
                if (model)
                {
                    model->SetMaterialKey(guidelineMaterialKey);
                }
            }
        }
        if (result.guidelineActor)
        {
            const float guidelineZ = result.stageData.GetPlaneZ() + result.guidelineZOffset;
            result.guidelineActor->UpdateGuideline(
                result.guidelineCenterX,
                result.guidelineHeightMeters,
                guidelineZ,
                result.guidelineWidthWorld,
                result.guidelineThicknessWorld);
            result.drawSystem->RegisterRenderable(*result.guidelineActor);
        }
    }
    if (result.drawSystem && physicsMachine)
    {
        result.sweetsManager = std::make_unique<SweetsSpace::SweetsManager>(
            *result.drawSystem,
            *physicsMachine,
            m_scheduler);
    }

    if (result.sweetsManager)
    {
        result.sweetsManager->SetSpawnOrigins(
            result.stageData.GetSpawnPointsPrimary(),
            result.stageData.GetSpawnPointsSecondary());
        result.sweetsManager->SetLiveSweetsCap(result.sweetsSpawnLimitSettings.spawnTotalCap);
        result.sweetsManager->SetSpawnPointCap(result.sweetsSpawnLimitSettings.spawnPerPointCap);
        const bool spawnConfigLoaded = result.sweetsManager->LoadPhaseSpawnConfig(kGameplayConfigPath);
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Sweets spawn config " << (spawnConfigLoaded ? "loaded" : "defaulted")
                << " from '" << kGameplayConfigPath << "' section='" << GameplayConfig::kSectionSpawnPhases << "'.\n";
            DebugTrace(oss.str());
        }
        {
            const auto presets = result.sweetsManager->EnumerateFactoryPresets();
            const auto colliderShapeToString = [](SweetsSpace::ColliderShape shape)
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
            };
            std::ostringstream oss;
            oss << kLogPrefix << "[SpawnFactory] presets=" << presets.size() << " (key, scale, collider)\n";
            for (const auto& preset : presets)
            {
                oss << kLogPrefix << "  kind='" << preset.first << "' scale=("
                    << preset.second.scale.x << ','
                    << preset.second.scale.y << ','
                    << preset.second.scale.z << ") collider="
                    << colliderShapeToString(preset.second.colliderPreset.shape)
                    << " pad=(" << preset.second.colliderPreset.padding.x << ','
                    << preset.second.colliderPreset.padding.y << ','
                    << preset.second.colliderPreset.padding.z << ")"
                    << " spawnWeight=" << preset.second.spawnWeight << "\n";
            }
            DebugTrace(oss.str());
        }
    }
}

void SceneComposer::ComposeLayout(Composition& result)
{
    StageLayoutLoadRequest stageLayoutRequest{
        "Assets/Config/StageLayout.csv",
        kGameplayConfigPath,
        Render::ModelCache::Instance(),
        result.physicsBinder != nullptr,
        kLogPrefix
    };
    StageLayoutLoadResult stageLayoutResult = StageLayoutLoader::LoadFromCsv(stageLayoutRequest);
    result.stageObjects = std::move(stageLayoutResult.stageObjects);
    result.stagePhysicsEntities = std::move(stageLayoutResult.physicsEntities);
    if (result.physicsBinder)
    {
        for (const auto& entity : result.stagePhysicsEntities)
        {
            if (entity)
            {
                result.physicsBinder->RegisterPhysicsBody(*entity);
            }
        }
    }
}

void SceneComposer::ComposeUI(Composition& result)
{
    const std::string backgroundTexture = result.stageData.GetBackgroundTexturePath();
    if (!backgroundTexture.empty())
    {
        // scrollOffset is in world units (meters). See SceneGame::Draw background scroll logic.
        constexpr float kDefaultBackgroundParallax = 0.0001f;
        BackgroundRendererConfig backgroundConfig{};
        BackgroundRendererConfig::BackgroundLayer layer;
        layer.texturePath = backgroundTexture;
        layer.parallaxFactor = kDefaultBackgroundParallax;
        layer.scrollOffsetInPixels = false;
        layer.samplingMode = BackgroundRendererConfig::SamplingMode::Clamp;
        layer.baseOffset01 = 4.00f;
        layer.tintColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        backgroundConfig.layers.push_back(layer);
        result.backgroundRenderer = std::make_unique<BackgroundRenderer>();
        if (!result.backgroundRenderer->Initialize(backgroundConfig))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Background renderer failed for texture '" << backgroundTexture << "'.\n";
            DebugTrace(oss.str());
            result.backgroundRenderer.reset();
        }
    }
    else
    {
        DebugTrace(std::string(kLogPrefix) + "Background texture path empty; background renderer skipped.\n");
    }

    result.uiManager = std::make_unique<UIManager>();
    if (result.uiManager && !result.uiManager->Initialize(kHudLayoutConfig, kResultLayoutConfig))
    {
        DebugTrace(std::string(kLogPrefix) + "UI manager failed to initialize; HUD/Result overlay disabled.\n");
        result.uiManager.reset();
    }
}
