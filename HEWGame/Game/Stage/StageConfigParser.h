#pragma once

#include <string>
#include <vector>

#include "ConfigSectionReader.h"
#include "Stage/StageData.h"

namespace Stage
{
    struct PlatformOverride
    {
        PlatformDescriptor descriptor{};
        bool centerProvided = false;
        bool scaleProvided = false;
        bool depthProvided = false;
        bool widthProvided = false;
        bool heightProvided = false;
        bool colorProvided = false;
    };

    struct StageConfigParseResult
    {
        bool parseError = false;
        bool primaryProvided = false;
        bool secondaryProvided = false;
        bool primaryParseError = false;
        bool secondaryParseError = false;

        std::vector<float> thresholds;
        std::vector<float> minithresholds;
        std::vector<float3> spawnPointsPrimary;
        std::vector<float3> spawnPointsSecondary;
        std::vector<float> rewardMultipliers;
        std::vector<float> minirewardMultipliers;
        std::vector<PlatformOverride> platformOverrides;

        float planeZ = 0.0f;
        bool planeZSet = false;
        float platformCenterY = 0.0f;
        bool platformCenterSet = false;
        float platformWidthFraction = 0.0f;
        bool platformWidthFractionSet = false;
        float platformHeightFraction = 0.0f;
        bool platformHeightFractionSet = false;
        float platformDepth = 0.0f;
        bool platformDepthSet = false;
        float3 platformScale{ 0.0f, 0.0f, 0.0f };
        bool platformScaleSet = false;
        float cellSizeWorld = 0.0f;
        bool cellSizeWorldSet = false;
        float horizontalSpeedCellsPerSec = 0.0f;
        bool horizontalSpeedSet = false;
        float verticalSpeedCellsPerSec = 0.0f;
        bool verticalSpeedSet = false;
        float pickupBoxWidthCells = 0.0f;
        bool pickupBoxWidthSet = false;
        float pickupBoxHeightCells = 0.0f;
        bool pickupBoxHeightSet = false;
        float pickupMaxDistance = 0.0f;
        float pickupDepthThreshold = 0.0f;
        Stage::PickupOffsetMode pickupOffsetMode = Stage::PickupOffsetMode::WorldX;
        float3 pickupOffsetLocalCells{ 0.0f, 0.0f, 0.0f };
        float guidelineHeight = 0.0f;
        bool guidelineHeightSet = false;
        float guidelineStep = 0.0f;
        bool guidelineStepSet = false;
        float guidelineThicknessPixels = 0.0f;
        bool guidelineThicknessSet = false;
        float guidelineWidthPaddingWorld = 0.0f;
        bool guidelineWidthPaddingSet = false;
        float guidelineZOffset = 0.0f;
        bool guidelineZOffsetSet = false;
        float heightBonusThreshold = 0.0f;
        bool heightBonusThresholdSet = false;
        float heightBonusSeconds = 0.0f;
        bool heightBonusSecondsSet = false;
        std::string backgroundTexturePath;
        bool backgroundTextureSet = false;
    };

    StageConfigParseResult ParseStageConfig(const std::vector<Config::SectionLine>& lines);
    bool TryParsePickupOffsetMode(const std::string& rawValue, Stage::PickupOffsetMode& outMode);
    const char* PickupOffsetModeToString(Stage::PickupOffsetMode mode);
}
