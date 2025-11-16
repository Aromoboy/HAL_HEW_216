#ifndef __SCENE_GAME_H__
#define __SCENE_GAME_H__

#include "Scene.h"
#include "Model.h"
#include "TestObject.h"
#include "SoundManagerEX.h"

#include "IModelLoader.h"
#include "ModelDX.h"
#include "ModelRender.h"

#include <memory>
#include <vector>

class CameraDebug;
namespace Systems
{
        class PhysicsMachine;
}

class SceneGame : public SceneSpace::Scene
{
public:
        SceneGame();
        ~SceneGame();
        void ConfigureUpdates() override;
        void Update(Core::UpdateContext& ctx) final;
        void Draw() final;

private:
        std::unique_ptr<Model> m_pModel;
        std::unique_ptr<CameraDebug> m_pCamera;
        std::unique_ptr<SoundManagerEX> m_pManager;

        std::unique_ptr<IModelLoader> m_loader;
        std::shared_ptr<ModelDX> m_model;
        std::unique_ptr<ModelRender> m_render;

        std::vector<std::shared_ptr<TestObject>> m_pObjects;
        std::unique_ptr<Systems::PhysicsMachine> m_physicsMachine;
        bool m_inputRegistered = false;
        bool m_physicsRegistered = false;
};

#endif // __SCENE_GAME_H__
