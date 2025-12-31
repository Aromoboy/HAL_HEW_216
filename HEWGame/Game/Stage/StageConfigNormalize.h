// ステージ設定の値を整えて扱いやすくする
// ステージ読み込みの後処理として使われる

#pragma once

#include <limits>
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
        float pickupBoxWidthCells = 0.0f;
        float pickupBoxHeightCells = 0.0f;
        float pickupBoxWidthWorld = 0.0f;
        float pickupBoxHeightWorld = 0.0f;
        float pickupMaxDistance = 0.0f;
        float pickupDepthThreshold = 0.0f;
        PickupOffset pickupOffset{};
        float guidelineHeight = 0.0f;
        float guidelineStep = 0.0f;
        float guidelineThicknessPixels = 0.0f;
        float guidelineWidthPaddingWorld = 0.0f;
        float guidelineZOffset = 0.0f;
        std::string backgroundTexturePath;
        float heightBonusThreshold = 0.0f;
        float heightBonusSeconds = 0.0f;

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
        static constexpr float kDefaultPickupBoxWidthCells = 5.0f;
        static constexpr float kDefaultPickupBoxHeightCells = 5.0f;
        static constexpr float kDefaultPickupMaxDistance = 0.5f;
        static constexpr float kDefaultPickupDepthThreshold = std::numeric_limits<float>::infinity();
        static constexpr Stage::PickupOffsetMode kDefaultPickupOffsetMode = Stage::PickupOffsetMode::WorldX;
        static constexpr float kDefaultGuidelineHeight = 2.0f;
        static constexpr float kDefaultGuidelineStep = 1.0f;
        static constexpr float kDefaultGuidelineThicknessPixels = 3.0f;
        static constexpr float kDefaultGuidelineZOffset = 0.1f;
        static constexpr float kDefaultHeightBonusThreshold = kDefaultGuidelineHeight;
        static constexpr float kDefaultHeightBonusSeconds = 5.0f;

        static float DefaultGuidelineWidthPaddingWorld();
        static float DefaultPlayerMoveSpeed();
        static const float3& DefaultPlatformScale();
        static const DirectX::XMFLOAT4& DefaultPlatformPrimaryColor();
        static const DirectX::XMFLOAT4& DefaultPlatformSecondaryColor();
        static const float3& DefaultPickupOffsetLocalCells();
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
