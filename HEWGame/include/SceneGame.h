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
#include "Player.h"

#include "Vector.h"

#include <cstddef>
#include <memory>
#include <unordered_set>
#include <vector>

struct CameraPrototypeSettings
{
    bool fixedPosition = false;
    float followSpeed = 5.0f;
    float3 offset{ 0.0f, 0.0f, 0.0f };
    float fovDegrees = 60.0f;
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
};

namespace Systems
{
    class PhysicsMachine;
    class GravityDampingSystem;
    class ContactLiteSystem;
}

class SceneGame : public SceneSpace::Scene
{
public:
    SceneGame();
    ~SceneGame();
    void ConfigureUpdates() override;
    void Update(Core::UpdateContext& ctx) final;
    void Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera) final;
    void ApplyCameraPrototype(size_t index);

private:
    void ConfigureCamera(const CameraPrototypeSettings& settings);
    void RegisterPhysicsBody(SceneSpace::PhysicalEntity& entity);
    void UnregisterPhysicsBody(SceneSpace::PhysicalEntity* entity);
    void UnregisterAllPhysicsBodies();
    std::unique_ptr<Model> m_pModel;
    std::shared_ptr<CameraSpace::PerspectiveCamera> m_gameCamera;
    std::unique_ptr<CameraRig> m_cameraRig;
    std::shared_ptr<SceneSpace::Entity> m_cameraAnchor;
    std::unique_ptr<SoundManagerEX> m_pManager;

    std::shared_ptr<IModelLoader> m_loader;
    std::shared_ptr<ModelDX> m_model;
    std::unique_ptr<ModelRender> m_render;

    std::unique_ptr<SceneSpace::Player> player;

    std::vector<std::shared_ptr<TestObject>> m_pObjects;
    std::unique_ptr<Systems::PhysicsMachine> m_physicsMachine;
    std::shared_ptr<Systems::GravityDampingSystem> m_gravitySystem;
    std::unique_ptr<Systems::ContactLiteSystem> m_contactSystem;
    std::unordered_set<SceneSpace::PhysicalEntity*> m_registeredPhysicsBodies;
    bool m_inputRegistered = false;
    bool m_physicsRegistered = false;
    bool m_contactRegistered = false;
    bool m_cameraRigRegistered = false;
    size_t m_activeCameraPrototypeIndex = 0;
};

#endif // __SCENE_GAME_H__