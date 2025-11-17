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
        const std::string& GetSourcePath() const noexcept;

    private:
        void ApplyDefaults();
        void LogSummary(const char* prefix, const std::string& detail) const;
        void EnsureRewardMultiplierCoverage(std::vector<float>& multipliers, std::size_t requiredCount) const;

        float m_planeZ;
        std::vector<float> m_phaseThresholds;
        std::vector<float> m_phaseRewardMultipliers;
        std::vector<float3> m_spawnPoints;
        std::string m_sourcePath;
        bool m_loadedFromFile;
    };
}
