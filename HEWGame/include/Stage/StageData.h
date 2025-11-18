// ============================================================================
// 概要: StageData に関するインターフェースやデータ構造を宣言し、include／Stage から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include／Stage 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

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

        // 備考: Attempts to load the stage data from a config file. に関する説明を日本語で補足している。
        // 備考: Returns true when data was parsed successfully, false when defaults were used. に関する説明を日本語で補足している。
        bool LoadFromFile(const std::string& path);

        const std::vector<float>& GetPhaseThresholds() const noexcept;
        const std::vector<float>& GetPhaseRewardMultipliers() const noexcept;
        const std::vector<float3>& GetSpawnPoints() const noexcept;
        float GetPlaneZ() const noexcept;
        float GetPlatformCenterY() const noexcept;
        const float3& GetPlatformScale() const noexcept;
        const std::string& GetSourcePath() const noexcept;
        float GetPlayerMoveSpeed() const noexcept;

    private:
        void ApplyDefaults();
        void LogSummary(const char* prefix, const std::string& detail) const;
        void EnsureRewardMultiplierCoverage(std::vector<float>& multipliers, std::size_t requiredCount) const;

        float m_planeZ;
        float m_platformCenterY;
        float3 m_platformScale;
        std::vector<float> m_phaseThresholds;
        std::vector<float> m_phaseRewardMultipliers;
        std::vector<float3> m_spawnPoints;
        std::string m_sourcePath;
        bool m_loadedFromFile;
        float m_playerMoveSpeed;
    };
}
