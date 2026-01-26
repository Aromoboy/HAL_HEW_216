#pragma once

#include <unordered_map>
#include <string>
#include <vector>

#include "PowerUpEffectConfig.h"


class PlayerData
{
public:
	struct Data
	{
		std::unordered_map<PowerUpType, int> levels;
		float fSpeed = 0.0f;
		bool creamEnabled = false;
		float fCakeSize = 0.0f;
		float fReplenishSpeed = 0.0f;
		int nReplenishCount = 0;
		float fPtMultipl = 1.0f;
		int nPoints = 0;
	};

	// Storage format (INI-like):
	// [PlayerData]
	// entry=currency,SpeedUp=level,CakeSize=level,SweetsSpawnSpeed=level,SweetsSpawnQuantity=level,PointAmount=level,Whip=level
	bool LoadData();
	bool SaveData() const;

	const std::vector<Data>& GetEntries() const;
	void SetEntries(const std::vector<Data>& entries);
	void SetEntries(std::vector<Data>&& entries);

private:
	std::vector<Data> m_entries;
};
