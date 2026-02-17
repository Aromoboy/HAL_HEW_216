// ゲーム本編のプレイ中シーンをまとめたファイルだ
// ステージ設定を読み込んでプレイヤーやスイーツやカメラを動かす流れを組み立てる
// 進行状態や結果は画面表示や強化選択に渡される
#ifndef __SCENE_GAME_H__
#define __SCENE_GAME_H__

#include "Scene.h"
#include "SceneManager.h"
#include "Render/Legacy/Model.h"
#include "CameraRig.h"
#include "Entities/PhysicalEntity.h"
#include "Entities/Player.h"
#include "GameplayRulesSystem.h"
#include "PlayerController.h"
#include "SceneGameContactHandler.h"
#include "ModelRender.h"
#include "PerspectiveCamera.h"
#include "Core/StepTimer.h"
#include "Systems/Physics/IPhysicsBinder.h"
#include "Vector.h"
#include "Transform.h"
#include "Stage/StageData.h"
#include "GameFlow/PhaseManager.h"
#include "BackgroundRenderer.h"
#include "StageLayoutLoader.h"
#include "SceneGameConfigTypes.h"
#include "BlobShadowRenderer.h"
#include "ISceneServices.h"
#include "Meteors/MeteorsTypes.h"
#include "EffectSystem.h"
#include <box2d/types.h>
#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include <string>
#include"SettingScene.h"
#include <Effekseer.h>
#include <EffekseerRendererDX11.h>

namespace Systems
{
    class DrawSystem;
}

namespace Core
{
    class UpdateNode;
    struct UpdateContext;
}

namespace MeteorsSpace
{
    class MeteorsManager;
}

class SceneGameUIOverlay;
class UIManager;

class SceneGame : public SceneSpace::Scene, public SceneSpace::IScene, public SceneSpace::ISceneServices
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

    using PlayerColliderTuning = SceneGameConfig::PlayerColliderTuning;
    using PlayerSpawnTransform = SceneGameConfig::PlayerSpawnTransform;
    using PlayerMovementSettings = SceneGameConfig::PlayerMovementSettings;
    using RenderPostProcessSettings = SceneGameConfig::RenderPostProcessSettings;
    using SceneLightingSettings = SceneGameConfig::LightingSettings;
    using BlobShadowSettings = SceneGameConfig::BlobShadowSettings;
    using CameraRigSettings = SceneGameConfig::CameraRigSettings;
    using GameCameraConfig = SceneGameConfig::GameCameraConfig;

    SceneGame(SceneSpace::SceneManager& a_manager);
    ~SceneGame();
    bool Load(Core::Input& input) override;
    void Update(Core::StepTimer& timer, Core::Input& input) override;
    void Draw() override;
    void OnEnter() override;
    void OnExit() override;
    void ConfigureUpdates() override;
    void Update(const Core::UpdateContext& ctx) final;
    void DrawWorld(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera) override;
    void Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera) final;
    void DrawOverlay() override;
    Systems::IPhysicsBinder* GetPhysicsBinder() override;
    Render::ModelRenderer* GetRenderer() override;
    Core::UpdateScheduler& GetScheduler() override;
    Core::Input* GetInput() override;
    Core::StepTimer* GetTimer() override;

private:
    struct MissileInstance
    {
        std::shared_ptr<Entities::PhysicalEntity> entity;
        float remainingSeconds = 0.0f;
        float ignorePlayerSeconds = 3.0f;
        std::vector<b2Filter> originalFilters;
        bool playerFilterIgnored = false;
    };

    void ConfigureCamera(const CameraRigSettings& settings, const GameCameraConfig& gameCameraConfig);
    void BeginCameraIntro();
    void UpdateCameraIntro(float dt);
    void UpdateCameraBoundsForPhase(float heightMeters);
    void ApplyPlayerWrapDeltaY(float wrapDeltaY);
#if defined(_DEBUG)
    void HandleColliderDebugCommands(const Core::UpdateContext& ctx);
    void DrawPhysicsDebugOverlay(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera);
#endif
    void FireMissile();
    void UpdateMissiles(float deltaSeconds);
    void ClearMissiles();
    void SetStageState(StageState newState);
    void OnStageStart();
    void OnStageClear();
    void OnStageFail();
    void ResetPlaySession();
    void ToggleGameplaySystems(bool enabled);
    int GetMeteorsCount() const;
    void Initialize(const Stage::StageData& stageData);
    void UnregisterMissileInstance(MissileInstance& instance);
    void RemoveMissileByEntity(const std::shared_ptr<Entities::PhysicalEntity>& missile);
    void ProcessPendingMissileRemovals();
    void ProcessPendingMeteorRemovals();
    std::shared_ptr<Entities::PhysicalEntity> FindMissileByEntity(Entities::PhysicalEntity* missile) const;
    void InitializeEffectSystem();
    void ShutdownEffectSystem();

