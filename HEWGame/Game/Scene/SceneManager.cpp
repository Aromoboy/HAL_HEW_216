#include "SceneManager.h"

#include "Core/StepTimer.h"
#include "Core/Input.h"
#include "Core/IUpdatable.h"

#include <cassert>
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
    SceneManager::SceneManager(Core::StepTimer& timer, Core::Input& input)
        : m_timer(timer)
        , m_input(input)
        , m_sceneLoaded(false)
        , m_sceneEntered(false)
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
        ActivatePendingScene();

        if (m_activeScene)
        {
            assert(m_sceneLoaded && m_sceneEntered);
            m_activeScene->Update(m_timer, m_input);
        }
    }

    void SceneManager::Draw()
    {
        if (m_activeScene)
        {
            assert(m_sceneLoaded && m_sceneEntered);
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

        std::unique_ptr<IScene> nextScene = std::move(m_pendingScene);
        if (!nextScene->Load(m_input))
        {
            LogSceneEvent("Scene load failed; activation canceled.", nextScene.get());
            return;
        }

        if (m_activeScene)
        {
            if (m_sceneEntered)
            {
                m_activeScene->OnExit();
                m_sceneEntered = false;
                m_sceneLoaded = false;
            }
            else
            {
                assert(false && "Attempted to exit an uninitialized scene");
            }
        }

        m_activeScene = std::move(nextScene);
        m_sceneLoaded = true;
        m_sceneEntered = false;

        m_activeScene->OnEnter();
        m_sceneEntered = true;
    }

    void SceneManager::LogSceneEvent(const char* eventLabel, const IScene* scene) const
    {
        std::ostringstream oss;
        oss << "[SceneManager] " << eventLabel << " scenePtr=" << scene << "\n";
        DebugTrace(oss.str());
    }
}
