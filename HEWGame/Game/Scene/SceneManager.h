// 画面切り替えをまとめた仕組みでゲーム起動中ずっと使う
// 現在のシーンと次に切り替えるシーンを保持して更新と描画を順番に呼ぶ
// 切り替え結果はゲーム全体の流れに反映される
#ifndef __SCENE_MANAGER_H__
#define __SCENE_MANAGER_H__

#include "SceneGameConfigTypes.h"
#include "VideoBackgroundRenderer.h"

#include <Effekseer.h>
#include <EffekseerRendererDX11.h>
#include <memory>
#include <string>

namespace Core
{
    class StepTimer;
    class Input;
}

namespace SceneSpace
{
    class SceneTransitionController
    {
    public:
        enum class State
        {
            Idle,
            FadingOut,
            Holding,
            Switching,
            FadingIn,
            PlayingTransitionVideo,
        };

        SceneTransitionController();
        void Configure(const SceneGameConfig::RenderFadeSettings& settings);
        void BeginFadeOut();
        void BeginFadeIn();
        void Update(float deltaSeconds);
        void Reset();
        State GetState() const;
        bool IsTransitioning() const;
        bool IsEnabled() const;
        float GetAlpha() const;

    private:
        State m_state;
        float m_elapsed;
        float m_duration;
        float m_fadeOutSeconds;
        float m_fadeInSeconds;
        float m_holdSeconds;
        SceneGameConfig::RenderFadeEasing m_easing;
        bool m_enabled;
    };

    class IScene
    {
    public:
        virtual ~IScene() = default;
        virtual bool Load(Core::Input& input) = 0;
        virtual void Update(Core::StepTimer& timer, Core::Input& input) = 0;
        virtual void Draw() = 0;
        virtual void OnEnter() {}
        virtual void OnExit() {}
        virtual void OnTransitionBegin() {}
        virtual void OnTransitionEnd() {}
    };

    class SceneManager
    {
    public:
        SceneManager(Core::StepTimer& timer, Core::Input& input, ::EffekseerRendererDX11::RendererRef m_renderer, ::Effekseer::ManagerRef m_manager);
        void SetInitialScene(std::unique_ptr<IScene> scene);
        void RequestSceneChange(std::unique_ptr<IScene> scene);
        void ConfigureRenderFade(const SceneGameConfig::RenderFadeSettings& settings,
                                 const SceneGameConfig::RenderTransitionVideoSettings& transitionVideoSettings);
        void Update();
        void Draw();
        void GameEndRequest();
        bool IsGameEnd();
        IScene* GetActiveScene();
        const IScene* GetActiveScene() const;
        float GetFadeAlpha() const;
        SceneTransitionController::State GetTransitionState() const;
        bool IsTransitioning() const;
        bool IsVideoTransitionActive() const;
        bool IsUsingVideoTransition() const;
        ::EffekseerRendererDX11::RendererRef GetEffectRenderer();
        ::Effekseer::ManagerRef GetEffectManager();

    private:
        void AdvanceTransition();
        void UpdateActiveSceneIfAllowed();
        void ActivatePendingScene();
        void LogSceneEvent(const char* eventLabel, const IScene* scene) const;

        Core::StepTimer& m_timer;
        Core::Input& m_input;
        ::EffekseerRendererDX11::RendererRef m_renderer;
        ::Effekseer::ManagerRef m_manager;


        std::unique_ptr<IScene> m_activeScene;
        std::unique_ptr<IScene> m_pendingScene;
        bool m_sceneLoaded;
        bool m_sceneEntered;
        bool m_isEnd;
        SceneTransitionController m_transition;
        bool m_transitionVideoEnabled;
        std::string m_transitionVideoPath;
        std::unique_ptr<VideoBackgroundRenderer> m_transitionVideo;
        SceneTransitionController::State m_lastTransitionState;
    };
}

#endif