protected:
    std::vector<StageObject> m_stageObjects;
    std::vector<std::shared_ptr<Entities::PhysicalEntity>> m_stagePhysicsEntities;
    std::shared_ptr<CameraSpace::PerspectiveCamera> m_gameCamera;
    std::unique_ptr<CameraRig> m_cameraRig;
    Render::ModelRenderer* m_renderer = nullptr;
    std::unique_ptr<Entities::Player> player;
    int m_playerHp = 10;
    float m_playerInvulnSeconds = 0.0f;
    float m_playerBlinkTimer = 0.0f;
    float m_playerBlinkAlpha = 1.0f;
    bool m_playerInvulnerable = false;
    std::unique_ptr<Systems::IPhysicsBinder> m_physicsBinder;
    std::unique_ptr<Systems::DrawSystem> m_drawSystem;
    std::unique_ptr<MeteorsSpace::MeteorsManager> m_meteorsManager;
    SceneGameContactHandler m_contactHandler;
    PlayerController m_playerController;
    bool m_inputRegistered = false;
    bool m_physicsRegistered = false;
    bool m_contactDispatchRegistered = false;
    bool m_cameraRigRegistered = false;
    bool m_drawRegistered = false;
    bool m_meteorsRegistered = false;
    bool m_meteorsLateRegistered = false;
    bool m_effectRegistered = false;
    bool m_loadFailed = false;
    Stage::StageData m_stageData;
    GameFlow::PhaseManager m_phaseManager;
    GameplayRulesSystem m_gameplayRules;
    void BroadcastPhaseAdvance(int newPhase);
    void ForwardMeteorsPhaseChange(int newPhase);
    StageState m_stageState = StageState::Intro;
    std::unique_ptr<SceneGameUIOverlay> m_uiOverlay;
    std::unique_ptr<UIManager> m_uiManager;
    std::unique_ptr<BackgroundRenderer> m_backgroundRenderer;
    std::unique_ptr<BlobShadowRenderer> m_blobShadowRenderer;
    std::unique_ptr<EffectSystem> m_effectSystem;
    ::Effekseer::Backend::GraphicsDeviceRef m_effectDevice;
    ::EffekseerRendererDX11::RendererRef m_effectRenderer;
    ::Effekseer::ManagerRef m_effectManager;
    std::unordered_map<std::string, EffectCreateData> m_effectMap;
    std::optional<float> m_cameraWrapMinY = std::nullopt;
    std::optional<float> m_cameraWrapMaxY = std::nullopt;
    float m_heightBonusThresholdMeters = 0.0f;
    float m_heightBonusSeconds = 0.0f;
    RenderPostProcessSettings m_renderPostProcessSettings{};
    SceneLightingSettings m_lightingSettings{};
    BlobShadowSettings m_blobShadowSettings{};
    GameCameraConfig m_gameCameraConfig{};
    CameraRigSettings m_cameraRigSettings{};
    std::optional<float> m_stagePlaneZ = std::nullopt;
    float m_playerBaseHorizontalSpeed = 0.0f;
    float m_playerBaseVerticalSpeed = 0.0f;
    float m_playerSpeedMultiplier = 1.0f;
    PlayerMovementSettings m_playerMovementSettings{};
    Core::Input* m_inputService = nullptr;
    bool m_playerColliderMismatchLogged = false;
    std::shared_ptr<Render::Model> m_playerModel;
    std::shared_ptr<Render::Model> m_missileModel;
    SceneGameConfig::MissileSettings m_missileSettings{};
    float m_missileFireCooldownSeconds = 0.0f;
    bool m_fireMissileHintEnabled = false;
    std::vector<MissileInstance> m_activeMissiles;
    std::vector<std::weak_ptr<Entities::PhysicalEntity>> m_pendingMissileRemovals;
    std::vector<std::weak_ptr<MeteorsSpace::ActiveMeteors>> m_pendingMeteorRemovals;
    // クリアできたかどうかの判定
    bool m_isGameClear = false;
    int m_phaseUpCount = 0;
    // 仮で作った何回フェーズを超えたらクリアになるかの判定
    int m_clearPhaseCount = 10;
    PlayerSpawnTransform m_playerSpawnTransform;
    bool m_cameraIntroActive = false;
    float m_cameraIntroElapsedSeconds = 0.0f;
    float m_cameraIntroDurationSeconds = 0.0f;
    float m_cameraIntroStartMultiplier = 1.0f;
    float3 m_cameraBaseOffset{ 0.0f, 0.0f, 0.0f };
    bool m_isComposed = false;
    float m_resultElapsedSeconds = 0.0f;
    bool m_resultReturnQueued = false;


    SceneSpace::SceneManager* m_manager = nullptr;
};

#endif
