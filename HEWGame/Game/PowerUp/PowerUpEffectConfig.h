#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>


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

enum class PowerUpEffectCurveType
{
	Levels,
	Linear,
	Toggle
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
		PowerUpEffectCurveType curveType = PowerUpEffectCurveType::Levels;
		std::string key;
		std::vector<float> levels;
		float baseValue = 0.0f;
		float perLevel = 0.0f;
	};

	static std::optional<Entry> FindByKey(std::string_view key);
	static PowerUpType FromKey(std::string_view key);
	static PowerUpEffectType GetEffectType(PowerUpType type);
	static PowerUpEffectResult ComputeEffect(PowerUpType type, int level);
	static const std::vector<Entry>& Entries();
	static const char* ToKey(PowerUpType type);
	static bool LoadFromConfig(const std::string& path, const std::string& section);
};
