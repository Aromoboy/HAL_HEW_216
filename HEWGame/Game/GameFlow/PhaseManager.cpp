#include "GameFlow/PhaseManager.h"

#include <algorithm>
#include <cmath>

namespace GameFlow
{
    namespace
    {
        constexpr float kAutoThresholdFallbackStep = 10.0f;
    }

    PhaseManager::PhaseManager()
        : m_nextThresholdIndex(0)
        , m_currentPhase(0)
        , m_initialized(false)
        , m_lastReportedHeight(0.0f)
        , m_elapsedSeconds(0.0f)
    {
    }

    void PhaseManager::Initialize(const std::vector<float>& thresholds, const std::vector<float>& rewardMultipliers)
    {
        m_thresholds = thresholds;
        std::sort(m_thresholds.begin(), m_thresholds.end());
        m_thresholds.erase(
            std::remove_if(m_thresholds.begin(), m_thresholds.end(), [](float value)
                {
                    return !std::isfinite(value) || value <= 0.0f;
                }),
            m_thresholds.end());
        m_thresholds.erase(std::unique(m_thresholds.begin(), m_thresholds.end()), m_thresholds.end());

        m_rewardMultipliers = rewardMultipliers;
        if (m_rewardMultipliers.empty())
        {
            m_rewardMultipliers.push_back(1.0f);
        }
        const std::size_t requiredMultiplierCount = m_thresholds.size() + 1;
        if (m_rewardMultipliers.size() < requiredMultiplierCount)
        {
            const float fillValue = m_rewardMultipliers.back();
            m_rewardMultipliers.resize(requiredMultiplierCount, fillValue);
        }
        else if (m_rewardMultipliers.size() > requiredMultiplierCount)
        {
            m_rewardMultipliers.resize(requiredMultiplierCount);
        }

        m_nextThresholdIndex = 0;
        m_currentPhase = 0;
        m_initialized = true;
        m_lastReportedHeight = 0.0f;
        m_elapsedSeconds = 0.0f;
        m_startTime = std::chrono::steady_clock::now();

    }

    void PhaseManager::Reset()
    {
        if (!m_initialized)
        {
            return;
        }

        m_nextThresholdIndex = 0;
        m_currentPhase = 0;
        m_lastReportedHeight = 0.0f;
        m_elapsedSeconds = 0.0f;
        m_startTime = std::chrono::steady_clock::now();

    }

    bool PhaseManager::AdvancePhase()
    {
        if (!m_initialized)
        {
            return false;
        }

        const std::size_t requiredMultiplierCount = m_thresholds.size() + 1;
        if (m_rewardMultipliers.size() < requiredMultiplierCount)
        {
            const float fillValue = m_rewardMultipliers.empty() ? 1.0f : m_rewardMultipliers.back();
            m_rewardMultipliers.resize(requiredMultiplierCount, fillValue);
        }

        ++m_currentPhase;
        m_nextThresholdIndex = std::min(m_nextThresholdIndex + 1, m_thresholds.size());

        return true;
    }


    PhaseManager::PhaseUpdateResult PhaseManager::UpdateHeight(float currentMaxHeight)
    {
        PhaseUpdateResult phaseupdate = {};

        phaseupdate.phaseAdvanced = false;

        if (!m_initialized)
        {
            return phaseupdate;
        }

        if (!std::isfinite(currentMaxHeight))
        {
            return phaseupdate;
        }

        m_lastReportedHeight = std::max(m_lastReportedHeight, currentMaxHeight);

        if (m_nextThresholdIndex >= m_thresholds.size() && !m_thresholds.empty())
        {
            while (true)
            {
                float step = kAutoThresholdFallbackStep;
                if (m_thresholds.size() >= 2)
                {
                    const float last = m_thresholds[m_thresholds.size() - 1];
                    const float prev = m_thresholds[m_thresholds.size() - 2];
                    const float candidate = last - prev;
                    if (std::isfinite(candidate) && candidate > 0.0f)
                    {
                        step = candidate;
                    }
                }

                const float target = m_thresholds.back() + step;
                if (m_lastReportedHeight < target)
                {
                    break;
                }

                const float newThreshold = target;
                m_thresholds.push_back(newThreshold);

                const std::size_t requiredMultiplierCount = m_thresholds.size() + 1;
                if (m_rewardMultipliers.empty())
                {
                    m_rewardMultipliers.push_back(1.0f);
                }
                if (m_rewardMultipliers.size() < requiredMultiplierCount)
                {
                    m_rewardMultipliers.resize(requiredMultiplierCount, m_rewardMultipliers.back());
                }

                if (newThreshold <= 0.0f || !std::isfinite(newThreshold))
                {
                    break;
                }
            }
        }

        while (m_nextThresholdIndex < m_thresholds.size()
            && m_lastReportedHeight >= m_thresholds[m_nextThresholdIndex])
        {
            const float threshold = m_thresholds[m_nextThresholdIndex];
            ++m_nextThresholdIndex;
            ++m_currentPhase;
            phaseupdate.phaseAdvanced = true;
        }

        return phaseupdate;

    }

    PhaseManager::PhaseUpdateResult PhaseManager::UpdateTime(float deltaSeconds)
    {
        (void)deltaSeconds;
        PhaseUpdateResult phaseupdate = {};
        phaseupdate.phaseAdvanced = false;
        return phaseupdate;
    }
    
    int PhaseManager::GetCurrentPhase() const noexcept
    {
        return m_currentPhase;
    }

    float PhaseManager::GetRewardMultiplier() const noexcept
    {
        return GetRewardMultiplierForPhase(m_currentPhase);
    }

    float PhaseManager::GetRewardMultiplierForPhase(int phaseupdateIndex) const noexcept
    {
        if (m_rewardMultipliers.empty())
        {
            return 1.0f;
        }

        if (phaseupdateIndex < 0)
        {
            return m_rewardMultipliers.front();
        }

        const std::size_t index = static_cast<std::size_t>(phaseupdateIndex);
        if (index >= m_rewardMultipliers.size())
        {
            return m_rewardMultipliers.back();
        }

        const float value = m_rewardMultipliers[index];
        if (!std::isfinite(value) || value <= 0.0f)
        {
            return 1.0f;
        }
        return value;
    }

}
