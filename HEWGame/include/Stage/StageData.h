#pragma once

#include <string>
#include <vector>

#include "Vector.h"

namespace Stage
{
    class StageData
    {
    public:
        StageData();

        // Attempts to load the stage data from a config file.
        // Returns true when data was parsed successfully, false when defaults were used.
        bool LoadFromFile(const std::string& path);

        const std::vector<float>& GetPhaseThresholds() const noexcept;
        const std::vector<float>& GetPhaseRewardMultipliers() const noexcept;
        const std::vector<float3>& GetSpawnPoints() const noexcept;
        float GetPlaneZ() const noexcept;
        float GetPlatformCenterY() const noexcept;
        float GetPlatformWidthFraction() const noexcept;
        float GetPlatformHeightFraction() const noexcept;
        float GetPlatformDepth() const noexcept;
        const float3& GetPlatformScale() const noexcept;
        const std::string& GetSourcePath() const noexcept;
        float GetPlayerMoveSpeed() const noexcept;
        float GetGuidelineHeight() const noexcept;
        const std::string& GetBackgroundTexturePath() const noexcept;
        float GetHeightBonusThreshold() const noexcept;
        float GetHeightBonusSeconds() const noexcept;

    private:
        void ApplyDefaults();
        void LogSummary(const char* prefix, const std::string& detail) const;
        void EnsureRewardMultiplierCoverage(std::vector<float>& multipliers, std::size_t requiredCount) const;

        float m_planeZ;
        float m_platformCenterY;
        float m_platformWidthFraction;
        float m_platformHeightFraction;
        float m_platformDepth;
        float3 m_platformScale;
        std::vector<float> m_phaseThresholds;
        std::vector<float> m_phaseRewardMultipliers;
        std::vector<float3> m_spawnPoints;
        std::string m_sourcePath;
        bool m_loadedFromFile;
        float m_playerMoveSpeed;
        float m_guidelineHeight;
        std::string m_backgroundTexturePath;
        float m_heightBonusThreshold;
        float m_heightBonusSeconds;
    };
}
