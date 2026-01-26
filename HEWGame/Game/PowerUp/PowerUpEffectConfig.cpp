#include "PowerUpEffectConfig.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "ConfigSectionReader.h"
#include "Debug.h"
#include "GameplayConfig.h"

namespace
{
	struct KnownPowerUp
	{
		PowerUpType type;
		const char* key;
	};

	constexpr std::array<KnownPowerUp, 6> kKnownPowerUps = {{
		{ PowerUpType::SpeedUp, "SpeedUp" },
		{ PowerUpType::CakeSize, "CakeSize" },
		{ PowerUpType::SweetsSpawnSpeed, "SweetsSpawnSpeed" },
		{ PowerUpType::SweetsSpawnQuantity, "SweetsSpawnQuantity" },
		{ PowerUpType::PointAmount, "PointAmount" },
		{ PowerUpType::Whip, "Whip" },
	}};

	constexpr const char* kLogPrefix = "[PowerUpEffectConfig] ";

	std::vector<PowerUpEffectConfig::Entry> s_powerUpEntries;
	bool s_powerUpEntriesLoaded = false;

	std::vector<std::string> SplitCsv(const std::string& value)
	{
		std::vector<std::string> tokens;
		std::string token;
		std::stringstream ss(value);
		while (std::getline(ss, token, ','))
		{
			token = Config::Trim(token);
			if (!token.empty())
			{
				tokens.push_back(token);
			}
		}
		return tokens;
	}

	bool TryParseFloat(const std::string& token, float& outValue)
	{
		const std::string trimmed = Config::Trim(token);
		if (trimmed.empty())
		{
			return false;
		}

		char* end = nullptr;
		const float value = std::strtof(trimmed.c_str(), &end);
		if (end == trimmed.c_str() || *end != '\0')
		{
			return false;
		}

		outValue = value;
		return true;
	}

	std::optional<PowerUpEffectType> ParseEffectType(const std::string& token)
	{
		const std::string lowered = Config::ToLowerCopy(token);
		if (lowered == "additivepercent")
		{
			return PowerUpEffectType::AdditivePercent;
		}
		if (lowered == "additiveabsolute")
		{
			return PowerUpEffectType::AdditiveAbsolute;
		}
		if (lowered == "multiplicativefinal")
		{
			return PowerUpEffectType::MultiplicativeFinal;
		}
		if (lowered == "featuretoggle")
		{
			return PowerUpEffectType::FeatureToggle;
		}
		return std::nullopt;
	}

	std::optional<PowerUpEffectCurveType> ParseCurveType(const std::string& token)
	{
		const std::string lowered = Config::ToLowerCopy(token);
		if (lowered == "levels")
		{
			return PowerUpEffectCurveType::Levels;
		}
		if (lowered == "linear")
		{
			return PowerUpEffectCurveType::Linear;
		}
		if (lowered == "toggle")
		{
			return PowerUpEffectCurveType::Toggle;
		}
		return std::nullopt;
	}

	void LogParseError(std::size_t lineNumber, const std::string& message)
	{
		DebugLog::DebugMessage(std::string(kLogPrefix) + "Line " + std::to_string(lineNumber) + ": " + message + "\n");
	}

	void EnsureLoaded()
	{
		if (s_powerUpEntriesLoaded)
		{
			return;
		}

		s_powerUpEntriesLoaded = PowerUpEffectConfig::LoadFromConfig(
			GameplayConfig::kConfigPath,
			GameplayConfig::kSectionPowerUpEffects);
		if (!s_powerUpEntriesLoaded)
		{
			DebugLog::DebugMessage(std::string(kLogPrefix)
				+ "Failed to load power-up effect config from '"
				+ GameplayConfig::kConfigPath
				+ "' (section '" + GameplayConfig::kSectionPowerUpEffects + "').\n");
		}
	}

	int ClampLevelIndex(int level, std::size_t size)
	{
		if (size == 0)
		{
			return 0;
		}
		return std::clamp(level, 0, static_cast<int>(size) - 1);
	}
}

std::optional<PowerUpEffectConfig::Entry> PowerUpEffectConfig::FindByKey(std::string_view key)
{
	EnsureLoaded();
	const auto it = std::find_if(s_powerUpEntries.begin(), s_powerUpEntries.end(), [key](const Entry& entry)
		{
			return entry.key == key;
		});

	if (it == s_powerUpEntries.end())
	{
		return std::nullopt;
	}

	return *it;
}

PowerUpType PowerUpEffectConfig::FromKey(std::string_view key)
{
	const auto it = std::find_if(kKnownPowerUps.begin(), kKnownPowerUps.end(), [key](const KnownPowerUp& entry)
		{
			return entry.key == key;
		});

	if (it == kKnownPowerUps.end())
	{
		return PowerUpType::Unknown;
	}

	return it->type;
}

