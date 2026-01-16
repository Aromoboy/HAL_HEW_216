#include "Stage/StageConfigNormalize.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

#include "Stage/StageConfigParser.h"

namespace
{
    constexpr const char* kLogPrefix = "[StageData] ";
    constexpr std::size_t kRequiredSpawnAnchors = 2;
    constexpr float kSpawnPlaneTolerance = 0.01f;

    const float3 kDefaultPlatformScale{ 12.0f, 2.0f, 4.0f };
    const DirectX::XMFLOAT4 kDefaultPlatformPrimaryColor{ 0.305f, 0.173f, 0.125f, 1.0f };
    const DirectX::XMFLOAT4 kDefaultPlatformSecondaryColor{ 0.180f, 0.258f, 0.329f, 1.0f };
    const float3 kDefaultPickupOffsetLocalCells{ 0.5f, 0.0f, 0.0f };
    const std::string kDefaultBackgroundTexturePath = "Assets/Texture/background.png";
    const std::string kDefaultGuidelineModelPath = "Assets/Model/GuideLine.fbx";
    const float3 kDefaultGuidelineSpinAxis{ 0.0f, 1.0f, 0.0f };
    const float kDefaultThresholds[] = { 5.0f, 15.0f, 30.0f };
    const float kDefaultRewardMultipliers[] = { 1.0f };
    const float kDefaultMiniThresholds[] = { 3.0f, 7.0f, 16.0f };
    const float kDefaultMiniRewardMultipliers[] = { 1.0f };

    DirectX::XMFLOAT4 SanitizeColor(const DirectX::XMFLOAT4& color, const DirectX::XMFLOAT4& fallback, const std::string& label)
    {
        auto clampComponent = [](float value)
        {
            return std::clamp(value, 0.0f, 1.0f);
        };

        if (!std::isfinite(color.x) || !std::isfinite(color.y) || !std::isfinite(color.z) || !std::isfinite(color.w))
        {
            std::ostringstream oss;
            oss << kLogPrefix << label << " invalid; using fallback color.\n";
            return fallback;
        }

        DirectX::XMFLOAT4 sanitized{ clampComponent(color.x), clampComponent(color.y), clampComponent(color.z), clampComponent(color.w) };
        if (sanitized.x != color.x || sanitized.y != color.y || sanitized.z != color.z || sanitized.w != color.w)
        {
            std::ostringstream oss;
            oss << kLogPrefix << label << " clamped to (" << sanitized.x << ',' << sanitized.y << ',' << sanitized.z << ',' << sanitized.w << ")\n";
        }

        return sanitized;
    }
}

namespace Stage
{
    float StageConfigDefaults::DefaultGuidelineWidthPaddingWorld()
    {
        return (kDefaultPlatformWidthFraction * kPlatformGridWidthCells * kDefaultCellSizeWorld) * 0.5f;
    }

    float StageConfigDefaults::DefaultPlayerMoveSpeed()
    {
        return kDefaultHorizontalSpeedCellsPerSec * kDefaultCellSizeWorld;
    }

    const float3& StageConfigDefaults::DefaultPlatformScale()
    {
        return kDefaultPlatformScale;
    }

    const DirectX::XMFLOAT4& StageConfigDefaults::DefaultPlatformPrimaryColor()
    {
        return kDefaultPlatformPrimaryColor;
    }

    const DirectX::XMFLOAT4& StageConfigDefaults::DefaultPlatformSecondaryColor()
    {
        return kDefaultPlatformSecondaryColor;
    }

    const float3& StageConfigDefaults::DefaultPickupOffsetLocalCells()
    {
        return kDefaultPickupOffsetLocalCells;
    }

    const float3& StageConfigDefaults::DefaultGuidelineSpinAxis()
    {
        return kDefaultGuidelineSpinAxis;
    }

    const std::string& StageConfigDefaults::DefaultBackgroundTexturePath()
    {
        return kDefaultBackgroundTexturePath;
    }

    const std::string& StageConfigDefaults::DefaultGuidelineModelPath()
    {
        return kDefaultGuidelineModelPath;
    }

    const std::vector<float>& StageConfigDefaults::DefaultThresholds()
    {
        static const std::vector<float> defaults(std::begin(kDefaultThresholds), std::end(kDefaultThresholds));
        return defaults;
    }

    const std::vector<float>& StageConfigDefaults::DefaultRewardMultipliers()
    {
        static const std::vector<float> defaults(std::begin(kDefaultRewardMultipliers), std::end(kDefaultRewardMultipliers));
        return defaults;
    }

    const std::vector<float>& StageConfigDefaults::DefaultMiniThresholds()
    {
        static const std::vector<float> defaults(std::begin(kDefaultMiniThresholds), std::end(kDefaultMiniThresholds));
        return defaults;
    }

    const std::vector<float>& StageConfigDefaults::DefaultMiniRewardMultipliers()
    {
        static const std::vector<float> defaults(std::begin(kDefaultMiniRewardMultipliers), std::end(kDefaultMiniRewardMultipliers));
        return defaults;
    }

    std::vector<float3> StageConfigDefaults::BuildDefaultSpawnPointsPrimary(float planeZ)
    {
        return {
            { -8.0f, 0.0f, planeZ },
            { -8.0f, 0.0f, planeZ },
        };
    }

    std::vector<float3> StageConfigDefaults::BuildDefaultSpawnPointsSecondary(float planeZ)
    {
        return {
            { 8.0f, 0.0f, planeZ },
            { 8.0f, 0.0f, planeZ },
        };
    }

