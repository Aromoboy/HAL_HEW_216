#pragma once

#include <string>
#include <vector>


class PlayerData
{
public:
	struct Data
	{
		float fSpeed = 0.0f;
		bool bCream = false;
		float fCakeSize = 0.0f;
		float fReplenishSpeed = 0.0f;
		int nReplenishCount = 0;
		float fPtMultipl = 1.0f;
		int nPoints = 0;
	};

	// Storage format (INI-like):
	// [PlayerData]
	// entry=fSpeed,bCream,fCakeSize,fReplenishSpeed,nReplenishCount,fPtMultipl,nPoints
	bool LoadData();
	bool SaveData() const;

	const std::vector<Data>& GetEntries() const;
	void SetEntries(const std::vector<Data>& entries);
	void SetEntries(std::vector<Data>&& entries);

private:
	std::vector<Data> m_entries;
};
