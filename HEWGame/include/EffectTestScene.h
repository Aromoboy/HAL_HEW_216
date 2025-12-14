#pragma once


#include <Effekseer.h>
#include <EffekseerRendererDX11.h>
#include <EffekseerSoundXAudio2.h>
#include <memory>
//$(ProjectDir)ThirdParty\Effekseer
#include "ICamera.h"
#include "Scene.h"
#include "SceneManager.h"
#include "EffectSystem.h"


class EffectTestScene :public SceneSpace::Scene, public SceneSpace::IScene
{
public:
	EffectTestScene(EffekseerRendererDX11::RendererRef, Effekseer::ManagerRef);
	~EffectTestScene();

	
	virtual void Update(Core::StepTimer& timer, Core::Input& input) override;
	virtual void Update(Core::UpdateContext& ctx) override;
	virtual void Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera);
	virtual void Draw()override;
	virtual void Load(Core::Input& input) override;
	void ConfigureUpdates() override;
private:

	std::unique_ptr<EffectSystem> m_effectSystem;

	std::shared_ptr<CameraSpace::ICamera> camera;
	::Effekseer::Backend::GraphicsDeviceRef device;
	::EffekseerRendererDX11::RendererRef render;
	::Effekseer::ManagerRef manager;
	int32_t time = 0;
	Effekseer::Handle efkHandle = 0;

	bool test = true;

};