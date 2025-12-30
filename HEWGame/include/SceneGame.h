#ifndef __SCENE_GAME_H__
#define __SCENE_GAME_H__

#include "Scene.h"
#include "SceneManager.h"
#include "Model.h"
#include "SoundManagerEX.h"
#include "CameraRig.h"
#include "Entities/PhysicalEntity.h"
#include "Entities/Player.h"
#include "GuidelineActor.h"
#include "IModelLoader.h"
#include "ModelRender.h"
#include "PerspectiveCamera.h"
#include "Core/StepTimer.h"
#include "Systems/PhysicsMachine.h"
#include "Vector.h"
#include "Transform.h"
#include "Stage/StageData.h"
#include "GameFlow/PhaseManager.h"
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
#include"SettingScene.h"
#include "ThirdParty/Effekseer/Effekseer.h"
#include "ResultRenderer.h"


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
    bool spinEnabled = false;
    float3 spinAxis{ 0.0f, 0.0f, 0.0f };
    float spinSpeed = 0.0f;
    std::shared_ptr<ModelDX> model;
    float currentRotation = 0.0f;
    std::string colliderPresetId;
    std::shared_ptr<Entities::PhysicalEntity> physicsEntity;
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

    SceneGame(SceneSpace::SceneManager& a_manager);
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
    bool CheckGuidelineCrossing(float& outTriggerHeight);
    void OnGuidelineCrossed();
    void ApplyGuidelineBonus(float triggerHeight);
    void HandleGuidelineAdvance(float triggerHeight);
    void ResetGuidelineHeight();
    void ProcessPostGameInput(Core::UpdateContext& ctx);
    void HandleManualRetryInput(Core::UpdateContext& ctx);
    void ToggleGameplaySystems(bool enabled);
    bool LoadStageLayout(const std::string& csvPath);
    void ValidatePlayerPhysicsRegistration();
    int ComputeScore() const;
    int GetSweetsCount() const;
    void NormalizePlayerColliderDefinitions();
    std::string ResolvePlayerColliderKey(const std::string& key) const;  
    void Initialize(const Stage::StageData& stageData);
    
    std::vector<StageObject> m_stageObjects;
    std::unique_ptr<Model> m_pModel;
    std::shared_ptr<CameraSpace::PerspectiveCamera> m_gameCamera;
    std::unique_ptr<CameraRig> m_cameraRig;
    std::shared_ptr<Entities::Entity> m_cameraAnchor;
    std::unique_ptr<SoundManagerEX> m_pManager;
    std::shared_ptr<IModelLoader> m_loader;
    std::unique_ptr<ModelRender> m_render;
    std::unique_ptr<Entities::Player> player;
    std::unique_ptr<Systems::PhysicsMachine> m_physicsMachine;
    std::shared_ptr<Systems::Box2DContactListener> m_contactListener;
    std::unique_ptr<Systems::DrawSystem> m_drawSystem;
    std::unique_ptr<GuidelineActor> m_guidelineActor;
    std::unique_ptr<SweetsSpace::SweetsManager> m_sweetsManager;
    std::unique_ptr<ResultRenderer> m_resultRenderer;
    std::unordered_set<Entities::PhysicalEntity*> m_registeredPhysicsBodies;
    bool m_loggedPlayerBodyRecovery = false;
    bool m_inputRegistered = false;
    bool m_physicsRegistered = false;
    bool m_cameraRigRegistered = false;
    bool m_drawRegistered = false;
    bool m_sweetsRegistered = false;
    bool m_sweetsLateRegistered = false;
    Stage::StageData m_stageData;
    GameFlow::PhaseManager m_phaseManager;
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
    float m_guidelineStepMeters = 0.0f;
    float m_guidelineThicknessPixels = 0.0f;
    float m_guidelineWidthPaddingWorld = 0.0f;
    float m_guidelineZOffset = 0.0f;
    float m_guidelineCenterX = 0.0f;
    float m_guidelineWidthWorld = 0.0f;
    float m_guidelineThicknessWorld = 0.0f;
    float m_guidelineCooldownDurationSeconds = 0.0f;
    float m_guidelineCooldownRemainingSeconds = 0.0f;
    float m_heightBonusThresholdMeters = 0.0f;
    float m_heightBonusSeconds = 0.0f;
    float m_groundY = 0.0f;
    float m_guidelineTriggerCooldownSeconds = 0.0f;
    float m_playerBaseHorizontalSpeed = 0.0f;
    float m_playerBaseVerticalSpeed = 0.0f;
    float m_playerPickupBoxWidth = 0.0f;
    float m_playerPickupBoxHeight = 0.0f;
    Stage::PickupOffset m_playerPickupOffset{};
    float m_playerPickupMaxDistance = 0.0f;
    float m_playerPickupDepthThreshold = std::numeric_limits<float>::infinity();
    float3 m_playerPickupOriginOffset{ 0.0f, 0.0f, 0.0f };
    float m_playerSpeedMultiplier = 1.0f;
    struct PendingTimeBonus
    {
        float seconds = 0.0f;
        float triggerHeight = 0.0f;
    };
    std::deque<PendingTimeBonus> m_pendingTimeBonuses;
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
    // クリアできたかどうかの判定
    bool m_isGameClear = false;
    int m_phaseUpCount = 0;
    // 仮で作った何回フェーズを超えたらクリアになるかの判定
    int m_clearPhaseCount = 10;
    PlayerColliderTuning m_playerColliderTuning;
    PlayerSpawnTransform m_playerSpawnTransform;
    TimerDisplay m_timerDisplay;
    Texture* m_numberTexture;
    std::unordered_set<SweetsSpace::ActiveSweets*> m_guidelineTriggeredThisHeight;


    SceneSpace::SceneManager* m_manager = nullptr;
};

#endif // __SCENE_GAME_H__
