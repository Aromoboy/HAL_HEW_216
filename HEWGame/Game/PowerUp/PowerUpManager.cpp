// 強化選択の流れと登録をまとめている
// 強化シーンの更新から呼ばれる

#include "PowerUpManager.h"

#include "Core/UpdateScheduler.h"
#include "PowerUpNode.h"
#include "SpriteRenderer.h"
#include "ICamera.h"
#include "Defines.h"
#include "Debug.h"

#include <cassert>


constexpr float2 kUnitDistanse = { 1.4f, -0.5f };

PowerUpManager::PowerUpManager(Core::UpdateScheduler& a_scheduler, TextureCache& a_cache, const NodeFormat& a_format)
	:m_scheduler(a_scheduler)
	, m_cache(a_cache)
	, m_format(a_format)
{
}

PowerUpManager::~PowerUpManager()
{
	// 全て開放
	for (auto& it : m_nodes)
	{
		it.second->ReleaseRegistrationHandle();
	}
}

void PowerUpManager::OnUpdate(const Core::UpdateContext& ctx)
{
	(void)ctx;
}

void PowerUpManager::SetFormat(const NodeFormat& a_format)
{
	m_format = a_format;
}

void PowerUpManager::AddNode(std::string name, std::string infomation, std::string key, int a_level, int a_maxLevel, int a_defaultLevelUpCost, int a_costAdditional, float a_costMagnification)
{
	std::shared_ptr<Texture2D> texture = m_cache.Get(key);

	int indexX = m_nodes.size() / m_format.y;
	int indexY = m_nodes.size() % m_format.y;

	float2 drawPos{ kUnitDistanse.x / m_format.x * indexX  - 0.5f, kUnitDistanse.y / m_format.y * indexY + 0.1f };
	float2 drawScale{ 1.0 / m_format.x, 1.0f / m_format.x };

	std::pair<std::string, std::unique_ptr<PowerUpNode>> node(key, std::make_unique<PowerUpNode>(texture, name, infomation, drawPos, drawScale, a_level, a_maxLevel, a_defaultLevelUpCost, a_costAdditional, a_costMagnification));
	m_nodes.push_back(std::move(node));
	auto handle = m_scheduler.Register(*m_nodes.back().second);
	if (!handle.IsValid())
	{
		DebugLog::DebugMessage("[PowerUpManager] Scheduler registration failed for PowerUpNode.\n");
		assert(false && "PowerUpNode registration failed.");
		return;
	}
	m_nodes.back().second->SetRegistrationHandle(std::move(handle));
	
}

void PowerUpManager::DrawAll(PowerUpRenderer& powerUpRenderer, const CameraSpace::ICamera& camera)
{
	for (auto& it : m_nodes)
	{
		it.second->Draw(powerUpRenderer, camera);
	}
}

int PowerUpManager::GetLevel(int num)
{
	if (num >= m_nodes.size()) return -1;

	return m_nodes.at(num).second->GetLevel();
}


int PowerUpManager::GetNodeCount()
{
	return m_nodes.size();
}

std::optional<float2> PowerUpManager::GetDrawPosition(int a_index)
{
	if (a_index >= m_nodes.size() || a_index < 0)
	{
		DebugMessage("Nodeが見つかりませんでした");
		return std::nullopt;
	}
	return m_nodes.at(a_index).second->GetDrawPos();
}

std::optional<float2> PowerUpManager::GetDrawScale(int a_index)
{
	if (a_index >= m_nodes.size() || a_index < 0) return std::nullopt;


	return m_nodes.at(a_index).second->GetDrawScale();
}

float2 PowerUpManager::GetNodeDistance()
{
	return float2{kUnitDistanse.x / m_format.x, kUnitDistanse.y / m_format.y };
}

std::optional<PowerUpManager::DrawInfomation> PowerUpManager::GetDrawInfomation()
{
	return std::optional<DrawInfomation>();
}

std::optional<int> PowerUpManager::GetCost(int a_index)
{
	if (a_index >= m_nodes.size() || a_index < 0) return std::nullopt;

	
	return m_nodes.at(a_index).second->GetCost();
}


bool PowerUpManager::LevelUp(int a_index)
{
	if (a_index >= m_nodes.size() || a_index < 0) return false;

	return m_nodes.at(a_index).second->LevelUp();
}
