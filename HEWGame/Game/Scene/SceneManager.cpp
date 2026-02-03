#include "SceneManager.h"

#include "Core/StepTimer.h"
#include "Core/Input.h"
#include "Core/IUpdatable.h"
#include "Scene.h"

#include <cassert>
#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>

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
}

namespace SceneSpace
{
    namespace
    {
        constexpr float kFadeDurationSeconds = 0.5f;

        float ApplyFadeEasing(float t, SceneGameConfig::RenderFadeEasing easing)
        {
            t = std::clamp(t, 0.0f, 1.0f);
            switch (easing)
            {
            case SceneGameConfig::RenderFadeEasing::SmoothStep:
                return t * t * (3.0f - 2.0f * t);
            case SceneGameConfig::RenderFadeEasing::EaseIn:
                return t * t;
            case SceneGameConfig::RenderFadeEasing::EaseOut:
                return 1.0f - (1.0f - t) * (1.0f - t);
            case SceneGameConfig::RenderFadeEasing::EaseInOut:
                if (t < 0.5f)
                {
                    return 2.0f * t * t;
                }
                return 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
            case SceneGameConfig::RenderFadeEasing::Linear:
            default:
                return t;
            }
        }
    }

    SceneTransitionController::SceneTransitionController()
        : m_state(State::Idle)
        , m_elapsed(0.0f)
        , m_duration(kFadeDurationSeconds)
        , m_fadeOutSeconds(kFadeDurationSeconds)
        , m_fadeInSeconds(kFadeDurationSeconds)
        , m_holdSeconds(0.0f)
        , m_easing(SceneGameConfig::RenderFadeEasing::Linear)
        , m_enabled(true)
    {
    }

    void SceneTransitionController::Configure(const SceneGameConfig::RenderFadeSettings& settings)
    {
        m_enabled = settings.enable;
        m_fadeOutSeconds = std::max(0.0f, settings.fadeOutSeconds);
        m_fadeInSeconds = std::max(0.0f, settings.fadeInSeconds);
        m_holdSeconds = std::max(0.0f, settings.holdSeconds);
        m_easing = settings.easing;
        if (!m_enabled)
        {
            Reset();
        }
    }

    void SceneTransitionController::BeginFadeOut()
    {
        if (!m_enabled)
        {
            m_state = State::Switching;
            m_elapsed = 0.0f;
            m_duration = 0.0f;
            return;
        }

        if (m_state != State::Idle)
        {
            return;
        }

        m_state = State::FadingOut;
        m_elapsed = 0.0f;
        m_duration = m_fadeOutSeconds;
    }

    void SceneTransitionController::BeginFadeIn()
    {
        if (!m_enabled)
        {
            Reset();
            return;
        }

        m_state = State::FadingIn;
        m_elapsed = 0.0f;
        m_duration = m_fadeInSeconds;
    }

    void SceneTransitionController::Update(float deltaSeconds)
    {
        if (m_state != State::FadingOut && m_state != State::FadingIn && m_state != State::Holding)
        {
            return;
        }

        m_elapsed += deltaSeconds;
        if (m_elapsed < m_duration)
        {
            return;
        }

        m_elapsed = m_duration;
        if (m_state == State::FadingOut)
        {
            if (m_holdSeconds > 0.0f)
            {
                m_state = State::Holding;
                m_elapsed = 0.0f;
                m_duration = m_holdSeconds;
                return;
            }

            m_state = State::Switching;
            return;
        }

        if (m_state == State::Holding)
        {
            m_state = State::Switching;
            return;
        }

        m_state = State::Idle;
    }

    void SceneTransitionController::Reset()
    {
        m_state = State::Idle;
        m_elapsed = 0.0f;
    }

    SceneTransitionController::State SceneTransitionController::GetState() const
    {
        return m_state;
    }

    bool SceneTransitionController::IsTransitioning() const
    {
        return m_state != State::Idle;
    }

    bool SceneTransitionController::IsEnabled() const
    {
        return m_enabled;
    }

    float SceneTransitionController::GetAlpha() const
    {
        if (!m_enabled)
        {
            return 0.0f;
        }

        if (m_state == State::Idle)
        {
            return 0.0f;
        }

        const float progress = m_duration > 0.0f ? std::clamp(m_elapsed / m_duration, 0.0f, 1.0f) : 1.0f;
        const float eased = ApplyFadeEasing(progress, m_easing);
        if (m_state == State::FadingOut)
        {
            return eased;
        }

        if (m_state == State::FadingIn)
        {
            return 1.0f - eased;
        }

        return 1.0f;
    }

    SceneManager::SceneManager(Core::StepTimer& timer, Core::Input& input, ::EffekseerRendererDX11::RendererRef a_renderer, ::Effekseer::ManagerRef a_manager)
        : m_timer(timer)
        , m_input(input)
        , m_sceneLoaded(false)
        , m_sceneEntered(false)
        , m_isEnd(false)
        , m_lastTransitionState(SceneTransitionController::State::Idle)
        , m_renderer(a_renderer)
        , m_manager(a_manager)
    {
    }

    void SceneManager::SetInitialScene(std::unique_ptr<IScene> scene)
    {
        m_pendingScene = std::move(scene);
        ActivatePendingScene();
    }

