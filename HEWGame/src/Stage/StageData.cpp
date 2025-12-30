#include "Stage/StageData.h"

#include <sstream>

#include "ConfigSectionReader.h"
#include "Stage/StageConfigNormalize.h"
#include "Stage/StageConfigParser.h"

namespace
{
    constexpr const char* kLogPrefix = "[StageData] ";
}

namespace Stage
{
    StageData::StageData()
    {
        ApplyDefaults();
    }

    bool StageData::LoadFromFile(const std::string& path)
    {
        return LoadFromFile(path, std::string{});
    }

    bool StageData::LoadFromFile(const std::string& path, const std::string& section)
    {
        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(path, section, lines))
        {
            std::ostringstream oss;
            if (section.empty())
            {
                oss << kLogPrefix << "Config not found at '" << path << "'. Using defaults.\n";
            }
            else
            {
                oss << kLogPrefix << "Config section '" << section << "' not found at '" << path
                    << "'. Using defaults.\n";
            }
            ApplyDefaults();
            m_sourcePath.clear();
            m_loadedFromFile = false;
            return false;
        }

        const StageConfigParseResult parsed = ParseStageConfig(lines);
        const StageConfigNormalized normalized = NormalizeStageConfig(parsed);
        ApplyNormalizedConfig(normalized, path, !normalized.parseError);

        std::ostringstream detail;
        detail << "source='" << path << "' parseError=" << std::boolalpha << normalized.parseError
            << " primaryDefaulted=" << normalized.primaryUsedDefaults
            << " secondaryDefaulted=" << normalized.secondaryUsedDefaults
            << " primaryProvided=" << normalized.primaryProvided
            << " secondaryProvided=" << normalized.secondaryProvided
            << " primaryParseError=" << normalized.primaryParseError
            << " secondaryParseError=" << normalized.secondaryParseError
            << " fallbackAnchors=" << normalized.insertedFallbackAnchor
            << " minithresholds=" << m_phaseMiniThresholds.size()
            << " minimultipliers=" << m_phaseRewardMiniMultipliers.size()
            << " platformCount=" << m_platforms.size();
        LogSummary("Loaded", detail.str());

