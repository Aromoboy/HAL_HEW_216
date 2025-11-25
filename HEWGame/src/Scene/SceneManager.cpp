#include "Scene/SceneManager.h"

#include "CameraManager.h"
#include "Core/DXContext.h"
#include "Core/UpdateScheduler.h"
#include "Systems/PhysicsMachine.h"

namespace SceneSpace
{
SceneManager::SceneManager(Core::UpdateScheduler* scheduler,
                         CameraSpace::CameraManager* cameraManager,
                         Systems::PhysicsMachine* physicsMachine,
                         Core::DXContext* renderContext,
                         Core::UpdatePhase phase,
                         int priority)
    : IScene(phase, priority)
    , m_updateScheduler(scheduler)
    , m_cameraManager(cameraManager)
    , m_physicsMachine(physicsMachine)
    , m_renderContext(renderContext)
{
    if (m_updateScheduler)
    {
        m_updateScheduler->Register(*this);
    }
}

void SceneManager::Load()
{
}

void SceneManager::Update(Core::UpdateContext& ctx)
{
    (void)ctx;
}

void SceneManager::Draw()
{
}

void SceneManager::SetActiveScene(std::shared_ptr<IScene> scene)
{
    m_activeScene = std::move(scene);
}

std::shared_ptr<IScene> SceneManager::GetActiveScene() const
{
    return m_activeScene;
}
}

