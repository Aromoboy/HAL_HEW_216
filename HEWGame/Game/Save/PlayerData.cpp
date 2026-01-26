#include "PlayerData.h"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string_view>

#include "Debug.h"

namespace
{
	constexpr const char* kPlayerDataPath = "Assets/Data/PlayerData.cfg";
	constexpr const char* kLogPrefix = "[PlayerData] ";
	constexpr const char* kSectionHeader = "[PlayerData]";

	std::string_view TrimView(std::string_view value)
	{
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
		{
			value.remove_prefix(1);
		}
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
		{
			value.remove_suffix(1);
		}
		return value;
	}

	std::vector<std::string_view> SplitCsv(std::string_view value)
	{
		std::vector<std::string_view> result;
		std::size_t start = 0;
		while (start <= value.size())
		{
			const std::size_t comma = value.find(',', start);
			if (comma == std::string_view::npos)
			{
				result.push_back(TrimView(value.substr(start)));
				break;
			}
			result.push_back(TrimView(value.substr(start, comma - start)));
			start = comma + 1;
		}
		return result;
	}

	bool TryParseInt(std::string_view token, int& outValue)
	{
		const std::string trimmed{ TrimView(token) };
		if (trimmed.empty())
		{
			return false;
		}

		char* end = nullptr;
		errno = 0;
		const long parsed = std::strtol(trimmed.c_str(), &end, 10);
		if (end == trimmed.c_str() || *end != '\0' || errno == ERANGE)
		{
			return false;
		}
		outValue = static_cast<int>(parsed);
		return true;
	}

	void LogParseError(std::size_t lineNumber, const std::string& message)
	{
		DebugLog::DebugMessage(std::string(kLogPrefix) + "Line " + std::to_string(lineNumber) + ": " + message + "\n");
	}

	void ApplyEffects(PlayerData::Data& entry)
	{
		entry.fSpeed = 0.0f;
		entry.creamEnabled = false;
		entry.fCakeSize = 0.0f;
		entry.fReplenishSpeed = 0.0f;
		entry.nReplenishCount = 0;
		entry.fPtMultipl = 1.0f;

		for (const auto& item : PowerUpEffectConfig::Entries())
		{
			const auto it = entry.levels.find(item.type);
			const int level = (it != entry.levels.end()) ? it->second : 0;
			const PowerUpEffectResult effect = PowerUpEffectConfig::ComputeEffect(item.type, level);

			switch (item.type)
			{
			case PowerUpType::SpeedUp:
				entry.fSpeed = effect.value;
				break;
			case PowerUpType::SweetsSpawnSpeed:
				entry.fReplenishSpeed = effect.value;
				break;
			case PowerUpType::SweetsSpawnQuantity:
				entry.nReplenishCount = static_cast<int>(effect.value);
				break;
			case PowerUpType::CakeSize:
				entry.fCakeSize = effect.value;
				break;
			case PowerUpType::PointAmount:
				entry.fPtMultipl = effect.value;
				break;
			case PowerUpType::Whip:
				entry.creamEnabled = effect.enabled;
				break;
			case PowerUpType::Unknown:
			default:
				break;
			}
		}
	}

	bool TryParseLevelPair(std::string_view token, PowerUpType& outType, int& outLevel)
	{
		const std::string_view trimmed = TrimView(token);
		const std::size_t separator = trimmed.find('=');
		if (separator == std::string_view::npos)
		{
			return false;
		}

		const std::string_view key = TrimView(trimmed.substr(0, separator));
		const std::string_view levelValue = TrimView(trimmed.substr(separator + 1));
		const auto entry = PowerUpEffectConfig::FindByKey(key);
		if (!entry)
		{
			return false;
		}
		outType = entry->type;
		return TryParseInt(levelValue, outLevel);
	}
}

bool PlayerData::LoadData()
{
	m_entries.clear();

	std::ifstream file(kPlayerDataPath);
	if (!file.is_open())
	{
		DebugLog::DebugMessage(std::string(kLogPrefix) + "Failed to open '" + kPlayerDataPath + "'.\n");
		return false;
	}

	bool inSection = false;
	bool hadError = false;
	bool anyEntry = false;
	std::string line;
	std::size_t lineNumber = 0;

	while (std::getline(file, line))
	{
		++lineNumber;
		const std::string_view trimmed = TrimView(line);
		if (trimmed.empty() || trimmed.starts_with('#') || trimmed.starts_with(';'))
		{
			continue;
		}

		if (trimmed.front() == '[')
		{
			inSection = (trimmed == kSectionHeader);
			continue;
		}

		if (!inSection)
		{
			continue;
		}

		if (!trimmed.starts_with("entry"))
		{
			continue;
		}

		const std::size_t equalsPos = trimmed.find('=');
		if (equalsPos == std::string_view::npos)
		{
			LogParseError(lineNumber, "Missing '=' for entry.");
			hadError = true;
			continue;
		}

		const std::string_view payload = TrimView(trimmed.substr(equalsPos + 1));
		if (payload.empty())
		{
			LogParseError(lineNumber, "Entry payload is empty.");
			hadError = true;
			continue;
		}

		const std::vector<std::string_view> fields = SplitCsv(payload);
		if (fields.size() < 2)
		{
			LogParseError(lineNumber, "Entry must have currency and level fields.");
			hadError = true;
			continue;
		}

		Data entry{};
		if (!TryParseInt(fields[0], entry.nPoints))
		{
			LogParseError(lineNumber, "Failed to parse currency.");
			hadError = true;
			continue;
		}

		bool hasLevel = false;
		for (std::size_t index = 1; index < fields.size(); ++index)
		{
			PowerUpType type = PowerUpType::Unknown;
			int level = 0;
			if (!TryParseLevelPair(fields[index], type, level))
			{
				LogParseError(lineNumber, "Failed to parse power-up level entry.");
				hadError = true;
				continue;
			}
			entry.levels[type] = level;
			hasLevel = true;
		}

		if (!hasLevel)
		{
			LogParseError(lineNumber, "Entry must contain at least one power-up level.");
			hadError = true;
			continue;
		}

		ApplyEffects(entry);
		m_entries.push_back(entry);
		anyEntry = true;
	}

	if (!anyEntry)
	{
		DebugLog::DebugMessage(std::string(kLogPrefix) + "No player data entries found.\n");
	}

	return !hadError;
}

bool PlayerData::SaveData() const
{
	std::ofstream out(kPlayerDataPath, std::ios::trunc);
	if (!out.is_open())
	{
		DebugLog::DebugMessage(std::string(kLogPrefix) + "Failed to write '" + kPlayerDataPath + "'.\n");
		return false;
	}

	out << kSectionHeader << "\n";
	for (const auto& entry : m_entries)
	{
		out << "entry=" << entry.nPoints;
		for (const auto& powerUpEntry : PowerUpEffectConfig::Entries())
		{
			const auto it = entry.levels.find(powerUpEntry.type);
			const int level = (it != entry.levels.end()) ? it->second : 0;
			out << "," << powerUpEntry.key << "=" << level;
		}
		out << "\n";
	}

	return true;
}

const std::vector<PlayerData::Data>& PlayerData::GetEntries() const
{
	return m_entries;
}

void PlayerData::SetEntries(const std::vector<Data>& entries)
{
	m_entries = entries;
}

void PlayerData::SetEntries(std::vector<Data>&& entries)
{
	m_entries = std::move(entries);
}
