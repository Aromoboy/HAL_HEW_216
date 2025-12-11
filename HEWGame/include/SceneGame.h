#ifndef __SCENE_GAME_H__
#define __SCENE_GAME_H__

#include "Scene.h"
#include "SceneManager.h"
#include "SoundManagerEX.h"
#include "CameraRig.h"
#include "Entities/PhysicalEntity.h"
#include "Entities/Player.h"
#include "IModelLoader.h"
#include "ModelRender.h"
#include "PerspectiveCamera.h"
#include "Core/StepTimer.h"
#include "Systems/PhysicsMachine.h"
#include "Vector.h"
#include "Transform.h"
#include "Stage/StageData.h"
#include "Stage/FoundationPlatform.h"
#include "GameFlow/PhaseManager.h"
#include "SweetsManager.h"
#include "HudRenderer.h"
#include "TimerDisplay.h"
#include "BackgroundRenderer.h"
#include "ColliderConfigLoader.h"
#include <cstddef>
#include <memory>
#include <map>
#include <unordered_set>
#include <vector>
#include <limits>
#include <deque>
#include <string>

struct CameraPrototypeSettings
{
    bool fixedPosition = false;
    float followSpeed = 5.0f;
    float3 offset{ 0.0f, 0.0f, 0.0f };
    float fovDegrees = 60.0f;
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
};

struct StageObject
{
    std::string modelPath;
    Transform transform;
    bool spinEnabled;
    float3 spinAxis;
    float spinSpeed;
    std::shared_ptr<Render::Model> model;
    float currentRotation;
};

struct StageAnchor
{
    Transform transform{};
    std::shared_ptr<Render::Model> model;
};

namespace Systems
{
    class PhysicsMachine;
    class Box2DContactListener;
    class DrawSystem;
}

namespace SweetsSpace
{
    class SweetsManager;
}

class SceneGame : public SceneSpace::Scene, public SceneSpace::IScene
{
public:
    enum class StageState
    {
        Intro,
        Playing,
        TimeUp,
        Cleared,
        ShowingResult,
    };

    struct PlayerColliderTuning
    {
        float thicknessPadding = 1.0f;
        float3 padding{ 0.0f, 0.0f, 0.0f };
        float3 offset{ 0.0f, 0.0f, 0.0f };
        float3 fallbackPadding{ 0.0f, 0.0f, 0.0f };
        float3 fallbackOffset{ 0.0f, 0.0f, 0.0f };
        float3 fallbackSize{ 1.0f, 1.0f, 1.0f };
    };

    struct PlayerSpawnTransform
    {
        Transform transform{ float3{ 0.0f, 0.0f, 0.0f }, float3{ 0.0f, 0.0f, 0.0f }, float3{ 1.0f, 1.0f, 1.0f } };
        bool loadedFromFile = false;
        bool rotationFromDegrees = false;
    };

    SceneGame();
    ~SceneGame();
    void Load(Core::Input& input) override;
    void Update(Core::StepTimer& timer, Core::Input& input) override;
    void Draw() override;
    void OnEnter() override;
    void OnExit() override;
    void ConfigureUpdates() override;
    void Update(Core::UpdateContext& ctx) final;
    void Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera) final;
private:
    void ConfigureCamera(const CameraPrototypeSettings& settings);
    void RefreshPlatformAnchor(bool forceRecompute);
#if defined(_DEBUG)
    void HandleColliderDebugCommands(Core::UpdateContext& ctx);
    void DrawPhysicsDebugOverlay();
