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
            bool miniPhaseAdvanced;
            float timeElapsed;
        };

        void Initialize(
            const std::vector<float>& thresholds,
            const std::vector<float>& rewardMultipliers,
            const std::vector<float>& minithresholds,
            const std::vector<float>& rewardMiniMultipliers
        );

        void Reset();
        PhaseUpdateResult UpdateHeight(float currentMaxHeight);
        // 時間ベースの進行は、フェーズの二重発動を避けるため意図的に無効化
        PhaseUpdateResult UpdateTime(float deltaSeconds);
        bool AdvancePhase();

        int GetCurrentPhase() const noexcept;
        float GetRewardMultiplier() const noexcept;

        // mini-phase support
        float GetRewardMiniMultiplier() const noexcept;

    private:

        // time tracking
        std::chrono::steady_clock::time_point m_startTime;

        // thresholds
        std::vector<float> m_thresholds;
        std::vector<float> m_rewardMultipliers;

        // mini thresholds
        std::vector<float> m_minithresholds;
        std::vector<float> m_minirewardMultipliers;

        std::size_t m_nextThresholdIndex;
        int m_currentPhase;

        std::size_t m_nextMiniThresholdIndex;
        int m_currentMiniPhase;

        bool m_initialized;
        float m_lastReportedHeight;
        float m_elapsedSeconds;

        float GetRewardMultiplierForPhase(int phaseIndex) const noexcept;
        float GetRewardMiniMultiplierForPhase(int phaseminiIndex) const noexcept;

    };
}
