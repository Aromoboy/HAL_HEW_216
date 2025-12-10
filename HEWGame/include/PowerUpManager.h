#pragma once

#include "Core/UpdateNode.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <utility>
#include <optional>
#include "TextureCache.h"
#include "Vector.h"
#include "PowerUpRenderer.h"

namespace Core
{
	class UpdateScheduler;
}

namespace Systems
{
	class DrawSystem;
	class PhysicsMachine;
}


namespace CameraSpace
{
	class ICamera;
}

class PowerUpNode;


class PowerUpManager : public Core::UpdateNode
{
public:

	struct NodeFormat
	{
		int x;
		int y;

		NodeFormat(int a_x, int a_y)
			:x(a_x)
			,y(a_y)
		{}
	};

	struct DrawInfomation
	{
		float3 drawPos;
		float3 drawScale;

		DrawInfomation(float3 _drawPos,float3 _drawScale)
			: drawPos(_drawPos)
			, drawScale(_drawScale)
		{
		}
	};

	PowerUpManager(Core::UpdateScheduler& a_scheduler,TextureCache& cache, const NodeFormat& format);
	~PowerUpManager();

	void OnUpdate(Core::UpdateContext& ctx) final;

	void SetFormat(const NodeFormat&);

	// Nodeを追加する
	void AddNode(std::string name, std::string infomation, std::string key, int a_level, int a_maxLevel, int a_defaultLevelUpCost, int a_costAdditional, float a_costMagnification);

	// 全てのノードを描画
	void DrawAll(PowerUpRenderer& powerUpRenderer, const CameraSpace::ICamera& Camera);

	// ゲッタ
	int GetLevel(int num); // Level
	int GetNodeCount(); // 保持しているノード数
	std::optional<float2> GetDrawPosition(int num);
	std::optional<int> GetCost(int);
	std::optional<float2> GetDrawScale(int);

	float2 GetNodeDistance();

	std::optional<DrawInfomation> GetDrawInfomation();


	// オブジェクト管理オブジェクトを取得
	void SetUpdateScheduler(Core::UpdateScheduler& scheduler);

	bool LevelUp(int);

private:
	
	// 全てのPowerUpNodeを保持
	std::vector<std::pair<std::string, std::unique_ptr<PowerUpNode>>> m_nodes;
	Core::UpdateScheduler& m_scheduler;

	NodeFormat m_format;

	

	TextureCache& m_cache;
};
