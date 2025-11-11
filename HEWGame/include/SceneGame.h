#ifndef __SCENE_GAME_H__
#define __SCENE_GAME_H__

#include "Scene.h"
#include "Model.h"
#include "ICamera.h"
#include "TestObject.h"
#include "SoundManagerEX.h"

#include "IModelLoader.h"
#include "ModelDX.h"
#include "ModelRender.h"

#include <memory>

class SceneGame : public Scene
{
public:
	SceneGame();
	~SceneGame();
	void Update(const Core::UpdateContext& context) final;
	void Draw() final;


private:
	std::unique_ptr<Model> m_pModel;
	std::unique_ptr<ICamera> m_pCamera;
	std::unique_ptr<SoundManagerEX> m_pManager;

	std::unique_ptr<IModelLoader> m_loader;
	std::shared_ptr<ModelDX> m_model;
	std::unique_ptr<ModelRender> m_render;

	std::vector<std::shared_ptr<TestObject>> m_pObjects;
};

#endif // __SCENE_GAME_H__