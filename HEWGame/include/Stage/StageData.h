#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>

#include "Vector.h"

namespace Stage
{
    struct PlatformDescriptor
    {
        float3 center{ 0.0f, 0.0f, 0.0f };
        float3 scale{ 1.0f, 1.0f, 1.0f };
        float depth = 1.0f;
        float widthFraction = 1.0f;
        float heightFraction = 1.0f;
        float widthWorld = 1.0f;
        float heightWorld = 1.0f;
        float depthWorld = 1.0f;
        DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    class StageData
    {
    public:
        StageData();

        // Attempts to load the stage data from a config file.
        // Returns true when data was parsed successfully, false when defaults were used.
        bool LoadFromFile(const std::string& path);

        const std::vector<float>& GetPhaseThresholds() const noexcept;
        const std::vector<float>& GetPhaseRewardMultipliers() const noexcept;
        //ミニフェーズ
        const std::vector<float>& GetPhaseMiniThresholds() const noexcept;
        const std::vector<float>& GetPhaseRewardMiniMultipliers() const noexcept;
        const std::vector<float3>& GetSpawnPoints() const noexcept;
        float GetPlaneZ() const noexcept;
        const std::vector<PlatformDescriptor>& GetPlatforms() const noexcept;
        const PlatformDescriptor& GetPlatform(std::size_t index) const;
        std::size_t GetPlatformCount() const noexcept;
        const std::string& GetSourcePath() const noexcept;
        float GetPlayerMoveSpeed() const noexcept;
        float GetPlayerHorizontalSpeedWorld() const noexcept;
        float GetPlayerVerticalSpeedWorld() const noexcept;
        float GetCellSizeWorld() const noexcept;
        float GetPickupBoxWidthWorld() const noexcept;
        float GetPickupBoxHeightWorld() const noexcept;
        float GetGuidelineHeight() const noexcept;
        const std::string& GetBackgroundTexturePath() const noexcept;
        float GetHeightBonusThreshold() const noexcept;
        float GetHeightBonusSeconds() const noexcept;
        float GetOsaraLeftOffsetX() const noexcept;
        float GetOsaraRightOffsetX() const noexcept;
        float GetOsaraOffsetY() const noexcept;
        float GetOsaraSpawnOffsetY() const noexcept;

    private:
        void ApplyDefaults();
        void LogSummary(const char* prefix, const std::string& detail) const;
        void EnsureRewardMultiplierCoverage(std::vector<float>& multipliers, std::size_t requiredCount,
            const float* defaultMultipliers) const;
        void LogPlatformSummary(std::size_t index, const PlatformDescriptor& descriptor) const;

        float m_planeZ;
        std::vector<PlatformDescriptor> m_platforms;
        std::vector<float> m_phaseThresholds;
        std::vector<float> m_phaseRewardMultipliers;
        std::vector<float> m_phaseMiniThresholds;        //ミニフェーズ
        std::vector<float> m_phaseRewardMiniMultipliers; //ミニフェーズ
        std::vector<float3> m_spawnPoints;
        std::string m_sourcePath;
        bool m_loadedFromFile;
        float m_cellSizeWorld;
        float m_playerHorizontalSpeedCellsPerSec;
        float m_playerVerticalSpeedCellsPerSec;
        float m_playerHorizontalSpeedWorld;
        float m_playerVerticalSpeedWorld;
        float m_pickupBoxWidthCells;
        float m_pickupBoxHeightCells;
        float m_pickupBoxWidthWorld;
        float m_pickupBoxHeightWorld;
        float m_playerMoveSpeed;
        float m_guidelineHeight;
        std::string m_backgroundTexturePath;
        float m_heightBonusThreshold;
        float m_heightBonusSeconds;
        float m_osaraLeftOffsetX;
        float m_osaraRightOffsetX;
        float m_osaraOffsetY;
        float m_osaraSpawnOffsetY;
        bool m_osaraOffsetsConfigured;
        bool m_spawnPointsSuppressed;
    };
}
