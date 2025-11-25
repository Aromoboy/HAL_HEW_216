#pragma once

#include "Scene/IScene.h"

namespace CameraSpace
{
    class CameraManager;
}

namespace Systems
{
    class PhysicsMachine;
}

namespace Core
{
    class DXContext;
    class UpdateScheduler;
    struct UpdateContext;
}

namespace SceneSpace
{
class PlayScene : public IScene
{
public:
    PlayScene(Core::UpdateScheduler* scheduler,
              CameraSpace::CameraManager* cameraManager,
              Systems::PhysicsMachine* physicsMachine,
              Core::DXContext* renderContext,
              Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
              int priority = 0);

    void Load() override;
    void Update(Core::UpdateContext& ctx) override;
    void Draw() override;

private:
    Core::UpdateScheduler* m_updateScheduler;
    CameraSpace::CameraManager* m_cameraManager;
    Systems::PhysicsMachine* m_physicsMachine;
    Core::DXContext* m_renderContext;
};
}

