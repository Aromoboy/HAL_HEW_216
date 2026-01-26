#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>


class Save
{
public:
	static Save& Instance()
	{
		static Save instance;
		return instance;
	}

	int GetCost();
	void UpdateCost(int);
	bool FileSave();
	int GetLevel(std::string key);
	bool UpdatePowerUp(std::string key, int level);

private:
	bool LoadData();
	
	void LoadCost(std::ifstream&);
	void LoadLevel(std::ifstream&);

	void SaveCost(std::ifstream& file, std::string& data); 
	void SaveLevel(std::ifstream& file, std::string& data);


	int m_cost = 0;
	std::unordered_map<std::string, int> m_levels;

	Save();
	~Save() = default;
};