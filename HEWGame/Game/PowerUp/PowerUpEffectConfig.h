#pragma once

#include <array>
#include <optional>
#include <string_view>


enum class PowerUpType
{
	SpeedUp,
	SweetsSpawnSpeed,
	SweetsSpawnQuantity,
	CakeSize,
	PointAmount,
	Whip,
	Unknown
};

enum class PowerUpEffectType
{
	AdditivePercent,
	AdditiveAbsolute,
	MultiplicativeFinal,
	FeatureToggle
};

struct PowerUpEffectResult
{
	PowerUpEffectType effectType = PowerUpEffectType::AdditivePercent;
	float value = 0.0f;
	bool enabled = false;
};

struct PowerUpEffectConfig
{
	struct Entry
	{
		PowerUpType type = PowerUpType::Unknown;
		PowerUpEffectType effectType = PowerUpEffectType::AdditivePercent;
		const char* key = "";
	};

	static std::optional<Entry> FindByKey(std::string_view key);
	static PowerUpType FromKey(std::string_view key);
	static PowerUpEffectType GetEffectType(PowerUpType type);
	static PowerUpEffectResult ComputeEffect(PowerUpType type, int level);
	static const std::array<Entry, 6>& Entries();
	static const char* ToKey(PowerUpType type);
};
