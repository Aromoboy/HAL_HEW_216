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
        int GetCurrentPhase() const noexcept;
        float GetRewardMultiplier() const noexcept;

    private:
        float GetRewardMultiplierForPhase(int phaseIndex) const noexcept;
        void LogPhaseAdvance(float threshold, float currentHeight) const;

        std::vector<float> m_thresholds;
        std::vector<float> m_rewardMultipliers;
        std::size_t m_nextThresholdIndex;
        int m_currentPhase;
        bool m_initialized;
        float m_lastReportedHeight;
        std::chrono::steady_clock::time_point m_startTime;
    };
}
