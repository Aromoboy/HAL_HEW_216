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
        std::string backgroundTexturePath;
        bool backgroundTextureSet = false;
        float backgroundSafeScrollY = 0.0f;
        bool backgroundSafeScrollYSet = false;
    };

    StageConfigParseResult ParseStageConfig(const std::vector<Config::SectionLine>& lines);
}
