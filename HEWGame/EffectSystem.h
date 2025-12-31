#ifndef __EFFECTSYSTEM__H__
#define __EFFECTSYSTEM__H__

#pragma once
#include <Effekseer.h>
#include <EffekseerRendererDX11.h>
#include <unordered_map>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/UpdateNode.h"
#include "Vector.h"
#include "ICamera.h"
#include <tuple>

using EffectFrame = int32_t;

struct EffectCreateData
{
	std::u16string path;
	float magnification;

	EffectCreateData(std::u16string a_path, float a_magnification)
		: path(a_path)
		, magnification(a_magnification)
	{
	}
};


class EffectSystem :public Core::UpdateNode
{
public:

	struct EffectCreateRequest
	{
		std::string effectName;
		float3 spawnPosition;
		float3 moveSpeed;
		EffectFrame startFrame;

		EffectCreateRequest(
			std::string a_effectName,
			float3 a_spawnPosition,
			float3 a_moveSpeed,
			EffectFrame a_startFrame = 0
		)
			: effectName(a_effectName)
			, spawnPosition(a_spawnPosition)
			, moveSpeed(a_moveSpeed)
			, startFrame(a_startFrame)
		{}
	};

	EffectSystem(
		::EffekseerRendererDX11::RendererRef a_render,
		::Effekseer::ManagerRef manager,
		const std::unordered_map<std::string, EffectCreateData>& a_effectPathMap
	);
		
	~EffectSystem();

	void SpawnEffect(const EffectCreateRequest& a_request);

	void OnUpdate(const Core::UpdateContext& ctx) override final;
	void OnUpdate(Core::UpdateContext& ctx) override final;
	void Draw(const CameraSpace::ICamera* a_camera);

	float m_time;

private:
	

	struct PlayingEffect
	{
		Effekseer::Handle handle;
		Effekseer::Vector3D move;
		PlayingEffect(Effekseer::Handle a_handle, Effekseer::Vector3D a_move)
			: handle(a_handle)
			, move(a_move)
		{
		}

		PlayingEffect(Effekseer::Handle a_handle, float3 a_move)
			: handle(a_handle)
		{
			move = Effekseer::Vector3D(a_move.x, a_move.y, a_move.z);
		}
	};

	void UpdateEffectTransform(const PlayingEffect& effect, float dt);

	std::vector<PlayingEffect> m_playingEffects;

	::EffekseerRendererDX11::RendererRef m_renderer;
	::Effekseer::ManagerRef m_manager;
	std::unordered_map<std::string, ::Effekseer::EffectRef> m_effectCache;
	
};
#endif //__EFFECTSYSTEM__H__

