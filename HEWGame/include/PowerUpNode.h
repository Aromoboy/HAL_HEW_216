#pragma once
#include "Core/UpdateNode.h"
#include "TextureDX.h"
#include "Transform.h"
#include <memory>

class SpriteRenderer;

namespace CameraSpace
{
	class ICamera;
}

class PowerUpNode : public Core::UpdateNode
{
public:

	struct Textures
	{
		std::shared_ptr<Texture2D> backFrame;
		std::shared_ptr<Texture2D> frame;
		std::shared_ptr<Texture2D> cost;
		std::shared_ptr<Texture2D> icon;

		Textures(std::shared_ptr<Texture2D> a_backFrame, std::shared_ptr<Texture2D> a_frame, std::shared_ptr<Texture2D> a_cost, std::shared_ptr<Texture2D> a_icon)
			: backFrame(a_backFrame)
			, frame(a_frame)
			, cost(a_cost)
			, icon(a_icon)
		{
		}
	};

	PowerUpNode(Textures a_texture, float2 a_drawPos, float2 a_drawScale, int a_level, int a_maxLevel, int a_defaultLevelUpCost, int a_costAdditional, float a_costMagnification);
	~PowerUpNode() = default;


	
	void OnUpdate(Core::UpdateContext& ctx) final;
	void Draw(SpriteRenderer& render, const CameraSpace::ICamera& camera);
	bool LevelUp();
	int GetCost();
	int GetLevel();
	float2 GetDrawPos();
	float2 GetDrawScale();
	

private:
	// レベル
	int m_level;
	int m_maxLevel;

	// 強化コスト
	int m_levelUpCost;

	// 強化コスト倍率
	int m_defaultLevelUpCost;
	int m_costAdditional;
	float m_costMagnification;

	float2 m_drawPos;
	float2 m_drawScale;

	Textures m_texture;

	// コストの更新
	void UpdateCost();


	

};