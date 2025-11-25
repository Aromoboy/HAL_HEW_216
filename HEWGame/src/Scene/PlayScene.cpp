#include "Scene/PlayScene.h"

#include "CameraManager.h"
#include "Core/DXContext.h"
#include "Core/UpdateScheduler.h"
#include "Systems/PhysicsMachine.h"

namespace SceneSpace
{
PlayScene::PlayScene(Core::UpdateScheduler* scheduler,
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

void PlayScene::Load()
{
}

void PlayScene::Update(Core::UpdateContext& ctx)
{
    (void)ctx;
}

void PlayScene::Draw()
{
}
}

