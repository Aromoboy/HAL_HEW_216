#include "PowerUpEffectConfig.h"

#include <algorithm>
#include <array>

namespace
{
	constexpr std::array<PowerUpEffectConfig::Entry, 6> kPowerUpEntries = {{
		{ PowerUpType::SpeedUp, PowerUpEffectType::AdditivePercent, "SpeedUp" },
		{ PowerUpType::CakeSize, PowerUpEffectType::AdditivePercent, "CakeSize" },
		{ PowerUpType::SweetsSpawnSpeed, PowerUpEffectType::AdditivePercent, "SweetsSpawnSpeed" },
		{ PowerUpType::SweetsSpawnQuantity, PowerUpEffectType::AdditiveAbsolute, "SweetsSpawnQuantity" },
		{ PowerUpType::PointAmount, PowerUpEffectType::MultiplicativeFinal, "PointAmount" },
		{ PowerUpType::Whip, PowerUpEffectType::FeatureToggle, "Whip" },
	}};

	constexpr std::array<float, 6> kAdditivePercentLevels = {{ 0.0f, 0.10f, 0.20f, 0.30f, 0.40f, 0.50f }};
	constexpr std::array<float, 6> kReplenishCountLevels = {{ 0.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f }};
	constexpr std::array<float, 6> kScoreMultiplierLevels = {{ 1.0f, 1.10f, 1.20f, 1.40f, 1.60f, 1.80f }};

	int ClampLevelIndex(int level)
	{
		return std::clamp(level, 0, static_cast<int>(kAdditivePercentLevels.size()) - 1);
	}
}

std::optional<PowerUpEffectConfig::Entry> PowerUpEffectConfig::FindByKey(std::string_view key)
{
	const auto it = std::find_if(kPowerUpEntries.begin(), kPowerUpEntries.end(), [key](const Entry& entry)
		{
			return entry.key == key;
		});

	if (it == kPowerUpEntries.end())
	{
		return std::nullopt;
	}

	return *it;
}

PowerUpType PowerUpEffectConfig::FromKey(std::string_view key)
{
	const auto entry = FindByKey(key);
	return entry ? entry->type : PowerUpType::Unknown;
}

PowerUpEffectType PowerUpEffectConfig::GetEffectType(PowerUpType type)
{
	const auto it = std::find_if(kPowerUpEntries.begin(), kPowerUpEntries.end(), [type](const Entry& entry)
		{
			return entry.type == type;
		});

	if (it == kPowerUpEntries.end())
	{
		return PowerUpEffectType::AdditivePercent;
	}

	return it->effectType;
}

PowerUpEffectResult PowerUpEffectConfig::ComputeEffect(PowerUpType type, int level)
{
	const int clampedLevel = ClampLevelIndex(level);
	PowerUpEffectResult result;
	result.effectType = GetEffectType(type);

	switch (type)
	{
	case PowerUpType::SpeedUp:
	case PowerUpType::SweetsSpawnSpeed:
	case PowerUpType::CakeSize:
		result.value = kAdditivePercentLevels[clampedLevel];
		break;
	case PowerUpType::SweetsSpawnQuantity:
		result.value = kReplenishCountLevels[clampedLevel];
		break;
	case PowerUpType::PointAmount:
		result.value = kScoreMultiplierLevels[clampedLevel];
		break;
	case PowerUpType::Whip:
		result.enabled = (level > 0);
		result.value = result.enabled ? 1.0f : 0.0f;
		break;
	case PowerUpType::Unknown:
	default:
		result.value = 0.0f;
		break;
	}

	if (result.effectType != PowerUpEffectType::FeatureToggle)
	{
		result.enabled = true;
	}

	return result;
}

const std::array<PowerUpEffectConfig::Entry, 6>& PowerUpEffectConfig::Entries()
{
	return kPowerUpEntries;
}

const char* PowerUpEffectConfig::ToKey(PowerUpType type)
{
	const auto it = std::find_if(kPowerUpEntries.begin(), kPowerUpEntries.end(), [type](const Entry& entry)
		{
			return entry.type == type;
		});

	if (it == kPowerUpEntries.end())
	{
		return "";
	}

	return it->key;
}
