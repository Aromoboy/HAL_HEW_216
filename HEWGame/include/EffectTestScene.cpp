#include "EffectTestScene.h"
#include "DirectX.h"
#include "Defines.h"
#include "PerspectiveCamera.h"
#include <unordered_map>
#include <string>


EffectTestScene::EffectTestScene(EffekseerRendererDX11::RendererRef renderer, Effekseer::ManagerRef manager)
{
	camera = std::make_shared<CameraSpace::PerspectiveCamera>();

	std::unordered_map<std::string, EffectCreateData> map;
	map.try_emplace("fly", EffectCreateData(u"Assets/Effect/fly_effect_test01.efkefc", 1.0f));

	m_effectSystem = std::make_unique<EffectSystem>(renderer, manager, map);

	EffectSystem::EffectCreateRequest request("fly", float3{0.0f,0.0f,5.0f}, float3{ 0.0f,0.0f,100.0f }, 0);
	m_effectSystem->SpawnEffect(request);
}

EffectTestScene::~EffectTestScene()
{
	Unregister(*m_effectSystem);
}


void EffectTestScene::Draw()
{
	m_effectSystem->Draw(camera.get());
}

void EffectTestScene::Update(Core::StepTimer& timer, Core::Input& input)
{
	RootUpdate(timer, input);
}

void EffectTestScene::Update(Core::UpdateContext& ctx)
{
}



void EffectTestScene::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
	m_effectSystem->Draw(camera.get());
}

void EffectTestScene::Load(Core::Input& input)
{
}

void EffectTestScene::ConfigureUpdates()
{
	Register(*m_effectSystem);
}
