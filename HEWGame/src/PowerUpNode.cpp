#include "PowerUpNode.h"


#include "SpriteRenderer.h"
#include "ICamera.h"



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
	float2 ret{ m_drawScale.x, m_drawScale.y };
	return ret;
}

void PowerUpNode::Draw(PowerUpRenderer& render, const CameraSpace::ICamera& camera)
{

	// 描画用データを作成
	PowerUpRenderer::NodeRenderData data(m_texture, m_stringTexture,m_drawPos, m_drawScale, m_dirty, m_nameText, m_infomationText, m_level, m_levelUpCost);

	// 描画
	render.DrawNode(data, camera);
	m_dirty = false;
	
}

void PowerUpNode::UpdateCost()
{
	float magnification = m_costMagnification * m_level;

	m_levelUpCost = m_defaultLevelUpCost * (magnification > 1.0f ? magnification : 1.0f) + m_level * m_costAdditional;

	m_dirty = true;
}

PowerUpNode::PowerUpNode(std::shared_ptr<Texture2D> a_texture, std::string a_nameText, std::string a_infomationText, float2 a_drawPos, float2 a_drawScale, int a_level, int a_maxLevel, int a_defaultLevelUpCost, int a_costAdditional, float a_costMagnification)
	: m_texture(a_texture)
	, m_nameText(a_nameText)
	, m_infomationText(a_infomationText)
	, m_drawPos(a_drawPos)
	, m_drawScale(a_drawScale)
	, m_level(a_level)
	, m_maxLevel(a_maxLevel)
	, m_defaultLevelUpCost(a_defaultLevelUpCost)
	, m_costAdditional(a_costAdditional)
	, m_costMagnification(a_costMagnification)
{
	m_stringTexture = std::make_shared<PowerUpStringTexture>();
	m_stringTexture->Create(DXGI_FORMAT_R8G8B8A8_UNORM, 512u, 240u);
	UpdateCost();
}


void PowerUpNode::OnUpdate(Core::UpdateContext& ctx)
{
	(void)ctx;
}