    PlatformDescriptor BuildDefaultPlatformDescriptor()
    {
        PlatformDescriptor descriptor{};
        descriptor.center = float3{ 0.0f, StageConfigDefaults::kDefaultPlatformCenterY, StageConfigDefaults::kDefaultPlaneZ };
        descriptor.scale = StageConfigDefaults::DefaultPlatformScale();
        descriptor.depth = StageConfigDefaults::kDefaultPlatformDepth;
        descriptor.widthFraction = StageConfigDefaults::kDefaultPlatformWidthFraction;
        descriptor.heightFraction = StageConfigDefaults::kDefaultPlatformHeightFraction;
        descriptor.widthWorld = StageConfigDefaults::kDefaultPlatformWidthFraction
            * StageConfigDefaults::kPlatformGridWidthCells * StageConfigDefaults::kDefaultCellSizeWorld;
        descriptor.heightWorld = StageConfigDefaults::kDefaultPlatformHeightFraction
            * StageConfigDefaults::kPlatformGridHeightCells * StageConfigDefaults::kDefaultCellSizeWorld;
        descriptor.depthWorld = StageConfigDefaults::kDefaultPlatformDepth * StageConfigDefaults::kDefaultCellSizeWorld;
        descriptor.color = StageConfigDefaults::DefaultPlatformPrimaryColor();
        return descriptor;
    }

    void EnsureRewardMultiplierCoverage(std::vector<float>& multipliers, std::size_t requiredCount,
        const float* defaultMultipliers)
    {
        if (requiredCount == 0)
        {
            requiredCount = 1;
        }

        const auto beforeFilter = multipliers.size();
        multipliers.erase(
            std::remove_if(multipliers.begin(), multipliers.end(), [](float value)
                {
                    return !std::isfinite(value) || value <= 0.0f;
                }),
            multipliers.end());
        if (beforeFilter != multipliers.size())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Filtered " << (beforeFilter - multipliers.size())
                << " invalid reward multipliers." << "\n";
        }

        if (multipliers.empty())
        {
            multipliers.assign(std::begin(kDefaultRewardMultipliers), std::end(kDefaultRewardMultipliers));
            std::ostringstream oss;
            oss << kLogPrefix << "No phaseRewardMultipliers provided; using defaults." << "\n";

        }

