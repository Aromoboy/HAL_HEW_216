#pragma once

#include <chrono>
#include <vector>

namespace GameFlow
{
    class PhaseManager
    {
    public:
        PhaseManager();

        void Initialize(const std::vector<float>& thresholds);
        // Returns true when the current phase increased.
        bool UpdateHeight(float currentMaxHeight);
        int GetCurrentPhase() const noexcept;

    private:
        void LogPhaseAdvance(float threshold, float currentHeight) const;

        std::vector<float> m_thresholds;
        std::size_t m_nextThresholdIndex;
        int m_currentPhase;
        bool m_initialized;
        float m_lastReportedHeight;
        std::chrono::steady_clock::time_point m_startTime;
    };
}
