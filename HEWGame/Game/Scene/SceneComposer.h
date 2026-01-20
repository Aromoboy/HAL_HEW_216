#pragma once

#include <memory>
#include <string>
#include <vector>

#include "GuidelineActor.h"
#include "SceneGame.h"
#include "StageLayoutLoader.h"

namespace Core
{
    class UpdateScheduler;
}

class UIManager;

namespace Systems
{
    class PhysicsBinder;
}

class SceneComposer
{
public:
    struct Composition
    {
        bool stageConfigLoaded = false;
        SceneGame::PlayerSpawnTransform playerSpawnTransform{};
        Stage::StageData stageData{};
        GameFlow::PhaseManager phaseManager{};
        float playerBaseHorizontalSpeed = 0.0f;
        float playerBaseVerticalSpeed = 0.0f;
        float playerPickupBoxWidth = 0.0f;
        float playerPickupBoxHeight = 0.0f;
        Stage::PickupOffset playerPickupOffset{};
        float playerPickupMaxDistance = 0.0f;
        float playerPickupDepthThreshold = 0.0f;
        float playerSpeedMultiplier = 1.0f;
        float guidelineHeightMeters = 0.0f;
        float guidelineStepMeters = 0.0f;
        float guidelineThicknessPixels = 0.0f;
        float guidelineWidthPaddingWorld = 0.0f;
        float guidelineZOffset = 0.0f;
        float3 guidelineSpinAxis{ 0.0f, 0.0f, 0.0f };
        float guidelineSpinSpeedDegPerSec = 0.0f;
        float guidelineCooldownDurationSeconds = 0.0f;
        float guidelineCooldownRemainingSeconds = 0.0f;
        float guidelineCenterX = 0.0f;
        float guidelineWidthWorld = 0.0f;
        float guidelineThicknessWorld = 0.0f;
        SceneGameConfig::RenderPostProcessSettings renderPostProcessSettings{};
        SceneGameConfig::LightingSettings lightingSettings{};
        SceneGameConfig::BlobShadowSettings blobShadowSettings{};
        SceneGameConfig::MaterialTuningSettings materialTuningSettings{};
        SceneGameConfig::SweetsOutOfBoundsSettings sweetsOutOfBoundsSettings{};
        SceneGameConfig::SweetsSpawnLimitSettings sweetsSpawnLimitSettings{};
        std::unique_ptr<Systems::PhysicsBinder> physicsBinder;
        std::unique_ptr<Systems::DrawSystem> drawSystem;
        std::unique_ptr<GuidelineActor> guidelineActor;
        std::unique_ptr<SweetsSpace::SweetsManager> sweetsManager;
        std::unique_ptr<UIManager> uiManager;
        std::unique_ptr<BackgroundRenderer> backgroundRenderer;
        std::unique_ptr<EffectSystem> effectSystem;
        std::vector<StageObject> stageObjects;
        std::vector<std::shared_ptr<Entities::PhysicalEntity>> stagePhysicsEntities;
    };

    explicit SceneComposer(Core::UpdateScheduler& scheduler);
    Composition ComposeBootstrap();
    void ComposeEffect(Composition& result, ::EffekseerRendererDX11::RendererRef a_render, ::Effekseer::ManagerRef manager);
    void Compose(Composition& result);

private:
    void ComposeConfig(Composition& result);
    void ComposeRules(Composition& result);
    void ComposePhysics(Composition& result);
    void ComposeLayout(Composition& result);
    void ComposeUI(Composition& result);


    Core::UpdateScheduler& m_scheduler;
};
