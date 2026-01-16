#pragma once

#include <memory>
#include <string>
#include <vector>

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
        SceneGameConfig::PlayerMovementSettings playerMovementSettings{};
        Stage::StageData stageData{};
        GameFlow::PhaseManager phaseManager{};
        float playerBaseHorizontalSpeed = 0.0f;
        float playerBaseVerticalSpeed = 0.0f;
        float playerSpeedMultiplier = 1.0f;
        SceneGameConfig::RenderPostProcessSettings renderPostProcessSettings{};
        SceneGameConfig::LightingSettings lightingSettings{};
        SceneGameConfig::BlobShadowSettings blobShadowSettings{};
        std::unique_ptr<Systems::PhysicsBinder> physicsBinder;
        std::unique_ptr<Systems::DrawSystem> drawSystem;
        std::unique_ptr<SweetsSpace::SweetsManager> sweetsManager;
        std::unique_ptr<UIManager> uiManager;
        std::unique_ptr<BackgroundRenderer> backgroundRenderer;
        std::vector<StageObject> stageObjects;
        std::vector<std::shared_ptr<Entities::PhysicalEntity>> stagePhysicsEntities;
    };

    explicit SceneComposer(Core::UpdateScheduler& scheduler);
    Composition ComposeBootstrap();
    void Compose(Composition& result);

private:
    void ComposeConfig(Composition& result);
    void ComposeRules(Composition& result);
    void ComposePhysics(Composition& result);
    void ComposeLayout(Composition& result);
    void ComposeUI(Composition& result);

    Core::UpdateScheduler& m_scheduler;
};
