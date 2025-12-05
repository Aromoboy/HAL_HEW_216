#include "SceneManager.h"

#include "Core/StepTimer.h"
#include "Core/Input.h"

#include <memory>

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
            m_activeScene->OnExit();
        }

        m_activeScene = std::move(m_pendingScene);
        m_sceneLoaded = false;

        m_activeScene->Load(m_input);
        m_sceneLoaded = true;

        m_activeScene->OnEnter();
    }
}
