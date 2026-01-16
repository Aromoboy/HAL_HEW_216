#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Stage/StageData.h"

namespace Stage
{
    struct StageConfigParseResult;

    struct StageConfigNormalized
    {
        float planeZ = 0.0f;
        std::vector<PlatformDescriptor> platforms;
        std::vector<float> phaseThresholds;
        std::vector<float> phaseRewardMultipliers;
        std::vector<float> phaseMiniThresholds;
        std::vector<float> phaseRewardMiniMultipliers;
        std::vector<float3> spawnPointsPrimary;
        std::vector<float3> spawnPointsSecondary;
        float cellSizeWorld = 0.0f;
        float playerHorizontalSpeedCellsPerSec = 0.0f;
        float playerVerticalSpeedCellsPerSec = 0.0f;
        float playerHorizontalSpeedWorld = 0.0f;
        float playerVerticalSpeedWorld = 0.0f;
        std::string backgroundTexturePath;
        std::optional<float> backgroundSafeScrollY;

        bool parseError = false;
        bool primaryUsedDefaults = false;
        bool secondaryUsedDefaults = false;
        bool primaryProvided = false;
        bool secondaryProvided = false;
        bool primaryParseError = false;
        bool secondaryParseError = false;
        bool insertedFallbackAnchor = false;
    };

    struct StageConfigDefaults
    {
        static constexpr float kDefaultPlaneZ = -5.0f;
        static constexpr float kDefaultPlatformCenterY = 0.0f;
        static constexpr float kDefaultPlatformWidthFraction = 0.20f;
        static constexpr float kDefaultPlatformHeightFraction = 0.05f;
        static constexpr float kPlatformGridWidthCells = 40.0f;
        static constexpr float kPlatformGridHeightCells = 30.0f;
        static constexpr float kDefaultPlatformDepth = 4.0f;
        static constexpr float kDefaultCellSizeWorld = 1.0f;
        static constexpr float kDefaultHorizontalSpeedCellsPerSec = 10.0f;
        static constexpr float kDefaultVerticalSpeedCellsPerSec = 12.0f;

        static float DefaultPlayerMoveSpeed();
        static const float3& DefaultPlatformScale();
        static const DirectX::XMFLOAT4& DefaultPlatformPrimaryColor();
        static const DirectX::XMFLOAT4& DefaultPlatformSecondaryColor();
        static const std::string& DefaultBackgroundTexturePath();
        static const std::vector<float>& DefaultThresholds();
        static const std::vector<float>& DefaultRewardMultipliers();
        static const std::vector<float>& DefaultMiniThresholds();
        static const std::vector<float>& DefaultMiniRewardMultipliers();
        static std::vector<float3> BuildDefaultSpawnPointsPrimary(float planeZ);
        static std::vector<float3> BuildDefaultSpawnPointsSecondary(float planeZ);
    };

    StageConfigNormalized NormalizeStageConfig(const StageConfigParseResult& parsed);
    StageConfigNormalized BuildDefaultStageConfig();
    PlatformDescriptor BuildDefaultPlatformDescriptor();
    void EnsureRewardMultiplierCoverage(std::vector<float>& multipliers, std::size_t requiredCount,
        const float* defaultMultipliers);
}
