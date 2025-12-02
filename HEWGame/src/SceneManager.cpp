#include "SceneManager.h"

#include "Core/StepTimer.h"
#include "Core/Input.h"

#include <iomanip>
#include <memory>
#include <sstream>
#include <typeinfo>

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
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
    SceneManager::SceneManager(Core::StepTimer& timer, Core::Input& input)
        : m_timer(timer)
        , m_input(input)
        , m_sceneLoaded(false)
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
        LogSceneEvent("RequestSwap", m_pendingScene.get());
    }

    void SceneManager::Update()
    {
        m_timer.Tick();
        ActivatePendingScene();

        if (m_activeScene)
        {
            m_activeScene->Update(m_timer, m_input);
        }
    }

    void SceneManager::Draw()
    {
        if (m_activeScene && m_sceneLoaded)
        {
            m_activeScene->Draw();
        }
    }

    SceneSpace::IScene* SceneManager::GetActiveScene()
    {
        return m_activeScene.get();
    }

    const SceneSpace::IScene* SceneManager::GetActiveScene() const
    {
        return m_activeScene.get();
    }

    void SceneManager::ActivatePendingScene()
    {
        if (!m_pendingScene)
        {
            return;
        }

        if (m_activeScene)
        {
            LogSceneEvent("Exit", m_activeScene.get());
            m_activeScene->OnExit();
        }

        m_activeScene = std::move(m_pendingScene);
        m_sceneLoaded = false;

        LogSceneEvent("Load", m_activeScene.get());
        m_activeScene->Load(m_input);
        m_sceneLoaded = true;

        LogSceneEvent("Enter", m_activeScene.get());
        m_activeScene->OnEnter();
    }

    void SceneManager::LogSceneEvent(const char* eventLabel, const IScene* scene) const
    {
        std::ostringstream oss;
        oss << "[SceneManager] " << std::setw(10) << eventLabel
            << " scenePtr=" << scene;
        if (scene)
        {
            oss << " typeid=" << typeid(*scene).name();
        }
        oss << '\n';
        DebugTrace(oss.str());
    }
}
