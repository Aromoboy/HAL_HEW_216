#pragma once

#include <DirectXMath.h>
#include <optional>
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

    struct StageConfigNormalized;

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
        //ミニフェーズ
        const std::vector<float>& GetPhaseMiniThresholds() const noexcept;
        const std::vector<float>& GetPhaseRewardMiniMultipliers() const noexcept;
        const std::vector<float3>& GetSpawnPointsPrimary() const noexcept;
        const std::vector<float3>& GetSpawnPointsSecondary() const noexcept;
        // 旧来のプライマリ出現ポイント用エイリアス
        float GetPlaneZ() const noexcept;
        const PlatformDescriptor& GetPlatform(std::size_t index) const;
        std::size_t GetPlatformCount() const noexcept;
        float GetPlayerHorizontalSpeedWorld() const noexcept;
        float GetPlayerVerticalSpeedWorld() const noexcept;
        float GetCellSizeWorld() const noexcept;
        const std::string& GetBackgroundTexturePath() const noexcept;
        std::optional<float> GetBackgroundSafeScrollY() const noexcept;
        std::optional<float> GetBackgroundSafeWindowY() const noexcept;
        std::optional<float> GetBackgroundSoftnessY() const noexcept;
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
        std::vector<float> m_phaseMiniThresholds;        //ミニフェーズ
        std::vector<float> m_phaseRewardMiniMultipliers; //ミニフェーズ
        std::vector<float3> m_spawnPointsPrimary;
        std::vector<float3> m_spawnPointsSecondary;
        std::string m_sourcePath;
        bool m_loadedFromFile;
        float m_cellSizeWorld;
        float m_playerHorizontalSpeedCellsPerSec;
        float m_playerVerticalSpeedCellsPerSec;
        float m_playerHorizontalSpeedWorld;
        float m_playerVerticalSpeedWorld;
        std::string m_backgroundTexturePath;
        std::optional<float> m_backgroundSafeScrollY;
        std::optional<float> m_backgroundSafeWindowY;
        std::optional<float> m_backgroundSoftnessY;
    };
}