#endif
    void SetStageState(StageState newState);
    void OnStageStart();
    void OnStageClear();
    void OnStageFail();
    void RegisterPhysicsBody(Entities::PhysicalEntity& entity);
    void UnregisterPhysicsBody(Entities::PhysicalEntity* entity);
    void UnregisterAllPhysicsBodies();
    void ResetPlaySession();
    void UpdateCountdown(Core::UpdateContext& ctx);
    void UpdateTowerHeightTelemetry();
    void ProcessPostGameInput(Core::UpdateContext& ctx);
    void HandleManualRetryInput(Core::UpdateContext& ctx);
    void ToggleGameplaySystems(bool enabled);
    bool TryGetGuidelineClipY(const ::CameraSpace::ICamera& camera, float& outClipY) const;
    bool LoadStageLayout(const std::string& csvPath);
    void ValidatePlayerPhysicsRegistration();
    int ComputeScore() const;
    void NormalizePlayerColliderDefinitions();
    std::string ResolvePlayerColliderKey(const std::string& key) const;
    std::vector<StageObject> m_stageObjects;
    std::shared_ptr<CameraSpace::PerspectiveCamera> m_gameCamera;
    std::unique_ptr<CameraRig> m_cameraRig;
    std::shared_ptr<Entities::Entity> m_cameraAnchor;
    std::unique_ptr<SoundManagerEX> m_pManager;
    std::shared_ptr<IModelLoader> m_loader;
    std::unique_ptr<ModelRender> m_render;
    std::unique_ptr<Entities::Player> player;
    std::vector<StageAnchor> m_stageAnchors;
    std::unique_ptr<Systems::PhysicsMachine> m_physicsMachine;
    std::shared_ptr<Systems::Box2DContactListener> m_contactListener;
    std::unique_ptr<Systems::DrawSystem> m_drawSystem;
    std::unique_ptr<SweetsSpace::SweetsManager> m_sweetsManager;
    std::vector<std::unique_ptr<Stage::FoundationPlatform>> m_foundationPlatforms;
    std::vector<bool> m_platformUpdateRegistered;
    std::vector<bool> m_platformDrawRegistered;
    Stage::FoundationPlatform* m_platePlatform = nullptr;
    std::unordered_set<Entities::PhysicalEntity*> m_registeredPhysicsBodies;
    bool m_loggedPlayerBodyRecovery = false;
    bool m_activeCameraMissingLogged = false;
    bool m_inputRegistered = false;
    bool m_physicsRegistered = false;
    bool m_cameraRigRegistered = false;
    bool m_drawRegistered = false;
    bool m_sweetsRegistered = false;
    bool m_sweetsLateRegistered = false;
    Stage::StageData m_stageData;
    GameFlow::PhaseManager m_phaseManager;
    void InitializeStageAnchors();
    void BroadcastPhaseAdvance(int newPhase);
    void ForwardSweetsPhaseChange(int newPhase);
    Core::StepTimer m_countdownTimer;
    StageState m_stageState = StageState::Intro;
    float m_totalPlaySeconds = 0.0f;
    float m_remainingCountdownSeconds = 0.0f;
    int m_lastAnnouncedCountdownSeconds = 0;
    float m_currentTowerHeight = 0.0f;
    float m_peakTowerHeight = 0.0f;
    float m_maxTowerHeight = 0.0f;
    mutable int m_lastScoreLogValue = std::numeric_limits<int>::min();
    mutable float m_lastScoreLogMultiplier = std::numeric_limits<float>::quiet_NaN();
    std::unique_ptr<HudRenderer> m_hudRenderer;
    std::unique_ptr<BackgroundRenderer> m_backgroundRenderer;
    float m_guidelineHeightMeters = 0.0f;
    float m_heightBonusThresholdMeters = 0.0f;
    float m_heightBonusSeconds = 0.0f;
    bool m_heightBonusArmed = false;
    float m_playerBaseHorizontalSpeed = 0.0f;
    float m_playerBaseVerticalSpeed = 0.0f;
    float m_playerPickupBoxWidth = 0.0f;
    float m_playerPickupBoxHeight = 0.0f;
    float m_playerPickupForwardOffset = 0.0f;
    float m_playerSpeedMultiplier = 1.0f;
    struct PendingTimeBonus
    {
        float seconds = 0.0f;
        float triggerHeight = 0.0f;
    };
    std::deque<PendingTimeBonus> m_pendingTimeBonuses;
    bool m_platformAnchorInitialized = false;
    mutable bool m_guidelineInvalidLogged = false;
    mutable bool m_guidelineClampLogged = false;
    float m_cachedPlayerMinX = std::numeric_limits<float>::quiet_NaN();
    float m_cachedPlayerMaxX = std::numeric_limits<float>::quiet_NaN();
    float m_cachedPlayerMaxY = std::numeric_limits<float>::quiet_NaN();
    bool m_cachedPlayerBoundsValid = false;
    bool m_playerBoundsFailureLogged = false;
    bool m_playerColliderMismatchLogged = false;
    std::shared_ptr<Render::Model> m_playerModel;
    std::map<std::string, Collision::ColliderDefinition> m_playerColliderDefinitions;
    bool m_playerColliderConfigLoaded = false;
    bool m_forceConfigPlayerCollider = false;
    PlayerColliderTuning m_playerColliderTuning;
    PlayerSpawnTransform m_playerSpawnTransform;
    TimerDisplay m_timerDisplay;
    std::shared_ptr<Texture2D> m_numberTexture;
};

#endif // __SCENE_GAME_H__
