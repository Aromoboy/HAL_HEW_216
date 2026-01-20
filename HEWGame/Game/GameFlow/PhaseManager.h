#pragma once

#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>

namespace GameFlow
{
    class PhaseManager
    {
    public:
        PhaseManager();

        struct PhaseUpdateResult
        {
            bool phaseAdvanced;
        };

        void Initialize(
            const std::vector<float>& thresholds,
            const std::vector<float>& rewardMultipliers
        );

        void Reset();
        PhaseUpdateResult UpdateHeight(float currentMaxHeight);
        // 時間ベースの進行は、フェーズの二重発動を避けるため意図的に無効化
        PhaseUpdateResult UpdateTime(float deltaSeconds);
        bool AdvancePhase();

        int GetCurrentPhase() const noexcept;
        float GetRewardMultiplier() const noexcept;

    private:

        // 時間計測
        std::chrono::steady_clock::time_point m_startTime;

        // 閾値
        std::vector<float> m_thresholds;
        std::vector<float> m_rewardMultipliers;

        std::size_t m_nextThresholdIndex;
        int m_currentPhase;

        bool m_initialized;
        float m_lastReportedHeight;
        float m_elapsedSeconds;

        float GetRewardMultiplierForPhase(int phaseIndex) const noexcept;

    };
}
