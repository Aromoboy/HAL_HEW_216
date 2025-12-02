#include "PowerUpNode.h"

#include "SpriteRenderer.h"
#include "ICamera.h"
#include "Debug.h"

constexpr float kBackDepth = 1.0f;
constexpr float kFrameDepth = 1.0f;
constexpr float kIconDepth = 1.0f;

constexpr float kBackScale = 1.8f;
constexpr float kFrameScale = 0.5f;
constexpr float kIconScale = 0.5f;

constexpr float2 kBackOffset  = { 0.00f,  0.000f };
constexpr float2 kFrameOffset = {-0.18f, -0.008f };
constexpr float2 kIconOffset  = {-0.18f, -0.008f };


bool PowerUpNode::LevelUp()
{
	// 最大レベルならば返す
	if (m_level == m_maxLevel) return false;

	// レベルアップ
	m_level++;
	UpdateCost();
	DebugMessage(std::to_string(m_level));

	return true;
}

int PowerUpNode::GetCost()
{
	return m_levelUpCost;
}

int PowerUpNode::GetLevel()
{
	return m_level;
}

float2 PowerUpNode::GetDrawPos()
{
	return m_drawPos;
}

float2 PowerUpNode::GetDrawScale()
{
	float2 ret{ m_drawScale.x * kBackScale, m_drawScale.y };
	return ret;
}

void PowerUpNode::Draw(SpriteRenderer& render, const CameraSpace::ICamera& camera)
{

	Transform trans;
	float2 offset;

	offset.x = kBackOffset.x * m_drawScale.x;
	offset.y = kBackOffset.y * m_drawScale.y;
	trans.position = float3{ m_drawPos.x + kBackOffset.x, m_drawPos.y + kBackOffset.y, kBackDepth };
	trans.scale = float3{ m_drawScale.x * kBackScale, m_drawScale.y * kBackScale, 1.0 };

	render.Draw(*m_texture.backFrame, trans, &camera);


	offset.x = kFrameOffset.x * m_drawScale.x * kBackScale;
	offset.y = kFrameOffset.y * m_drawScale.y * kBackScale;

	// 
	trans.position = float3{ m_drawPos.x + offset.x, m_drawPos.y + offset.y, kFrameDepth };
	trans.scale = float3{ m_drawScale.x * kFrameScale, m_drawScale.y * kFrameScale, 1.0f };

	render.Draw(*m_texture.frame, trans, &camera);

	offset.x = kIconOffset.x * m_drawScale.x * kBackScale;
	offset.y = kIconOffset.y * m_drawScale.y * kBackScale;

	trans.position = float3{ m_drawPos.x +offset.x, m_drawPos.y + offset.y, kIconDepth };
	trans.scale = float3{ m_drawScale.x * kIconScale, m_drawScale.y * kIconScale, 1.0f };

	render.Draw(*m_texture.icon, trans, &camera);
}

void PowerUpNode::UpdateCost()
{
	float magnification = m_costMagnification * m_level;

	m_levelUpCost = m_defaultLevelUpCost * (magnification > 1.0f ? magnification : 1.0f) + m_level * m_costAdditional;
}

PowerUpNode::PowerUpNode(Textures a_texture, float2 a_drawPos, float2 a_drawScale, int a_level, int a_maxLevel, int a_defaultLevelUpCost, int a_costAdditional, float a_costMagnification)
	: m_texture(a_texture)
	, m_drawPos(a_drawPos)
	, m_drawScale(a_drawScale)
	, m_level(a_level)
	, m_maxLevel(a_maxLevel)
	, m_defaultLevelUpCost(a_defaultLevelUpCost)
	, m_costAdditional(a_costAdditional)
	, m_costMagnification(a_costMagnification)
{
	UpdateCost();
}

void PowerUpNode::OnUpdate(Core::UpdateContext& ctx)
{
	(void)ctx;
}