PowerUpEffectType PowerUpEffectConfig::GetEffectType(PowerUpType type)
{
	EnsureLoaded();
	const auto it = std::find_if(s_powerUpEntries.begin(), s_powerUpEntries.end(), [type](const Entry& entry)
		{
			return entry.type == type;
		});

	if (it == s_powerUpEntries.end())
	{
		return PowerUpEffectType::AdditivePercent;
	}

	return it->effectType;
}

PowerUpEffectResult PowerUpEffectConfig::ComputeEffect(PowerUpType type, int level)
{
	EnsureLoaded();
	const auto it = std::find_if(s_powerUpEntries.begin(), s_powerUpEntries.end(), [type](const Entry& entry)
		{
			return entry.type == type;
		});

	PowerUpEffectResult result;
	result.effectType = GetEffectType(type);

	if (it == s_powerUpEntries.end())
	{
		return result;
	}

	const Entry& entry = *it;
	switch (entry.curveType)
	{
	case PowerUpEffectCurveType::Levels:
	{
		const int clampedLevel = ClampLevelIndex(level, entry.levels.size());
		if (!entry.levels.empty())
		{
			result.value = entry.levels[clampedLevel];
		}
		break;
	}
	case PowerUpEffectCurveType::Linear:
		result.value = entry.baseValue + entry.perLevel * static_cast<float>(level);
		break;
	case PowerUpEffectCurveType::Toggle:
		result.enabled = (level > 0);
		result.value = result.enabled ? 1.0f : 0.0f;
		break;
	default:
		break;
	}

	if (entry.effectType != PowerUpEffectType::FeatureToggle)
	{
		result.enabled = true;
	}

	return result;
}

const std::vector<PowerUpEffectConfig::Entry>& PowerUpEffectConfig::Entries()
{
	EnsureLoaded();
	return s_powerUpEntries;
}

const char* PowerUpEffectConfig::ToKey(PowerUpType type)
{
	EnsureLoaded();
	const auto it = std::find_if(s_powerUpEntries.begin(), s_powerUpEntries.end(), [type](const Entry& entry)
		{
			return entry.type == type;
		});

	if (it == s_powerUpEntries.end())
	{
		return "";
	}

	return it->key.c_str();
}

bool PowerUpEffectConfig::LoadFromConfig(const std::string& path, const std::string& section)
{
	s_powerUpEntries.clear();

	std::vector<Config::SectionLine> lines;
	if (!Config::ReadSectionLines(path, section, lines))
	{
		return false;
	}

	for (const auto& line : lines)
	{
		if (line.key != "entry")
		{
			continue;
		}

		const std::vector<std::string> tokens = SplitCsv(line.value);
		if (tokens.size() < 3)
		{
			LogParseError(line.lineNumber, "Entry must provide key, effect type, and curve.");
			continue;
		}

		const std::string& key = tokens[0];
		const PowerUpType type = FromKey(key);
		if (type == PowerUpType::Unknown)
		{
			LogParseError(line.lineNumber, "Unknown power-up key '" + key + "'.");
			continue;
		}

		const auto effectType = ParseEffectType(tokens[1]);
		if (!effectType)
		{
			LogParseError(line.lineNumber, "Unknown effect type '" + tokens[1] + "'.");
			continue;
		}

		const auto curveType = ParseCurveType(tokens[2]);
		if (!curveType)
		{
			LogParseError(line.lineNumber, "Unknown curve type '" + tokens[2] + "'.");
			continue;
		}

		Entry entry{};
		entry.type = type;
		entry.effectType = *effectType;
		entry.curveType = *curveType;
		entry.key = key;

		bool parseFailed = false;
		switch (*curveType)
		{
		case PowerUpEffectCurveType::Levels:
		{
			if (tokens.size() < 4)
			{
				LogParseError(line.lineNumber, "Levels curve requires at least one value.");
				parseFailed = true;
				break;
			}
			for (std::size_t index = 3; index < tokens.size(); ++index)
			{
				float value = 0.0f;
				if (!TryParseFloat(tokens[index], value))
				{
					LogParseError(line.lineNumber, "Failed to parse level value '" + tokens[index] + "'.");
					parseFailed = true;
					break;
				}
				entry.levels.push_back(value);
			}
			break;
		}
		case PowerUpEffectCurveType::Linear:
		{
			if (tokens.size() < 5)
			{
				LogParseError(line.lineNumber, "Linear curve requires base and per-level values.");
				parseFailed = true;
				break;
			}
			if (!TryParseFloat(tokens[3], entry.baseValue) || !TryParseFloat(tokens[4], entry.perLevel))
			{
				LogParseError(line.lineNumber, "Failed to parse linear curve values.");
				parseFailed = true;
			}
			break;
		}
		case PowerUpEffectCurveType::Toggle:
			break;
		default:
			break;
		}

		if (parseFailed)
		{
			continue;
		}

		s_powerUpEntries.push_back(std::move(entry));
	}

	return !s_powerUpEntries.empty();
}