    void SceneManager::RequestSceneChange(std::unique_ptr<IScene> scene)
    {
        m_pendingScene = std::move(scene);
    }

    void SceneManager::Update()
    {
        m_timer.Tick();
        const float deltaSeconds = m_timer.DeltaTime();
        m_transition.Update(deltaSeconds);
        AdvanceTransition();
        UpdateActiveSceneIfAllowed();
    }

    void SceneManager::Draw()
    {
        if (m_activeScene)
        {
            assert(m_sceneLoaded && m_sceneEntered);
            m_activeScene->Draw();
        }
    }

    void SceneManager::DrawOverlay()
    {
        if (m_activeScene)
        {
            assert(m_sceneLoaded && m_sceneEntered);
            if (auto* scene = dynamic_cast<Scene*>(m_activeScene.get()))
            {
                scene->RootDrawOverlay();
            }
        }
    }

    void SceneManager::GameEndRequest()
    {
        m_isEnd = true;
    }

    bool SceneManager::IsGameEnd()
    {
        return m_isEnd;
    }

    SceneSpace::IScene* SceneManager::GetActiveScene()
    {
        return m_activeScene.get();
    }

    const SceneSpace::IScene* SceneManager::GetActiveScene() const
    {
        return m_activeScene.get();
    }

    float SceneManager::GetFadeAlpha() const
    {
        return m_transition.GetAlpha();
    }

    void SceneManager::ConfigureRenderFade(const SceneGameConfig::RenderFadeSettings& settings)
    {
        m_transition.Configure(settings);
    }

    void SceneManager::AdvanceTransition()
    {
        const SceneTransitionController::State stateBefore = m_transition.GetState();
        switch (stateBefore)
        {
        case SceneTransitionController::State::Idle:
            if (m_pendingScene)
            {
                if (!m_transition.IsEnabled())
                {
                    // インスタントスイッチ：遷移を無効化し、フェードを完全にスキップする
                    ActivatePendingScene();
                }
                else if (m_activeScene)
                {
                    m_transition.BeginFadeOut();
                }
                else
                {
                    // フェードをスキップ：アクティブなシーンはまだないため、すぐに読み込む
                    ActivatePendingScene();
                }
            }
            break;
        case SceneTransitionController::State::Switching:
            ActivatePendingScene();
            if (m_transition.IsEnabled())
            {
                m_transition.BeginFadeIn();
            }
            else
            {
                m_transition.Reset();
            }
            break;
        case SceneTransitionController::State::FadingOut:
        case SceneTransitionController::State::Holding:
        case SceneTransitionController::State::FadingIn:
        default:
            break;
        }

        const SceneTransitionController::State stateAfter = m_transition.GetState();
        if (m_lastTransitionState == SceneTransitionController::State::Idle
            && stateAfter != SceneTransitionController::State::Idle)
        {
            if (m_activeScene)
            {
                m_activeScene->OnTransitionBegin();
            }
        }
        else if (m_lastTransitionState != SceneTransitionController::State::Idle
            && stateAfter == SceneTransitionController::State::Idle)
        {
            if (m_activeScene)
            {
                m_activeScene->OnTransitionEnd();
            }
        }

        m_lastTransitionState = stateAfter;
        m_input.Enable(!m_transition.IsTransitioning());
    }

    void SceneManager::UpdateActiveSceneIfAllowed()
    {
        if (m_activeScene && !m_transition.IsTransitioning())
        {
            assert(m_sceneLoaded && m_sceneEntered);
            m_activeScene->Update(m_timer, m_input);
        }
    }

    SceneTransitionController::State SceneManager::GetTransitionState() const
    {
        return m_transition.GetState();
    }

    bool SceneManager::IsTransitioning() const
    {
        return m_transition.IsTransitioning();
    }

    ::EffekseerRendererDX11::RendererRef SceneManager::GetEffectRenderer()
    {
        return m_renderer;
    }

    ::Effekseer::ManagerRef SceneManager::GetEffectManager()
    {
        return m_manager;
    }

    void SceneManager::ActivatePendingScene()
    {
        if (!m_pendingScene)
        {
            return;
        }

        if (m_activeScene)
        {
            if (m_sceneEntered)
            {
                m_activeScene->OnExit();
            }
            else
            {
                assert(false && "Attempted to exit an uninitialized scene");
            }
        }

        m_sceneEntered = false;
        m_sceneLoaded = false;

        std::unique_ptr<IScene> nextScene = std::move(m_pendingScene);
        if (!nextScene->Load(m_input))
        {
            LogSceneEvent("Scene load failed; activation canceled.", nextScene.get());
            if (m_activeScene)
            {
                m_activeScene->OnEnter();
                m_sceneLoaded = true;
                m_sceneEntered = true;
            }
            return;
        }

        m_activeScene = std::move(nextScene);
        m_sceneLoaded = true;
        m_sceneEntered = false;

        m_activeScene->OnEnter();
        m_sceneEntered = true;

        if (m_transition.IsTransitioning())
        {
            m_activeScene->OnTransitionBegin();
        }
    }

    void SceneManager::LogSceneEvent(const char* eventLabel, const IScene* scene) const
    {
        std::ostringstream oss;
        oss << "[SceneManager] " << eventLabel << " scenePtr=" << scene << "\n";
        DebugTrace(oss.str());
    }
}
