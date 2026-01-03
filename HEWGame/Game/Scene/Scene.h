#ifndef __SCENE_H__
#define __SCENE_H__

#include "CameraManager.h"
#include "Core/UpdateScheduler.h"

#include <memory>

namespace Core
{
        class StepTimer;
        class Input;
        struct UpdateContext;
        class IUpdatable;
}

namespace SceneSpace
{
class Scene
{
public:
void RootUpdate(Core::StepTimer& timer, Core::Input& input);
void RootDraw();
std::shared_ptr<::CameraSpace::ICamera> GetActiveCamera() const;

Scene();
virtual ~Scene();
virtual void Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera) = 0;

protected:
virtual void ConfigureUpdates();
virtual void Update(const Core::UpdateContext& ctx) = 0;
virtual void DrawWorld(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera);
virtual void DrawOverlay();
void RegisterSceneUpdateNode(Core::UpdatePhase phase, int priority);
void UnregisterSceneUpdateNode();
Core::IUpdatable* GetSceneUpdateNode();
const Core::IUpdatable* GetSceneUpdateNode() const;
void RegisterCameraUpdateNode(Core::UpdatePhase phase, int priority);
void UnregisterCameraUpdateNode();
Core::IUpdatable* GetCameraUpdateNode();
const Core::IUpdatable* GetCameraUpdateNode() const;

bool Register(Core::IUpdatable& node);
void Unregister(Core::IUpdatable& node);

Core::UpdateScheduler& GetUpdateScheduler();
Core::Input* GetInput();
const Core::Input* GetInput() const;
Core::StepTimer* GetTimer();
const Core::StepTimer* GetTimer() const;
CameraSpace::CameraManager& GetCameraManager();
const CameraSpace::CameraManager& GetCameraManager() const;
std::shared_ptr<::CameraSpace::ICamera> AddCamera(std::shared_ptr<::CameraSpace::ICamera> camera);
void SetActiveCamera(const std::shared_ptr<::CameraSpace::ICamera>& camera);


private:
Core::UpdateScheduler m_scheduler;
bool m_isConfigured;
Core::Input* m_input;
Core::StepTimer* m_timer;
CameraSpace::CameraManager m_cameraManager;
class SceneUpdateNode;
std::unique_ptr<SceneUpdateNode> m_sceneUpdateNode;
bool m_sceneUpdateRegistered;
class CameraUpdateNode;
std::unique_ptr<CameraUpdateNode> m_cameraUpdateNode;
bool m_cameraUpdateRegistered;
};
}

#endif
