#include "Save.h"
#include <fstream>
#include <sstream>

constexpr const char* kSaveFilePath = "Assets/Data/Save.cfg";

Save::Save()
	: m_cost(0)
	, m_levels()
{
	LoadData();
}

int Save::GetCost()
{
	return m_cost;
}

void Save::UpdateCost(int a_cost)
{
	m_cost = a_cost;
}

bool Save::FileSave()
{
	std::ifstream file;
	std::string line;
	file.open(kSaveFilePath);
	if (!file.is_open())
	{
		file.close();
		return false;
	}

	std::string outString;
	while (std::getline(file, line))
	{
		if (line == "[Cost]")
		{
			outString += "[Cost]\n";
			SaveCost(file, outString);
		}
		else if (line == "[PowerUp]")
		{
			outString += "[PowerUp]\n";
			SaveLevel(file, outString);
		}
		else
		{
			outString += "\n";
		}
	}

	file.close();

	std::ofstream out(kSaveFilePath, std::ios::trunc);
	if (!out.is_open())
	{
		out.close();
		return false;
	}
	out.write(outString.c_str(), outString.size());
	out.close();
	return true;
}

int Save::GetLevel(std::string key)
{
	if (m_levels.find(key) != m_levels.end())
	{
		return m_levels.at(key);
	}
	// 見つからなかった場合は0レベルを返す
	return 0;
}

bool Save::UpdatePowerUp(std::string key, int a_level)
{
	if (m_levels.find(key) != m_levels.end())
	{
		m_levels.at(key) = a_level;
		return true;
	}
	// 見つからなかった場合はfalse
	return false;
}

bool Save::LoadData()
{
	std::ifstream file;
	std::string line;

	// ファイルが開けなかったら失敗
	file.open(kSaveFilePath);
	if (!file.is_open())
	{
		file.close();
		return false;
	}
	// ファイルを走査
	while (std::getline(file, line))
	{
		if (line == "[Cost]")
		{
			LoadCost(file);
		}
		else if (line == "[PowerUp]")
		{
			LoadLevel(file);
		}

	}

	file.close();
	return true;
}

void Save::LoadCost(std::ifstream& file)
{
	// 取得したデータを入れる変数
	std::string data;

	std::getline(file, data);

	if (!data.starts_with("cost")) return;

	auto startPos = data.find('=') + 1;

	if (startPos == std::string::npos) return;

	int cost = std::stoi(data.substr(startPos));

	m_cost = cost;
	
}

void Save::LoadLevel(std::ifstream& file)
{
	// 取得したデータを入れる変数
	std::string data;

	while (std::getline(file, data))
	{
		if (!data.starts_with("entry")) break;

		auto startPos = data.find('=') + 1;
		auto checkPos = data.find(',');

		if (startPos == std::string::npos || checkPos == std::string::npos) continue;

		std::string name = data.substr(startPos, checkPos - startPos);
		int level = std::stoi(data.substr(checkPos + 1));

		m_levels.insert({ name, level });
	}
}

void Save::SaveCost(std::ifstream& file, std::string& data)
{
	std::string dataCmp;
	while (std::getline(file, dataCmp))
	{
		if (!dataCmp.starts_with("cost"))
		{
			data.push_back('\n');
			break;
		}

		auto startPos = dataCmp.find('=');
		if (startPos == std::string::npos) continue;
		startPos += 1;
		
		dataCmp.erase(startPos);
		std::string cost = std::to_string(m_cost) + "\n";
		dataCmp.insert(startPos, cost);
		data += dataCmp;
	}
}

void Save::SaveLevel(std::ifstream& file, std::string& data)
{
	std::string dataCmp;
	while (std::getline(file, dataCmp))
	{
		if (!dataCmp.starts_with("entry"))
		{
			data.push_back('\n');
			break;
		}

		auto startPos = dataCmp.find('=');
		auto checkPos = dataCmp.find(',');

		if (startPos == std::string::npos || checkPos == std::string::npos) continue;
		startPos += 1;

		std::string name = dataCmp.substr(startPos, checkPos - startPos);

		if (m_levels.find(name) == m_levels.end()) continue;

		dataCmp.erase(checkPos + 1);
		std::string level = std::to_string(m_levels.at(name)) + "\n";
		dataCmp.insert(checkPos + 1, level);
		data += (dataCmp);
	}
}

