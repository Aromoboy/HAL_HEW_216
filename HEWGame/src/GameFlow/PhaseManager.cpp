#include "GameFlow/PhaseManager.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace GameFlow
{
    namespace
    {
        constexpr float kAutoThresholdFallbackStep = 10.0f;
    }

    PhaseManager::PhaseManager()
        : m_nextThresholdIndex(0)
        , m_currentPhase(0)
        , m_nextMiniThresholdIndex(0)
        , m_currentMiniPhase(0)
        , m_initialized(false)
        , m_lastReportedHeight(0.0f)
        , m_elapsedSeconds(0.0f)
    {
    }

    void PhaseManager::Initialize(const std::vector<float>& thresholds,const std::vector<float>& rewardMultipliers, 
                                  const std::vector<float>& minithresholds, const std::vector<float>& rewardMiniMultipliers)
    {
        m_thresholds = thresholds;
        m_minithresholds = minithresholds; 
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

        //Mini Phase
        std::sort(m_minithresholds.begin(), m_minithresholds.end());
        m_minithresholds.erase(
            std::remove_if(m_minithresholds.begin(), m_minithresholds.end(), [](float value)
                {
                    return !std::isfinite(value) || value <= 0.0f;
                }),
            m_minithresholds.end());
        m_minithresholds.erase(std::unique(m_minithresholds.begin(), m_minithresholds.end()), m_minithresholds.end());

        m_minirewardMultipliers = rewardMiniMultipliers;
        if (m_minirewardMultipliers.empty())
        {
            m_minirewardMultipliers.push_back(1.0f);
        }
        const std::size_t requiredMiniMultiplierCount = m_minithresholds.size() + 1;
        if (m_minirewardMultipliers.size() < requiredMiniMultiplierCount)
        {
            const float fillValue = m_minirewardMultipliers.back();
            m_minirewardMultipliers.resize(requiredMiniMultiplierCount, fillValue);
        }
        else if (m_minirewardMultipliers.size() > requiredMiniMultiplierCount)
        {
            m_minirewardMultipliers.resize(requiredMiniMultiplierCount);
        }
        
        m_nextThresholdIndex = 0;
        m_nextMiniThresholdIndex = 0; //Mini Phase
        m_currentPhase = 0;
        m_currentMiniPhase = 0; //Mini Phase
        m_initialized = true;
        m_lastReportedHeight = 0.0f;
        m_elapsedSeconds = 0.0f;
        m_startTime = std::chrono::steady_clock::now();

        std::ostringstream oss;
        oss << kLogPrefix << "Initialized with " << m_thresholds.size() << " thresholds"
            << " and " << m_rewardMultipliers.size() << " multipliers.";
        if (!m_thresholds.empty())
        {
            oss << " nextThreshold=" << m_thresholds.front();
        }
        oss << " currentMultiplier=" << GetRewardMultiplier();
        oss << "\n";
        //Mini Phase
        oss << kLogPrefix << "Initialized with " << m_minithresholds.size() << " minithresholds"
            << " and " << m_minirewardMultipliers.size() << " minimultipliers.";
        if (!m_minithresholds.empty())
        {
            oss << " nextminiThreshold=" << m_minithresholds.front();
        }
        oss << " currentMultiplier=" << GetRewardMultiplier();
        oss << "\n";
        DebugTrace(oss.str());
    }

    void PhaseManager::Reset()
    {
        if (!m_initialized)
        {
            return;
        }

        m_nextThresholdIndex = 0;
        m_currentPhase = 0;
        //Mini Phase
        m_nextMiniThresholdIndex = 0;
        m_currentMiniPhase = 0;
        m_lastReportedHeight = 0.0f;
        m_elapsedSeconds = 0.0f;
        m_startTime = std::chrono::steady_clock::now();

        std::ostringstream oss;
        oss << kLogPrefix << "Reset. thresholdCount=" << m_thresholds.size()
            << " multiplierCount=" << m_rewardMultipliers.size();
        if (!m_thresholds.empty())
        {
            oss << " nextThreshold=" << m_thresholds.front();
        }
        oss << " currentMultiplier=" << GetRewardMultiplier();
        oss << "\n";

        //Mini Phase
        oss << kLogPrefix << "Reset. minithresholdCount=" << m_minithresholds.size()
            << " minimultiplierCount=" << m_minirewardMultipliers.size();
        if (!m_minithresholds.empty())
        {
            oss << " nextThreshold=" << m_minithresholds.front();
        }
        oss << " currentMultiplier=" << GetRewardMultiplier();
        oss << "\n";

        DebugTrace(oss.str());
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
        phaseupdate.miniPhaseAdvanced = false;
        phaseupdate.timeElapsed = 0.0f;

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

                std::ostringstream oss;
                oss << kLogPrefix << "auto-threshold=" << (m_thresholds.size() - 1)
                    << " value=" << newThreshold << "\n";
                DebugTrace(oss.str());

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
            phaseupdate.timeElapsed += GetRewardMultiplierForPhase(m_currentPhase);
            LogPhaseAdvance(threshold, m_lastReportedHeight, "height");
        }

        //Mini Phase
        while (m_nextMiniThresholdIndex < m_minithresholds.size()
            && m_lastReportedHeight >= m_minithresholds[m_nextMiniThresholdIndex])
        {
            const float minithreshold = m_minithresholds[m_nextMiniThresholdIndex];
            ++m_nextMiniThresholdIndex;
            ++m_currentMiniPhase;
            phaseupdate.miniPhaseAdvanced = true;
            phaseupdate.timeElapsed += GetRewardMiniMultiplierForPhase(m_currentMiniPhase);
            LogPhaseAdvance(minithreshold, m_lastReportedHeight, "height");
        }

        return phaseupdate;

    }

    PhaseManager::PhaseUpdateResult PhaseManager::UpdateTime(float deltaSeconds)
    {
        (void)deltaSeconds;
        PhaseUpdateResult phaseupdate = {};
        phaseupdate.phaseAdvanced = false;
        phaseupdate.miniPhaseAdvanced = false;
        phaseupdate.timeElapsed = 0.0f;
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

    //Mini Phase
    float PhaseManager::GetRewardMiniMultiplier() const noexcept
    {
        return GetRewardMiniMultiplierForPhase(m_currentMiniPhase);
    }

    void PhaseManager::LogPhaseAdvance(float threshold, float currentValue, const char* metricLabel) const
    {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();

        std::ostringstream oss;
        oss << kLogPrefix << "{\"phaseupdate\":" << m_currentPhase
            << ",\"threshold\":" << threshold
            << ",\"" << (metricLabel ? metricLabel : "value") << "\":" << currentValue
            << ",\"multiplier\":" << GetRewardMultiplier()
            << ",\"timeMs\":" << elapsedMs << "}\n";
        DebugTrace(oss.str());
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

    //Mini Phase
    float PhaseManager::GetRewardMiniMultiplierForPhase(int phaseupdateminiIndex) const noexcept
    {
        if (m_minirewardMultipliers.empty())
        {
            return 1.0f;
        }

        if (phaseupdateminiIndex < 0)
        {
            return m_minirewardMultipliers.front();
        }

        const std::size_t index = static_cast<std::size_t>(phaseupdateminiIndex);
        if (index >= m_minirewardMultipliers.size())
        {
            return m_minirewardMultipliers.back();
        }

        const float value = m_minirewardMultipliers[index];
        if (!std::isfinite(value) || value <= 0.0f)
        {
            return 1.0f;
        }
        return value;
    }
}
