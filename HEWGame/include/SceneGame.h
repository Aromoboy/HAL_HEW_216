#ifndef __SCENE_GAME_H__
#define __SCENE_GAME_H__

#include "Scene.h"
#include "Model.h"
#include "TestObject.h"
#include "SoundManagerEX.h"

#include "CameraRig.h"
#include "Entity.h"
#include "IModelLoader.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "PerspectiveCamera.h"
#include "Vector.h"

#include <memory>
#include <vector>

namespace Systems
{
class PhysicsMachine;
}

struct CameraPrototypeSettings
{
        bool fixedPosition = false;
        float followSpeed = 6.0f;
        float3 offset{ 0.0f, 3.0f, -12.0f };
        float fovDegrees = 60.0f;
        float yawRadians = 0.0f;
        float pitchRadians = -0.2f;
};

class SceneGame : public SceneSpace::Scene
{
public:
        SceneGame();
        ~SceneGame();
        void ConfigureUpdates() override;
void Update(Core::UpdateContext& ctx) final;
void Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera) final;

private:
void ConfigureCamera(const CameraPrototypeSettings& settings);
std::unique_ptr<Model> m_pModel;
std::shared_ptr<CameraSpace::PerspectiveCamera> m_gameCamera;
std::unique_ptr<CameraRig> m_cameraRig;
std::shared_ptr<SceneSpace::Entity> m_cameraAnchor;
std::unique_ptr<SoundManagerEX> m_pManager;

std::unique_ptr<IModelLoader> m_loader;
std::shared_ptr<ModelDX> m_model;
std::unique_ptr<ModelRender> m_render;

std::vector<std::shared_ptr<TestObject>> m_pObjects;
std::unique_ptr<Systems::PhysicsMachine> m_physicsMachine;
bool m_inputRegistered = false;
bool m_physicsRegistered = false;
bool m_cameraRigRegistered = false;
};

#endif // __SCENE_GAME_H__