        return m_loadedFromFile;
    }

    const std::vector<float>& StageData::GetPhaseThresholds() const noexcept
    {
        return m_phaseThresholds;
    }

    const std::vector<float>& StageData::GetPhaseRewardMultipliers() const noexcept
    {
        return m_phaseRewardMultipliers;
    }
    //ミニフェーズ
    const std::vector<float>& StageData::GetPhaseMiniThresholds() const noexcept
    {
        return m_phaseMiniThresholds;
    }
    //ミニフェーズ
    const std::vector<float>& StageData::GetPhaseRewardMiniMultipliers() const noexcept
    {
        return m_phaseRewardMiniMultipliers;
    }

    const std::vector<float3>& StageData::GetSpawnPointsPrimary() const noexcept
    {
        return m_spawnPointsPrimary;
    }

    const std::vector<float3>& StageData::GetSpawnPointsSecondary() const noexcept
    {
        return m_spawnPointsSecondary;
    }

    float StageData::GetPlaneZ() const noexcept
    {
        return m_planeZ;
    }

    const PlatformDescriptor& StageData::GetPlatform(std::size_t index) const
    {
        if (index < m_platforms.size())
        {
            return m_platforms[index];
        }

        if (m_platforms.empty())
        {
            static PlatformDescriptor s_fallback = BuildDefaultPlatformDescriptor();
            return s_fallback;
        }

        return m_platforms.front();
    }

    std::size_t StageData::GetPlatformCount() const noexcept
    {
        return m_platforms.size();
    }

    float StageData::GetPlayerHorizontalSpeedWorld() const noexcept
    {
        return m_playerHorizontalSpeedWorld;
    }

    float StageData::GetPlayerVerticalSpeedWorld() const noexcept
    {
        return m_playerVerticalSpeedWorld;
    }

    float StageData::GetCellSizeWorld() const noexcept
    {
        return m_cellSizeWorld;
    }

    float StageData::GetPickupBoxWidthWorld() const noexcept
    {
        return m_pickupBoxWidthWorld;
    }

    float StageData::GetPickupBoxHeightWorld() const noexcept
    {
        return m_pickupBoxHeightWorld;
    }

    float StageData::GetPickupMaxDistance() const noexcept
    {
        return m_pickupMaxDistance;
    }

    float StageData::GetPickupDepthThreshold() const noexcept
    {
        return m_pickupDepthThreshold;
    }

    const PickupOffset& StageData::GetPickupOffset() const noexcept
    {
        return m_pickupOffset;
    }

    float StageData::GetGuidelineHeight() const noexcept
    {
        return m_guidelineHeight;
    }

    float StageData::GetGuidelineStep() const noexcept
    {
        return m_guidelineStep;
    }

    float StageData::GetGuidelineThicknessPixels() const noexcept
    {
        return m_guidelineThicknessPixels;
    }

    float StageData::GetGuidelineWidthPaddingWorld() const noexcept
    {
        return m_guidelineWidthPaddingWorld;
    }

    float StageData::GetGuidelineZOffset() const noexcept
    {
        return m_guidelineZOffset;
    }

    const std::string& StageData::GetBackgroundTexturePath() const noexcept
    {
        return m_backgroundTexturePath;
    }

    float StageData::GetHeightBonusThreshold() const noexcept
    {
        return m_heightBonusThreshold;
    }

    float StageData::GetHeightBonusSeconds() const noexcept
    {
        return m_heightBonusSeconds;
    }

    void StageData::ApplyDefaults()
    {
        const StageConfigNormalized defaults = BuildDefaultStageConfig();
        ApplyNormalizedConfig(defaults, std::string{}, false);
        LogSummary("Defaults", "applied");
    }

    void StageData::ApplyNormalizedConfig(const StageConfigNormalized& normalized, const std::string& sourcePath, bool loadedFromFile)
    {
        m_planeZ = normalized.planeZ;
        m_phaseThresholds = normalized.phaseThresholds;
        m_phaseRewardMultipliers = normalized.phaseRewardMultipliers;
        m_phaseMiniThresholds = normalized.phaseMiniThresholds;
        m_phaseRewardMiniMultipliers = normalized.phaseRewardMiniMultipliers;
        m_spawnPointsPrimary = normalized.spawnPointsPrimary;
        m_spawnPointsSecondary = normalized.spawnPointsSecondary;
        m_platforms = normalized.platforms;
        m_cellSizeWorld = normalized.cellSizeWorld;
        m_playerHorizontalSpeedCellsPerSec = normalized.playerHorizontalSpeedCellsPerSec;
        m_playerVerticalSpeedCellsPerSec = normalized.playerVerticalSpeedCellsPerSec;
        m_playerHorizontalSpeedWorld = normalized.playerHorizontalSpeedWorld;
        m_playerVerticalSpeedWorld = normalized.playerVerticalSpeedWorld;
        m_pickupBoxWidthCells = normalized.pickupBoxWidthCells;
        m_pickupBoxHeightCells = normalized.pickupBoxHeightCells;
        m_pickupBoxWidthWorld = normalized.pickupBoxWidthWorld;
        m_pickupBoxHeightWorld = normalized.pickupBoxHeightWorld;
        m_pickupMaxDistance = normalized.pickupMaxDistance;
        m_pickupDepthThreshold = normalized.pickupDepthThreshold;
        m_pickupOffset = normalized.pickupOffset;
        // Keep the configured guideline height; do not override valid config with a magic value.
        m_guidelineHeight = normalized.guidelineHeight;
        m_guidelineStep = normalized.guidelineStep;
        m_guidelineThicknessPixels = normalized.guidelineThicknessPixels;
        m_guidelineWidthPaddingWorld = normalized.guidelineWidthPaddingWorld;
        m_guidelineZOffset = normalized.guidelineZOffset;
        m_heightBonusThreshold = normalized.heightBonusThreshold;
        m_heightBonusSeconds = normalized.heightBonusSeconds;
        m_backgroundTexturePath = normalized.backgroundTexturePath;
        m_sourcePath = sourcePath;
        m_loadedFromFile = loadedFromFile;
    }

    void StageData::LogSummary(const char* prefix, const std::string& detail) const
    {
        std::ostringstream oss;
        oss << kLogPrefix << prefix << " planeZ=" << m_planeZ
            << " thresholds=" << m_phaseThresholds.size()
            << " multipliers=" << m_phaseRewardMultipliers.size()
            << " minithreshold=" << m_phaseMiniThresholds.size()          //ミニフェーズ
            << " minimultipliers=" << m_phaseRewardMiniMultipliers.size() //ミニフェーズ
            << " spawnPointsPrimary=" << m_spawnPointsPrimary.size()
            << " spawnPointsSecondary=" << m_spawnPointsSecondary.size()
            << " platforms=" << m_platforms.size()
            << " cellSizeWorld=" << m_cellSizeWorld
            << " playerHorizontalSpeedWorld=" << m_playerHorizontalSpeedWorld
            << " playerVerticalSpeedWorld=" << m_playerVerticalSpeedWorld
            << " pickupBoxWidthWorld=" << m_pickupBoxWidthWorld
            << " pickupBoxHeightWorld=" << m_pickupBoxHeightWorld
            << " pickupMaxDistance=" << m_pickupMaxDistance
            << " pickupDepthThreshold=" << m_pickupDepthThreshold
            << " pickupOffsetMode=" << PickupOffsetModeToString(m_pickupOffset.mode)
            << " pickupOffsetLocalWorld=(" << m_pickupOffset.localOffset.x
            << ',' << m_pickupOffset.localOffset.y << ',' << m_pickupOffset.localOffset.z << ')'
            << " guidelineHeight=" << m_guidelineHeight
            << " guidelineStep=" << m_guidelineStep
            << " guidelineThicknessPixels=" << m_guidelineThicknessPixels
            << " guidelineWidthPaddingWorld=" << m_guidelineWidthPaddingWorld
            << " guidelineZOffset=" << m_guidelineZOffset
            << " heightBonusThreshold=" << m_heightBonusThreshold
            << " heightBonusSeconds=" << m_heightBonusSeconds
            << " backgroundTexture='" << m_backgroundTexturePath << "'"
            << " detail=" << detail << "\n";

        for (std::size_t i = 0; i < m_platforms.size(); ++i)
        {
            LogPlatformSummary(i, m_platforms[i]);
        }
    }

    void StageData::LogPlatformSummary(std::size_t index, const PlatformDescriptor& descriptor) const
    {
        std::ostringstream oss;
        oss << kLogPrefix << " platform[" << index << "] center=(" << descriptor.center.x << ',' << descriptor.center.y
            << ',' << descriptor.center.z << ") scale=(" << descriptor.scale.x << ',' << descriptor.scale.y << ','
            << descriptor.scale.z << ") depth=" << descriptor.depth
            << " worldScale=(" << descriptor.widthWorld << ',' << descriptor.heightWorld << ',' << descriptor.depthWorld << ')'
            << " widthFraction=" << descriptor.widthFraction
            << " color=(" << descriptor.color.x << ',' << descriptor.color.y << ','
            << descriptor.color.z << ',' << descriptor.color.w << ")\n";
    }

    void StageData::EnsureRewardMultiplierCoverage(std::vector<float>& multipliers, std::size_t requiredCount,
        const float* defaultMultipliers) const
    {
        ::Stage::EnsureRewardMultiplierCoverage(multipliers, requiredCount, defaultMultipliers);
    }
}
