#pragma once

#include <chrono>
#include <vector>

namespace GameFlow
{
    class PhaseManager
    {
    public:
        PhaseManager();

        void Initialize(const std::vector<float>& thresholds, const std::vector<float>& rewardMultipliers);
        void Reset();
        // 現在のフェーズが増加した場合に true を返す
        bool UpdateHeight(float currentMaxHeight);

        bool UpdateMiniHeight(float currentMaxMiniHeight); //ミニフェーズ

        int GetCurrentPhase() const noexcept;
        float GetRewardMultiplier() const noexcept;

    private:
        float GetRewardMultiplierForPhase(int phaseIndex) const noexcept;
        void LogPhaseAdvance(float threshold, float currentHeight) const;

        std::vector<float> m_thresholds;
        std::vector<float> m_rewardMultipliers;

        //ミニフェーズ 
        std::vector<float> m_minithresholds;
        std::vector<float> m_minirewardMultipliers;

        std::size_t m_nextThresholdIndex;
        int m_currentPhase;
        bool m_initialized;
        float m_lastReportedHeight;
        std::chrono::steady_clock::time_point m_startTime;
    };
}
