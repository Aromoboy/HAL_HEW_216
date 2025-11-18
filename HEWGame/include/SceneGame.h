#ifndef __SCENE_GAME_H__
#define __SCENE_GAME_H__

#include "Scene.h"
#include "Model.h"
#include "TestObject.h"
#include "SoundManagerEX.h"
#include "CameraRig.h"
#include "Entity.h"
#include "IModelLoader.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "PerspectiveCamera.h"
#include "Player.h"
#include "Core/StepTimer.h"
#include "Systems/PhysicsMachine.h"
#include "Vector.h"
#include "Stage/StageData.h"
#include "Stage/FoundationPlatform.h"
#include "GameFlow/PhaseManager.h"
#include "SweetsManager.h"
#include "HudRenderer.h"
#include "BackgroundRenderer.h"
#include <cstddef>
#include <memory>
#include <unordered_set>
#include <vector>
#include <limits>

struct CameraPrototypeSettings
{
    bool fixedPosition = false;
    float followSpeed = 5.0f;
    float3 offset{ 0.0f, 0.0f, 0.0f };
    float fovDegrees = 60.0f;
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
};

namespace Systems
{
    class PhysicsMachine;
    class GravityDampingSystem;
    class ContactLiteSystem;
    class DrawSystem;
    class IntegrateSystem;
}

namespace SweetsSpace
{
    class SweetsManager;
}

class SceneGame : public SceneSpace::Scene
{
public:
    SceneGame();
    ~SceneGame();
    void ConfigureUpdates() override;
    void Update(Core::UpdateContext& ctx) final;
    void Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera) final;
private:
    void ConfigureCamera(const CameraPrototypeSettings& settings);
    void RefreshPlatformAnchor(bool forceRecompute);
#if defined(_DEBUG)
    void HandleColliderDebugCommands(Core::UpdateContext& ctx);
#endif
    void RegisterPhysicsBody(SceneSpace::PhysicalEntity& entity);
    void UnregisterPhysicsBody(SceneSpace::PhysicalEntity* entity);
    void UnregisterAllPhysicsBodies();
    void ResetPlaySession();
    void UpdateCountdown(Core::UpdateContext& ctx);
    void UpdateTowerHeightTelemetry();
    void ProcessPostGameInput(Core::UpdateContext& ctx);
    void HandleManualRetryInput(Core::UpdateContext& ctx);
    void ToggleGameplaySystems(bool enabled);
    bool TryGetGuidelineClipY(const CameraSpace::ICamera& camera, float& outClipY) const;
#if defined(_DEBUG)
    void RenderRunTelemetryOverlay() const;
#endif
    int ComputeScore() const;
    std::unique_ptr<Model> m_pModel;
    std::shared_ptr<CameraSpace::PerspectiveCamera> m_gameCamera;
    std::unique_ptr<CameraRig> m_cameraRig;
    std::shared_ptr<SceneSpace::Entity> m_cameraAnchor;
    std::unique_ptr<SoundManagerEX> m_pManager;
    std::shared_ptr<IModelLoader> m_loader;
    std::shared_ptr<ModelDX> m_model;
    std::unique_ptr<ModelRender> m_render;
    std::unique_ptr<SceneSpace::Player> player;
    std::vector<std::shared_ptr<TestObject>> m_pObjects;
    std::unique_ptr<Systems::PhysicsMachine> m_physicsMachine;
    std::shared_ptr<Systems::IntegrateSystem> m_integrateSystem;
    std::shared_ptr<Systems::GravityDampingSystem> m_gravitySystem;
    std::unique_ptr<Systems::ContactLiteSystem> m_contactSystem;
    std::unique_ptr<Systems::DrawSystem> m_drawSystem;
    std::unique_ptr<SweetsSpace::SweetsManager> m_sweetsManager;
    std::unique_ptr<Stage::FoundationPlatform> m_stagePlatform;
    std::unordered_set<SceneSpace::PhysicalEntity*> m_registeredPhysicsBodies;
    bool m_inputRegistered = false;
    bool m_physicsRegistered = false;
    bool m_contactRegistered = false;
    bool m_cameraRigRegistered = false;
    bool m_drawRegistered = false;
    bool m_sweetsRegistered = false;
    bool m_stagePlatformRegistered = false;
    bool m_stagePlatformDrawRegistered = false;
    Stage::StageData m_stageData;
    GameFlow::PhaseManager m_phaseManager;
    void InitializeStageAnchors();
    void BroadcastPhaseAdvance(int newPhase);
    void ForwardSweetsPhaseChange(int newPhase);
    Core::StepTimer m_countdownTimer;
    float m_totalPlaySeconds = 0.0f;
    float m_remainingCountdownSeconds = 0.0f;
    int m_lastAnnouncedCountdownSeconds = 0;
    bool m_isPlaying = false;
    bool m_timeUp = false;
    float m_currentTowerHeight = 0.0f;
    float m_peakTowerHeight = 0.0f;
    float m_maxTowerHeight = 0.0f;
    mutable int m_lastScoreLogValue = std::numeric_limits<int>::min();
    mutable float m_lastScoreLogMultiplier = std::numeric_limits<float>::quiet_NaN();
    std::unique_ptr<HudRenderer> m_hudRenderer;
    std::unique_ptr<BackgroundRenderer> m_backgroundRenderer;
    float m_guidelineHeightMeters = 0.0f;
    float m_cachedViewportWidth = std::numeric_limits<float>::quiet_NaN();
    float m_cachedViewportHeight = std::numeric_limits<float>::quiet_NaN();
    float3 m_cachedCameraPosition{ std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN() };
    float3 m_cachedCameraRotation{ std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN() };
    float m_cachedCameraFovDegrees = std::numeric_limits<float>::quiet_NaN();
    float m_cachedCameraAspect = std::numeric_limits<float>::quiet_NaN();
    float3 m_cachedCameraRigOffset{ std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN() };
    bool m_platformAnchorInitialized = false;
    mutable bool m_guidelineInvalidLogged = false;
    mutable bool m_guidelineClampLogged = false;
};

#endif // __SCENE_GAME_H__
