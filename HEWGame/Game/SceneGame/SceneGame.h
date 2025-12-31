// ゲーム本編のプレイ中シーンをまとめたファイルだ
// ステージ設定を読み込んでプレイヤーやスイーツやカメラを動かす流れを組み立てる
// 進行状態や結果は画面表示や強化選択に渡される
#ifndef __SCENE_GAME_H__
#define __SCENE_GAME_H__

#include "Scene.h"
#include "SceneManager.h"
#include "Render/Legacy/Model.h"
#include "SoundManagerEX.h"
#include "CameraRig.h"
#include "Entities/PhysicalEntity.h"
#include "Entities/Player.h"
#include "GameplayRulesSystem.h"
#include "GuidelineSystem.h"
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
#include "ISceneServices.h"
#include <cstddef>
#include <memory>
#include <vector>
#include <string>
#include"SettingScene.h"
#include "ThirdParty/Effekseer/Effekseer.h"


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
    class DrawSystem;
}

namespace SweetsSpace
{
    class SweetsManager;
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

    SceneGame(SceneSpace::SceneManager& a_manager);
    ~SceneGame();
    bool Load(Core::Input& input) override;
    void Update(Core::StepTimer& timer, Core::Input& input) override;
    void Draw() override;
    void OnEnter() override;
    void OnExit() override;
    void ConfigureUpdates() override;
    void Update(const Core::UpdateContext& ctx) final;
    void Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera) final;
    void DrawOverlay() override;
    Systems::IPhysicsBinder* GetPhysicsBinder() override;
    Render::ModelRenderer* GetRenderer() override;
    Core::UpdateScheduler& GetScheduler() override;
    Core::Input* GetInput() override;
    Core::StepTimer* GetTimer() override;
private:
    void ConfigureCamera(const CameraPrototypeSettings& settings);
#if defined(_DEBUG)
    void HandleColliderDebugCommands(const Core::UpdateContext& ctx);
    void DrawPhysicsDebugOverlay();
#endif
    void SetStageState(StageState newState);
    void OnStageStart();
    void OnStageClear();
    void OnStageFail();
    void ResetPlaySession();
    void ResetGuidelineHeight();
    void ToggleGameplaySystems(bool enabled);
    int GetSweetsCount() const;
    void Initialize(const Stage::StageData& stageData);
    
    std::vector<StageObject> m_stageObjects;
    std::vector<std::shared_ptr<Entities::PhysicalEntity>> m_stagePhysicsEntities;
    std::shared_ptr<CameraSpace::PerspectiveCamera> m_gameCamera;
    std::unique_ptr<CameraRig> m_cameraRig;
    std::shared_ptr<Entities::Entity> m_cameraAnchor;
    std::unique_ptr<SoundManagerEX> m_pManager;
    Render::ModelRenderer* m_renderer = nullptr;
    std::unique_ptr<Entities::Player> player;
    std::unique_ptr<Systems::IPhysicsBinder> m_physicsBinder;
    std::unique_ptr<Systems::DrawSystem> m_drawSystem;
    std::unique_ptr<SweetsSpace::SweetsManager> m_sweetsManager;
    SceneGameContactHandler m_contactHandler;
    PlayerController m_playerController;
    bool m_inputRegistered = false;
    bool m_physicsRegistered = false;
    bool m_contactDispatchRegistered = false;
    bool m_cameraRigRegistered = false;
    bool m_drawRegistered = false;
    bool m_sweetsRegistered = false;
    bool m_sweetsLateRegistered = false;
    bool m_loadFailed = false;
    Stage::StageData m_stageData;
    GameFlow::PhaseManager m_phaseManager;
    GameplayRulesSystem m_gameplayRules;
    GuidelineSystem m_guidelineSystem;
    void BroadcastPhaseAdvance(int newPhase);
    void ForwardSweetsPhaseChange(int newPhase);
    StageState m_stageState = StageState::Intro;
    std::unique_ptr<SceneGameUIOverlay> m_uiOverlay;
    std::unique_ptr<UIManager> m_uiManager;
    std::unique_ptr<BackgroundRenderer> m_backgroundRenderer;
    float m_heightBonusThresholdMeters = 0.0f;
    float m_heightBonusSeconds = 0.0f;
    float m_playerBaseHorizontalSpeed = 0.0f;
    float m_playerBaseVerticalSpeed = 0.0f;
    float m_playerPickupBoxWidth = 0.0f;
    float m_playerPickupBoxHeight = 0.0f;
    Stage::PickupOffset m_playerPickupOffset{};
    float m_playerPickupMaxDistance = 0.0f;
    float m_playerPickupDepthThreshold = std::numeric_limits<float>::infinity();
    float3 m_playerPickupOriginOffset{ 0.0f, 0.0f, 0.0f };
    float m_playerSpeedMultiplier = 1.0f;
    Core::Input* m_inputService = nullptr;
    bool m_playerColliderMismatchLogged = false;
    std::shared_ptr<Render::Model> m_playerModel;
    // クリアできたかどうかの判定
    bool m_isGameClear = false;
    int m_phaseUpCount = 0;
    // 仮で作った何回フェーズを超えたらクリアになるかの判定
    int m_clearPhaseCount = 10;
    PlayerSpawnTransform m_playerSpawnTransform;
    bool m_isComposed = false;


    SceneSpace::SceneManager* m_manager = nullptr;
};

#endif // __SCENE_GAME_H__
