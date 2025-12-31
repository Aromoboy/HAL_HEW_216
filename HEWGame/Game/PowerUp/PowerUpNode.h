// 強化項目の一つを表すデータと動きを扱う
// 強化選択の更新と描画から参照される

#pragma once
#include "Core/UpdateNode.h"
#include "TextureDX.h"
#include "Transform.h"
#include <memory>
#include "PowerUpRenderer.h"

class SpriteRenderer;


namespace CameraSpace
{
	class ICamera;
}

class PowerUpNode : public Core::UpdateNode
{
public:


	PowerUpNode(std::shared_ptr<Texture2D> a_texture, std::string a_nameText, std::string a_infomationText, float2 a_drawPos, float2 a_drawScale, int a_level, int a_maxLevel, int a_defaultLevelUpCost, int a_costAdditional, float a_costMagnification);
	~PowerUpNode() = default;


	
	void OnUpdate(const Core::UpdateContext& ctx) final;
	void Draw(PowerUpRenderer& render, const CameraSpace::ICamera& camera);
	bool LevelUp();
	int GetCost();
	int GetLevel();
	float2 GetDrawPos();
	float2 GetDrawScale();
	

private:

	// テクスチャ
	std::shared_ptr<Texture2D> m_texture;
	std::shared_ptr<PowerUpStringTexture> m_stringTexture;
	bool m_dirty = true;

	// テキスト
	std::string m_nameText;
	std::string m_infomationText;

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

	

	// コストの更新
	void UpdateCost();


	

};