#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>

#include "PickupConfig.h"
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

    struct StageConfigNormalized;

    enum class BackgroundScrollMode
    {
        Clamp,
        Wrap
    };

    class StageData
    {
    public:
        StageData();

        // 設定ファイルからステージデータを読み込む。
        // 成功時は真、失敗時は偽（状態は保持）。
        bool LoadFromFile(const std::string& path);
        bool LoadFromFile(const std::string& path, const std::string& section);

        const std::vector<float>& GetPhaseThresholds() const noexcept;
        const std::vector<float>& GetPhaseRewardMultipliers() const noexcept;
        const std::vector<float3>& GetSpawnPointsPrimary() const noexcept;
        const std::vector<float3>& GetSpawnPointsSecondary() const noexcept;
        // 旧来のプライマリ出現ポイント用エイリアス
        float GetPlaneZ() const noexcept;
        const PlatformDescriptor& GetPlatform(std::size_t index) const;
        std::size_t GetPlatformCount() const noexcept;
        float GetPlayerHorizontalSpeedWorld() const noexcept;
        float GetPlayerVerticalSpeedWorld() const noexcept;
        float GetCellSizeWorld() const noexcept;
        float GetPickupBoxWidthWorld() const noexcept;
        float GetPickupBoxHeightWorld() const noexcept;
        float GetPickupMaxDistance() const noexcept;
        float GetPickupDepthThreshold() const noexcept;
        const PickupOffset& GetPickupOffset() const noexcept;
        float GetGuidelineHeight() const noexcept;
        float GetGuidelineStep() const noexcept;
        float GetGuidelinePlayerMaxYOffset() const noexcept;
        float GetGuidelineThicknessPixels() const noexcept;
        float GetGuidelineThicknessWorld() const noexcept;
        float GetGuidelineWidthPaddingWorld() const noexcept;
        float GetGuidelineZOffset() const noexcept;
        float3 GetGuidelineSpinAxis() const noexcept;
        float GetGuidelineSpinSpeedDegPerSec() const noexcept;
        const std::string& GetGuidelineModelPath() const noexcept;
        const std::string& GetGuidelineMaterialKey() const noexcept;
        float GetGuidelineTimeBonusSeconds() const noexcept;
        float GetGuidelineMilestoneBonusSeconds() const noexcept;
        int GetGuidelineMilestoneInterval() const noexcept;
        const std::string& GetBackgroundTexturePath() const noexcept;
        const std::string& GetBackgroundVideoPath() const noexcept;
        bool IsBackgroundVideoAutoLoop() const noexcept;
        BackgroundScrollMode GetBackgroundScrollMode() const noexcept;
        float GetBackgroundBaseOffset01() const noexcept;
        float GetBackgroundBaseOffsetPixels() const noexcept;
    private:
        void ApplyDefaults();
        void ApplyNormalizedConfig(const StageConfigNormalized& normalized, const std::string& sourcePath, bool loadedFromFile);
        void LogSummary(const char* prefix, const std::string& detail) const;
        void EnsureRewardMultiplierCoverage(std::vector<float>& multipliers, std::size_t requiredCount,
            const float* defaultMultipliers) const;
        void LogPlatformSummary(std::size_t index, const PlatformDescriptor& descriptor) const;

        float m_planeZ;
        std::vector<PlatformDescriptor> m_platforms;
        std::vector<float> m_phaseThresholds;
        std::vector<float> m_phaseRewardMultipliers;
        std::vector<float3> m_spawnPointsPrimary;
        std::vector<float3> m_spawnPointsSecondary;
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
        float m_pickupMaxDistance;
        float m_pickupDepthThreshold;
        PickupOffset m_pickupOffset;
        float m_guidelineHeight;
        float m_guidelineStep;
        float m_guidelinePlayerMaxYOffset;
        float m_guidelineThicknessPixels;
        float m_guidelineThicknessWorld;
        float m_guidelineWidthPaddingWorld;
        float m_guidelineZOffset;
        float3 m_guidelineSpinAxis;
        float m_guidelineSpinSpeedDegPerSec;
        std::string m_guidelineModelPath;
        std::string m_guidelineMaterialKey;
        float m_guidelineTimeBonusSeconds;
        float m_guidelineMilestoneBonusSeconds;
        int m_guidelineMilestoneInterval;
        std::string m_backgroundTexturePath;
        std::string m_backgroundVideoPath;
        bool m_backgroundVideoAutoLoop = true;
        BackgroundScrollMode m_backgroundScrollMode = BackgroundScrollMode::Clamp;
        float m_backgroundBaseOffset01 = 0.0f;
        float m_backgroundBaseOffsetPixels = 0.0f;
    };
}