        if (multipliers.size() < requiredCount)
        {
            const float fillValue = multipliers.back();
            const auto previousSize = multipliers.size();
            multipliers.resize(requiredCount, fillValue);
            std::ostringstream oss;
            oss << kLogPrefix << "Extended reward multipliers from " << previousSize
                << " to " << multipliers.size() << " entries using fillValue=" << fillValue << ".\n";
        }
        else if (multipliers.size() > requiredCount)
        {
            multipliers.resize(requiredCount);
            std::ostringstream oss;
            oss << kLogPrefix << "Truncated reward multipliers to match phase count (" << requiredCount << ").\n";
        }
    }

    StageConfigNormalized NormalizeStageConfig(const StageConfigParseResult& parsed)
    {
        StageConfigNormalized normalized{};
        normalized.parseError = parsed.parseError;
        normalized.primaryProvided = parsed.primaryProvided;
        normalized.secondaryProvided = parsed.secondaryProvided;
        normalized.primaryParseError = parsed.primaryParseError;
        normalized.secondaryParseError = parsed.secondaryParseError;

        std::vector<float> thresholds = parsed.thresholds;
        std::vector<float> minithresholds = parsed.minithresholds;
        std::vector<float3> spawnPointsPrimary = parsed.spawnPointsPrimary;
        std::vector<float3> spawnPointsSecondary = parsed.spawnPointsSecondary;
        std::vector<float> rewardMultipliers = parsed.rewardMultipliers;
        std::vector<float> minirewardMultipliers = parsed.minirewardMultipliers;
        std::vector<PlatformOverride> platformOverrides = parsed.platformOverrides;
        float planeZ = parsed.planeZ;
        bool planeZSet = parsed.planeZSet;
        float platformCenterY = parsed.platformCenterY;
        bool platformCenterSet = parsed.platformCenterSet;
        float platformWidthFraction = parsed.platformWidthFraction;
        bool platformWidthFractionSet = parsed.platformWidthFractionSet;
        float platformHeightFraction = parsed.platformHeightFraction;
        bool platformHeightFractionSet = parsed.platformHeightFractionSet;
        float platformDepth = parsed.platformDepth;
        bool platformDepthSet = parsed.platformDepthSet;
        float3 platformScale = parsed.platformScale;
        bool platformScaleSet = parsed.platformScaleSet;
        float cellSizeWorld = parsed.cellSizeWorld;
        float horizontalSpeedCellsPerSec = parsed.horizontalSpeedCellsPerSec;
        bool verticalSpeedSet = parsed.verticalSpeedSet;
        float verticalSpeedCellsPerSec = parsed.verticalSpeedCellsPerSec;
        float pickupBoxWidthCells = parsed.pickupBoxWidthCells;
        float pickupBoxHeightCells = parsed.pickupBoxHeightCells;
        float pickupMaxDistance = parsed.pickupMaxDistance;
        float pickupDepthThreshold = parsed.pickupDepthThreshold;
        Stage::PickupOffsetMode pickupOffsetMode = parsed.pickupOffsetMode;
        float3 pickupOffsetLocalCells = parsed.pickupOffsetLocalCells;
        float guidelineHeight = parsed.guidelineHeight;
        float guidelineStep = parsed.guidelineStep;
        float guidelineThicknessPixels = parsed.guidelineThicknessPixels;
        float guidelineThicknessWorld = parsed.guidelineThicknessWorld;
        float guidelineWidthPaddingWorld = parsed.guidelineWidthPaddingWorld;
        float guidelineZOffset = parsed.guidelineZOffset;
        float3 guidelineSpinAxis = parsed.guidelineSpinAxis;
        float guidelineSpinSpeedDegPerSec = parsed.guidelineSpinSpeedDegPerSec;
        std::string guidelineModelPath = parsed.guidelineModelPath;
        float heightBonusThreshold = parsed.heightBonusThreshold;
        float heightBonusSeconds = parsed.heightBonusSeconds;
        std::string backgroundTexturePath = parsed.backgroundTexturePath;

        if (!planeZSet)
        {
            planeZ = StageConfigDefaults::kDefaultPlaneZ;
            std::ostringstream oss;
            oss << kLogPrefix << "planeZ not specified; defaulting to " << planeZ << "\n";
        }

        if (!platformCenterSet)
        {
            platformCenterY = StageConfigDefaults::kDefaultPlatformCenterY;
            std::ostringstream oss;
            oss << kLogPrefix << "platformCenterY not specified; defaulting to " << platformCenterY << "\n";
        }

        if (!platformScaleSet)
        {
            platformScale = StageConfigDefaults::DefaultPlatformScale();
            std::ostringstream oss;
            oss << kLogPrefix << "platformScale not specified; using default ("
                << platformScale.x << ',' << platformScale.y << ',' << platformScale.z << ").\n";
        }

        if (!parsed.guidelineHeightSet)
        {
            guidelineHeight = StageConfigDefaults::kDefaultGuidelineHeight;
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineHeight not specified; defaulting to " << guidelineHeight << "\n";
        }

        if (!parsed.guidelineStepSet)
        {
            guidelineStep = StageConfigDefaults::kDefaultGuidelineStep;
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineStep not specified; defaulting to " << guidelineStep << "\n";
        }
        if (!parsed.guidelineThicknessSet)
        {
            guidelineThicknessPixels = StageConfigDefaults::kDefaultGuidelineThicknessPixels;
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineThicknessPixels not specified; defaulting to "
                << guidelineThicknessPixels << "\n";
        }
        if (!parsed.guidelineThicknessWorldSet)
        {
            guidelineThicknessWorld = StageConfigDefaults::kDefaultGuidelineThicknessWorld;
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineThicknessWorld not specified; defaulting to "
                << guidelineThicknessWorld << "\n";
        }
        if (!parsed.guidelineWidthPaddingSet)
        {
            guidelineWidthPaddingWorld = StageConfigDefaults::DefaultGuidelineWidthPaddingWorld();
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineWidthPaddingWorld not specified; defaulting to "
                << guidelineWidthPaddingWorld << "\n";
        }
        if (!parsed.guidelineZOffsetSet)
        {
            guidelineZOffset = StageConfigDefaults::kDefaultGuidelineZOffset;
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineZOffset not specified; defaulting to "
                << guidelineZOffset << "\n";
        }

        if (!parsed.guidelineSpinAxisSet)
        {
            guidelineSpinAxis = StageConfigDefaults::DefaultGuidelineSpinAxis();
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineSpinAxis not specified; defaulting to ("
                << guidelineSpinAxis.x << ',' << guidelineSpinAxis.y << ',' << guidelineSpinAxis.z << ")\n";
        }

        if (!parsed.guidelineSpinSpeedSet)
        {
            guidelineSpinSpeedDegPerSec = StageConfigDefaults::kDefaultGuidelineSpinSpeedDegPerSec;
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineSpinSpeedDegPerSec not specified; defaulting to "
                << guidelineSpinSpeedDegPerSec << "\n";
        }

        if (!parsed.guidelineModelPathSet)
        {
            guidelineModelPath = StageConfigDefaults::DefaultGuidelineModelPath();
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineModelPath not specified; defaulting to '"
                << guidelineModelPath << "'.\n";
        }

        if (!parsed.heightBonusThresholdSet)
        {
            heightBonusThreshold = StageConfigDefaults::kDefaultHeightBonusThreshold;
            std::ostringstream oss;
            oss << kLogPrefix << "heightBonusThreshold not specified; defaulting to " << heightBonusThreshold << "\n";
        }

        if (!parsed.heightBonusSecondsSet)
        {
            heightBonusSeconds = StageConfigDefaults::kDefaultHeightBonusSeconds;
            std::ostringstream oss;
            oss << kLogPrefix << "heightBonusSeconds not specified; defaulting to " << heightBonusSeconds << "\n";
        }

        if (!parsed.backgroundTextureSet)
        {
            backgroundTexturePath = StageConfigDefaults::DefaultBackgroundTexturePath();
            std::ostringstream oss;
            oss << kLogPrefix << "backgroundTexture not specified; defaulting to '"
                << backgroundTexturePath << "'.\n";
        }

        if (!platformWidthFractionSet)
        {
            platformWidthFraction = StageConfigDefaults::kDefaultPlatformWidthFraction;
            std::ostringstream oss;
            oss << kLogPrefix << "platformWidthFraction not specified; defaulting to "
                << platformWidthFraction << "\n";
        }

        if (!platformHeightFractionSet)
        {
            platformHeightFraction = StageConfigDefaults::kDefaultPlatformHeightFraction;
            std::ostringstream oss;
            oss << kLogPrefix << "platformHeightFraction not specified; defaulting to "
                << platformHeightFraction << "\n";
        }

        if (!platformDepthSet)
        {
            platformDepth = StageConfigDefaults::kDefaultPlatformDepth;
            std::ostringstream oss;
            oss << kLogPrefix << "platformDepth not specified; defaulting to " << platformDepth << "\n";
        }

        auto sanitizeFraction = [](float value, float defaultValue, const char* name)
        {
            float sanitized = value;
            if (!std::isfinite(sanitized) || sanitized <= 0.0f)
            {
                sanitized = defaultValue;
            }
            bool treatedAsPercent = false;
            if (sanitized > 1.0f)
            {
                sanitized *= 0.01f;
                treatedAsPercent = true;
            }
            sanitized = std::clamp(sanitized, 0.01f, 1.0f);

            if (!std::isfinite(value) || value <= 0.0f)
            {
                std::ostringstream oss;
                oss << kLogPrefix << name << " invalid; using default " << defaultValue << "\n";
            }
            else if (treatedAsPercent)
            {
                std::ostringstream oss;
                oss << kLogPrefix << name << " treated as percent; normalized to " << sanitized << "\n";
            }
            else if (sanitized != value)
            {
                std::ostringstream oss;
                oss << kLogPrefix << name << " clamped to " << sanitized << " (from " << value << ")\n";
            }

            return sanitized;
        };

        platformWidthFraction = sanitizeFraction(platformWidthFraction, StageConfigDefaults::kDefaultPlatformWidthFraction, "platformWidthFraction");
        platformHeightFraction = sanitizeFraction(platformHeightFraction, StageConfigDefaults::kDefaultPlatformHeightFraction, "platformHeightFraction");

        auto sanitizeDepth = [](float value, float defaultValue, const std::string& name)
        {
            float sanitized = value;
            if (!std::isfinite(sanitized) || sanitized == 0.0f)
            {
                std::ostringstream oss;
                oss << kLogPrefix << name << " value " << sanitized
                    << " invalid; using default " << defaultValue << "\n";
                sanitized = defaultValue;
            }
            sanitized = std::max(0.01f, static_cast<float>(std::fabs(sanitized)));
            return sanitized;
        };

        auto sanitizeScaleVector = [](const float3& value, const std::string& name)
        {
            auto sanitizeScale = [](float component)
            {
                return std::max(0.01f, static_cast<float>(std::fabs(component)));
            };

            float3 sanitized = value;
            const float3 raw = sanitized;
            sanitized.x = sanitizeScale(sanitized.x);
            sanitized.y = sanitizeScale(sanitized.y);
            sanitized.z = sanitizeScale(sanitized.z);
            if (sanitized.x != raw.x || sanitized.y != raw.y || sanitized.z != raw.z)
            {
                std::ostringstream oss;
                oss << kLogPrefix << name << " values sanitized from ("
                    << raw.x << ',' << raw.y << ',' << raw.z << ") to ("
                    << sanitized.x << ',' << sanitized.y << ',' << sanitized.z << ").\n";
            }
            return sanitized;
        };

        auto sanitizeCenterComponent = [](float value, float defaultValue, const std::string& label)
        {
            if (!std::isfinite(value))
            {
                std::ostringstream oss;
                oss << kLogPrefix << label << " invalid; defaulting to " << defaultValue << "\n";
                return defaultValue;
            }
            return value;
        };

        platformDepth = sanitizeDepth(platformDepth, StageConfigDefaults::kDefaultPlatformDepth, "platformDepth");
        platformScale = sanitizeScaleVector(platformScale, "platformScale");

        auto defaultColorForIndex = [](std::size_t index)
        {
            return (index % 2 == 0) ? StageConfigDefaults::DefaultPlatformPrimaryColor() : StageConfigDefaults::DefaultPlatformSecondaryColor();
        };

        auto buildDefaultPlatform = [&](std::size_t index)
        {
            PlatformDescriptor descriptor{};
            descriptor.center = float3{ 0.0f, platformCenterY, planeZ };
            descriptor.scale = platformScale;
            descriptor.depth = platformDepth;
            descriptor.widthFraction = platformWidthFraction;
            descriptor.heightFraction = platformHeightFraction;
            descriptor.color = defaultColorForIndex(index);
            return descriptor;
        };

        auto sanitizePlatformDescriptor = [&](PlatformDescriptor descriptor, std::size_t index)
        {
            const std::string labelPrefix = std::string("platform") + std::to_string(index);
            descriptor.center.x = sanitizeCenterComponent(descriptor.center.x, 0.0f, labelPrefix + ".centerX");
            descriptor.center.y = sanitizeCenterComponent(descriptor.center.y, platformCenterY, labelPrefix + ".centerY");
            descriptor.center.z = sanitizeCenterComponent(descriptor.center.z, planeZ, labelPrefix + ".centerZ");

            descriptor.scale = sanitizeScaleVector(descriptor.scale, labelPrefix + ".scale");
            descriptor.depth = sanitizeDepth(descriptor.depth, platformDepth, labelPrefix + ".depth");

            const std::string widthLabel = labelPrefix + ".widthFraction";
            descriptor.widthFraction = sanitizeFraction(descriptor.widthFraction, platformWidthFraction, widthLabel.c_str());
            const std::string heightLabel = labelPrefix + ".heightFraction";
            descriptor.heightFraction = sanitizeFraction(descriptor.heightFraction, platformHeightFraction, heightLabel.c_str());
            descriptor.color = SanitizeColor(descriptor.color, defaultColorForIndex(index), labelPrefix + ".color");
            return descriptor;
        };

        std::vector<PlatformDescriptor> platformDescriptors;
        if (platformOverrides.empty())
        {
            platformDescriptors.push_back(sanitizePlatformDescriptor(buildDefaultPlatform(0), 0));
        }
        else
        {
            for (std::size_t i = 0; i < platformOverrides.size(); ++i)
            {
                PlatformDescriptor descriptor = buildDefaultPlatform(i);
                const PlatformOverride& override = platformOverrides[i];
                if (override.centerProvided)
                {
                    descriptor.center = override.descriptor.center;
                }
                if (override.scaleProvided)
                {
                    descriptor.scale = override.descriptor.scale;
                }
                if (override.depthProvided)
                {
                    descriptor.depth = override.descriptor.depth;
                }
                if (override.widthProvided)
                {
                    descriptor.widthFraction = override.descriptor.widthFraction;
                }
                if (override.heightProvided)
                {
                    descriptor.heightFraction = override.descriptor.heightFraction;
                }
                if (override.colorProvided)
                {
                    descriptor.color = override.descriptor.color;
                }

                platformDescriptors.push_back(sanitizePlatformDescriptor(descriptor, i));
            }
        }

        if (thresholds.empty())
        {
            thresholds = StageConfigDefaults::DefaultThresholds();
            std::ostringstream oss;
            oss << kLogPrefix << "No phaseThresholds provided; using defaults.\n";
        }

        std::sort(thresholds.begin(), thresholds.end());
        thresholds.erase(
            std::remove_if(thresholds.begin(), thresholds.end(), [](float value)
                {
                    return !std::isfinite(value) || value <= 0.0f;
                }),
            thresholds.end());

        thresholds.erase(std::unique(thresholds.begin(), thresholds.end()), thresholds.end());

        if (thresholds.empty())
        {
            thresholds = StageConfigDefaults::DefaultThresholds();
            std::ostringstream oss;
            oss << kLogPrefix << "All thresholds were invalid; falling back to defaults.\n";
        }
        //ミニフェーズ
        if (minithresholds.empty())
        {
            minithresholds = StageConfigDefaults::DefaultMiniThresholds();
            std::ostringstream oss;
            oss << kLogPrefix << "No phaseMiniThresholds provided; using defaults.\n";
        }

        std::sort(minithresholds.begin(), minithresholds.end());
        minithresholds.erase(
            std::remove_if(minithresholds.begin(), minithresholds.end(), [](float value)
                {
                    return !std::isfinite(value) || value <= 0.0f;
                }),
            minithresholds.end());

        minithresholds.erase(std::unique(minithresholds.begin(), minithresholds.end()), minithresholds.end());

        if (minithresholds.empty())
        {
            minithresholds = StageConfigDefaults::DefaultMiniThresholds();
            std::ostringstream oss;
            oss << kLogPrefix << "All minithresholds were invalid; falling back to defaults.\n";
        }

        const auto defaultSpawnPointsPrimary = StageConfigDefaults::BuildDefaultSpawnPointsPrimary(planeZ);
        const auto defaultSpawnPointsSecondary = StageConfigDefaults::BuildDefaultSpawnPointsSecondary(planeZ);

        auto sanitizeSpawnGroup = [&](std::vector<float3>& spawnPoints, const std::vector<float3>& defaults, const char* label, bool parseFailed, bool allowDefaults, bool& usedDefaults)
        {
            const std::size_t parsedAnchorCount = spawnPoints.size();
            usedDefaults = false;
            if (parsedAnchorCount > 0 && parsedAnchorCount < kRequiredSpawnAnchors)
            {
                normalized.parseError = true;
                parseFailed = true;
                std::ostringstream oss;
                oss << kLogPrefix << label << " provided " << parsedAnchorCount
                    << " anchor" << (parsedAnchorCount == 1 ? "" : "s")
                    << "; expected at least " << kRequiredSpawnAnchors
                    << ". Keeping lane empty to avoid silent defaults.\n";
                spawnPoints.clear();
            }

            if (spawnPoints.empty() && !parseFailed && allowDefaults)
            {
                spawnPoints = defaults;
                usedDefaults = true;
                std::ostringstream oss;
                oss << kLogPrefix << label << " missing; using defaults for this lane.\n";
            }

            if (!spawnPoints.empty())
            {
                const float spawnPlaneZ = spawnPoints.front().z;
                bool spawnPlaneConsistent = true;
                for (const auto& spawn : spawnPoints)
                {
                    if (std::fabs(spawn.z - spawnPlaneZ) > kSpawnPlaneTolerance)
                    {
                        spawnPlaneConsistent = false;
                        break;
                    }
                }

                if (!spawnPlaneConsistent)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << label << " contain multiple planeZ values; anchors may be misaligned.\n";
                }

                const float spawnPlaneDelta = std::fabs(spawnPlaneZ - planeZ);
                if (spawnPlaneDelta > kSpawnPlaneTolerance)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << label << " planeZ=" << spawnPlaneZ
                        << " differs from configured planeZ=" << planeZ
                        << " by " << spawnPlaneDelta << "; verify alignment.\n";
                }
            }
        };

        bool insertedFallbackAnchor = false;
        sanitizeSpawnGroup(
            spawnPointsPrimary,
            defaultSpawnPointsPrimary,
            "spawnPointsPrimary",
            normalized.primaryParseError,
            !normalized.primaryProvided,
            normalized.primaryUsedDefaults);
        sanitizeSpawnGroup(
            spawnPointsSecondary,
            defaultSpawnPointsSecondary,
            "spawnPointsSecondary",
            normalized.secondaryParseError,
            false,
            normalized.secondaryUsedDefaults);

        if (spawnPointsPrimary.empty() && spawnPointsSecondary.empty())
        {
            spawnPointsPrimary.push_back(float3{ 8.0f, -4.0f, 0.0f });
            spawnPointsSecondary.push_back(float3{ -8.0f, -4.0f, 0.0f });
            insertedFallbackAnchor = true;
            normalized.parseError = true;
            std::ostringstream oss;
            oss << kLogPrefix << "Both spawn lanes are empty; inserting single fallback anchor at (+-8,0,0).\n";
        }

        if (!spawnPointsPrimary.empty() && !spawnPointsSecondary.empty())
        {
            const float laneDelta = std::fabs(spawnPointsPrimary.front().z - spawnPointsSecondary.front().z);
            if (laneDelta > kSpawnPlaneTolerance)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "spawn point lanes use different planeZ values (primary="
                    << spawnPointsPrimary.front().z << ", secondary=" << spawnPointsSecondary.front().z
                    << "); verify cross-lane alignment.\n";
            }
        }

        auto sanitizePositive = [](float value, float fallback, const char* label)
        {
            if (std::isfinite(value) && value > 0.0f)
            {
                return value;
            }

            std::ostringstream oss;
            oss << kLogPrefix << label << " value " << value
                << " invalid; using fallback " << fallback << "\n";
            return fallback;
        };

        auto sanitizeDepthThreshold = [](float value, float fallback, const char* label)
        {
            if (std::isfinite(value))
            {
                if (value >= 0.0f)
                {
                    return value;
                }

                std::ostringstream oss;
                oss << kLogPrefix << label << " value " << value
                    << " invalid; using fallback " << fallback << "\n";
                return fallback;
            }

            if (std::isinf(value))
            {
                return value;
            }

            std::ostringstream oss;
            oss << kLogPrefix << label << " invalid; using fallback " << fallback << "\n";
            return fallback;
        };

        cellSizeWorld = sanitizePositive(cellSizeWorld, StageConfigDefaults::kDefaultCellSizeWorld, "cellSizeWorld");

        auto computeWorldDimension = [&](float fraction, float gridCells, float fallback)
        {
            if (std::isfinite(fraction) && fraction > 0.0f && std::isfinite(gridCells) && gridCells > 0.0f)
            {
                const float worldDimension = cellSizeWorld * gridCells * fraction;
                if (std::isfinite(worldDimension) && worldDimension > 0.0f)
                {
                    return worldDimension;
                }
            }

            return std::max(0.01f, fallback);
        };

        for (auto& descriptor : platformDescriptors)
        {
            descriptor.widthWorld = computeWorldDimension(descriptor.widthFraction, StageConfigDefaults::kPlatformGridWidthCells, descriptor.scale.x);
            descriptor.heightWorld = computeWorldDimension(descriptor.heightFraction, StageConfigDefaults::kPlatformGridHeightCells, descriptor.scale.y);
            descriptor.depthWorld = std::max(0.01f, descriptor.depth * cellSizeWorld);
        }

        horizontalSpeedCellsPerSec = sanitizePositive(
            horizontalSpeedCellsPerSec,
            StageConfigDefaults::kDefaultHorizontalSpeedCellsPerSec,
            "horizontalSpeedCellsPerSec");

        if (!verticalSpeedSet)
        {
            verticalSpeedCellsPerSec = horizontalSpeedCellsPerSec;
            verticalSpeedSet = true;
            std::ostringstream oss;
            oss << kLogPrefix << "verticalSpeedCellsPerSec not specified; mirroring horizontal speed ("
                << verticalSpeedCellsPerSec << ").\n";
        }

        verticalSpeedCellsPerSec = sanitizePositive(
            verticalSpeedCellsPerSec,
            StageConfigDefaults::kDefaultVerticalSpeedCellsPerSec,
            "verticalSpeedCellsPerSec");

        pickupBoxWidthCells = sanitizePositive(
            pickupBoxWidthCells,
            StageConfigDefaults::kDefaultPickupBoxWidthCells,
            "pickupBoxWidthCells");

        pickupBoxHeightCells = sanitizePositive(
            pickupBoxHeightCells,
            StageConfigDefaults::kDefaultPickupBoxHeightCells,
            "pickupBoxHeightCells");

        pickupMaxDistance = sanitizePositive(
            pickupMaxDistance,
            StageConfigDefaults::kDefaultPickupMaxDistance,
            "pickupMaxDistance");
        pickupDepthThreshold = sanitizeDepthThreshold(
            pickupDepthThreshold,
            StageConfigDefaults::kDefaultPickupDepthThreshold,
            "pickupDepthThreshold");

        auto sanitizeVectorFinite = [](const float3& value, const float3& fallback, const char* label)
        {
            if (std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z))
            {
                return value;
            }

            std::ostringstream oss;
            oss << kLogPrefix << label << " contains non-finite component; using fallback ("
                << fallback.x << ',' << fallback.y << ',' << fallback.z << ")\n";
            return fallback;
        };

        pickupOffsetLocalCells = sanitizeVectorFinite(
            pickupOffsetLocalCells,
            StageConfigDefaults::DefaultPickupOffsetLocalCells(),
            "pickupOffsetLocalCells");

        const float3 pickupOffsetLocalWorld
        {
            pickupOffsetLocalCells.x * cellSizeWorld,
            pickupOffsetLocalCells.y * cellSizeWorld,
            pickupOffsetLocalCells.z * cellSizeWorld,
        };

        Stage::PickupOffset pickupOffset
        {
            pickupOffsetMode,
            pickupOffsetLocalWorld,
        };

        const Stage::PickupOffset defaultPickupOffsetWorld
        {
            StageConfigDefaults::kDefaultPickupOffsetMode,
            float3{ StageConfigDefaults::DefaultPickupOffsetLocalCells().x * cellSizeWorld,
                StageConfigDefaults::DefaultPickupOffsetLocalCells().y * cellSizeWorld,
                StageConfigDefaults::DefaultPickupOffsetLocalCells().z * cellSizeWorld },
        };

        pickupOffset.localOffset = sanitizeVectorFinite(
            pickupOffset.localOffset,
            defaultPickupOffsetWorld.localOffset,
            "pickupOffsetLocalWorld");

        const float playerMoveSpeedWorld = horizontalSpeedCellsPerSec * cellSizeWorld;

        if (!std::isfinite(guidelineHeight))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineHeight value " << guidelineHeight
                << " invalid; keeping parsed value.\n";
        }

        if (!std::isfinite(guidelineStep))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineStep value " << guidelineStep
                << " invalid; keeping parsed value.\n";
        }
        if (!std::isfinite(guidelineThicknessPixels) || guidelineThicknessPixels <= 0.0f)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineThicknessPixels value " << guidelineThicknessPixels
                << " invalid; using default " << StageConfigDefaults::kDefaultGuidelineThicknessPixels << "\n";
            guidelineThicknessPixels = StageConfigDefaults::kDefaultGuidelineThicknessPixels;
        }
        if (!std::isfinite(guidelineThicknessWorld) || guidelineThicknessWorld <= 0.0f)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineThicknessWorld value " << guidelineThicknessWorld
                << " invalid; using default " << StageConfigDefaults::kDefaultGuidelineThicknessWorld << "\n";
            guidelineThicknessWorld = StageConfigDefaults::kDefaultGuidelineThicknessWorld;
        }
        if (!std::isfinite(guidelineWidthPaddingWorld) || guidelineWidthPaddingWorld < 0.0f)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineWidthPaddingWorld value " << guidelineWidthPaddingWorld
                << " invalid; using default " << StageConfigDefaults::DefaultGuidelineWidthPaddingWorld() << "\n";
            guidelineWidthPaddingWorld = StageConfigDefaults::DefaultGuidelineWidthPaddingWorld();
        }
        if (!std::isfinite(guidelineZOffset))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineZOffset value " << guidelineZOffset
                << " invalid; using default " << StageConfigDefaults::kDefaultGuidelineZOffset << "\n";
            guidelineZOffset = StageConfigDefaults::kDefaultGuidelineZOffset;
        }

        auto sanitizeAxis = [](const float3& value, const float3& fallback, const char* label)
        {
            if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
            {
                std::ostringstream oss;
                oss << kLogPrefix << label << " contains non-finite component; using fallback ("
                    << fallback.x << ',' << fallback.y << ',' << fallback.z << ")\n";
                return fallback;
            }

            const float lengthSq = (value.x * value.x) + (value.y * value.y) + (value.z * value.z);
            if (!(lengthSq > 0.0f))
            {
                std::ostringstream oss;
                oss << kLogPrefix << label << " has zero length; using fallback ("
                    << fallback.x << ',' << fallback.y << ',' << fallback.z << ")\n";
                return fallback;
            }

            const float invLength = 1.0f / std::sqrt(lengthSq);
            return float3{ value.x * invLength, value.y * invLength, value.z * invLength };
        };

        guidelineSpinAxis = sanitizeAxis(
            guidelineSpinAxis,
            StageConfigDefaults::DefaultGuidelineSpinAxis(),
            "guidelineSpinAxis");

        if (!std::isfinite(guidelineSpinSpeedDegPerSec))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineSpinSpeedDegPerSec value " << guidelineSpinSpeedDegPerSec
                << " invalid; using default " << StageConfigDefaults::kDefaultGuidelineSpinSpeedDegPerSec << "\n";
            guidelineSpinSpeedDegPerSec = StageConfigDefaults::kDefaultGuidelineSpinSpeedDegPerSec;
        }

        if (!std::isfinite(heightBonusThreshold) || heightBonusThreshold <= 0.0f)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "heightBonusThreshold value " << heightBonusThreshold
                << " invalid; using default " << StageConfigDefaults::kDefaultHeightBonusThreshold << "\n";
            heightBonusThreshold = StageConfigDefaults::kDefaultHeightBonusThreshold;
        }

        if (!std::isfinite(heightBonusSeconds))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "heightBonusSeconds value " << heightBonusSeconds
                << " invalid; using default " << StageConfigDefaults::kDefaultHeightBonusSeconds << "\n";
            heightBonusSeconds = StageConfigDefaults::kDefaultHeightBonusSeconds;
        }
        else if (heightBonusSeconds <= 0.0f)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "heightBonusSeconds <= 0. Bonus events disabled.\n";
            heightBonusSeconds = 0.0f;
        }
        else if (std::fabs(heightBonusSeconds - StageConfigDefaults::kDefaultHeightBonusSeconds)
            > std::numeric_limits<float>::epsilon())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "heightBonusSeconds=" << heightBonusSeconds
                << " overridden to requirement default " << StageConfigDefaults::kDefaultHeightBonusSeconds
                << " (+5s per crossing).\n";
            heightBonusSeconds = StageConfigDefaults::kDefaultHeightBonusSeconds;
        }

        const std::size_t requiredMultiplierCount = thresholds.size() + 1;
        const std::size_t minirequiredMultiplierCount = minithresholds.size() + 1; //ミニフェーズ
        EnsureRewardMultiplierCoverage(rewardMultipliers, requiredMultiplierCount,
            kDefaultRewardMultipliers);
        //ミニフェーズ
        EnsureRewardMultiplierCoverage(minirewardMultipliers, minirequiredMultiplierCount,
            kDefaultMiniRewardMultipliers);

        normalized.planeZ = planeZ;
        normalized.phaseThresholds = std::move(thresholds);
        normalized.phaseRewardMultipliers = std::move(rewardMultipliers);
        normalized.phaseMiniThresholds = std::move(minithresholds);
        normalized.phaseRewardMiniMultipliers = std::move(minirewardMultipliers);
        normalized.spawnPointsPrimary = std::move(spawnPointsPrimary);
        normalized.spawnPointsSecondary = std::move(spawnPointsSecondary);
        normalized.platforms = std::move(platformDescriptors);
        normalized.cellSizeWorld = cellSizeWorld;
        normalized.playerHorizontalSpeedCellsPerSec = horizontalSpeedCellsPerSec;
        normalized.playerVerticalSpeedCellsPerSec = verticalSpeedCellsPerSec;
        normalized.playerHorizontalSpeedWorld = playerMoveSpeedWorld;
        normalized.playerVerticalSpeedWorld = verticalSpeedCellsPerSec * cellSizeWorld;
        normalized.pickupBoxWidthCells = pickupBoxWidthCells;
        normalized.pickupBoxHeightCells = pickupBoxHeightCells;
        normalized.pickupBoxWidthWorld = pickupBoxWidthCells * cellSizeWorld;
        normalized.pickupBoxHeightWorld = pickupBoxHeightCells * cellSizeWorld;
        normalized.pickupMaxDistance = pickupMaxDistance;
        normalized.pickupDepthThreshold = pickupDepthThreshold;
        normalized.pickupOffset = pickupOffset;
        normalized.guidelineHeight = guidelineHeight;
        normalized.guidelineStep = guidelineStep;
        normalized.guidelineThicknessPixels = guidelineThicknessPixels;
        normalized.guidelineThicknessWorld = guidelineThicknessWorld;
        normalized.guidelineWidthPaddingWorld = guidelineWidthPaddingWorld;
        normalized.guidelineZOffset = guidelineZOffset;
        normalized.guidelineSpinAxis = guidelineSpinAxis;
        normalized.guidelineSpinSpeedDegPerSec = guidelineSpinSpeedDegPerSec;
        normalized.guidelineModelPath = std::move(guidelineModelPath);
        normalized.heightBonusThreshold = heightBonusThreshold;
        normalized.heightBonusSeconds = heightBonusSeconds;
        normalized.backgroundTexturePath = std::move(backgroundTexturePath);
        normalized.insertedFallbackAnchor = insertedFallbackAnchor;

        return normalized;
    }

    StageConfigNormalized BuildDefaultStageConfig()
    {
        StageConfigNormalized normalized{};
        normalized.planeZ = StageConfigDefaults::kDefaultPlaneZ;
        normalized.phaseThresholds = StageConfigDefaults::DefaultThresholds();
        normalized.phaseRewardMultipliers = StageConfigDefaults::DefaultRewardMultipliers();
        EnsureRewardMultiplierCoverage(normalized.phaseRewardMultipliers, normalized.phaseThresholds.size() + 1,
            kDefaultRewardMultipliers);
        normalized.phaseMiniThresholds = StageConfigDefaults::DefaultMiniThresholds();
        normalized.phaseRewardMiniMultipliers = StageConfigDefaults::DefaultMiniRewardMultipliers();
        EnsureRewardMultiplierCoverage(normalized.phaseRewardMiniMultipliers, normalized.phaseMiniThresholds.size() + 1,
            kDefaultMiniRewardMultipliers);

        normalized.spawnPointsPrimary = StageConfigDefaults::BuildDefaultSpawnPointsPrimary(StageConfigDefaults::kDefaultPlaneZ);
        normalized.spawnPointsSecondary.clear();
        normalized.platforms.clear();
        normalized.platforms.push_back(BuildDefaultPlatformDescriptor());
        normalized.cellSizeWorld = StageConfigDefaults::kDefaultCellSizeWorld;
        normalized.playerHorizontalSpeedCellsPerSec = StageConfigDefaults::kDefaultHorizontalSpeedCellsPerSec;
        normalized.playerVerticalSpeedCellsPerSec = StageConfigDefaults::kDefaultVerticalSpeedCellsPerSec;
        normalized.playerHorizontalSpeedWorld = StageConfigDefaults::DefaultPlayerMoveSpeed();
        normalized.playerVerticalSpeedWorld = StageConfigDefaults::kDefaultVerticalSpeedCellsPerSec * StageConfigDefaults::kDefaultCellSizeWorld;
        normalized.pickupBoxWidthCells = StageConfigDefaults::kDefaultPickupBoxWidthCells;
        normalized.pickupBoxHeightCells = StageConfigDefaults::kDefaultPickupBoxHeightCells;
        normalized.pickupBoxWidthWorld = StageConfigDefaults::kDefaultPickupBoxWidthCells * StageConfigDefaults::kDefaultCellSizeWorld;
        normalized.pickupBoxHeightWorld = StageConfigDefaults::kDefaultPickupBoxHeightCells * StageConfigDefaults::kDefaultCellSizeWorld;
        normalized.pickupMaxDistance = StageConfigDefaults::kDefaultPickupMaxDistance;
        normalized.pickupDepthThreshold = StageConfigDefaults::kDefaultPickupDepthThreshold;
        normalized.pickupOffset = PickupOffset
        {
            StageConfigDefaults::kDefaultPickupOffsetMode,
            float3{ StageConfigDefaults::DefaultPickupOffsetLocalCells().x * StageConfigDefaults::kDefaultCellSizeWorld,
                StageConfigDefaults::DefaultPickupOffsetLocalCells().y * StageConfigDefaults::kDefaultCellSizeWorld,
                StageConfigDefaults::DefaultPickupOffsetLocalCells().z * StageConfigDefaults::kDefaultCellSizeWorld },
        };
        normalized.guidelineHeight = StageConfigDefaults::kDefaultGuidelineHeight;
        normalized.guidelineStep = StageConfigDefaults::kDefaultGuidelineStep;
        normalized.guidelineThicknessPixels = StageConfigDefaults::kDefaultGuidelineThicknessPixels;
        normalized.guidelineThicknessWorld = StageConfigDefaults::kDefaultGuidelineThicknessWorld;
        normalized.guidelineWidthPaddingWorld = StageConfigDefaults::DefaultGuidelineWidthPaddingWorld();
        normalized.guidelineZOffset = StageConfigDefaults::kDefaultGuidelineZOffset;
        normalized.guidelineSpinAxis = StageConfigDefaults::DefaultGuidelineSpinAxis();
        normalized.guidelineSpinSpeedDegPerSec = StageConfigDefaults::kDefaultGuidelineSpinSpeedDegPerSec;
        normalized.guidelineModelPath = StageConfigDefaults::DefaultGuidelineModelPath();
        normalized.backgroundTexturePath = StageConfigDefaults::DefaultBackgroundTexturePath();
        normalized.heightBonusThreshold = StageConfigDefaults::kDefaultHeightBonusThreshold;
        normalized.heightBonusSeconds = StageConfigDefaults::kDefaultHeightBonusSeconds;
        normalized.parseError = false;
        normalized.primaryUsedDefaults = true;
        normalized.secondaryUsedDefaults = false;
        normalized.primaryProvided = false;
        normalized.secondaryProvided = false;
        normalized.primaryParseError = false;
        normalized.secondaryParseError = false;
        normalized.insertedFallbackAnchor = false;
        return normalized;
    }
}
