#include "GameFlow/PhaseManager.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    void DebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr const char* kLogPrefix = "[PhaseManager] ";
}

namespace GameFlow
{
    PhaseManager::PhaseManager()
        : m_nextThresholdIndex(0)
        , m_currentPhase(0)
        , m_initialized(false)
        , m_lastReportedHeight(0.0f)
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
        m_lastReportedHeight = 0.0f;
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
        DebugTrace(oss.str());
    }


    bool PhaseManager::UpdateHeight(float currentMaxHeight)
    {
        if (!m_initialized)
        {
            return false;
        }

        if (!std::isfinite(currentMaxHeight))
        {
            return false;
        }

        m_lastReportedHeight = std::max(m_lastReportedHeight, currentMaxHeight);

        bool advanced = false;
        while (m_nextThresholdIndex < m_thresholds.size()
            && m_lastReportedHeight >= m_thresholds[m_nextThresholdIndex])
        {
            const float threshold = m_thresholds[m_nextThresholdIndex];
            ++m_nextThresholdIndex;
            ++m_currentPhase;
            advanced = true;
            LogPhaseAdvance(threshold, m_lastReportedHeight);
        }

        return advanced;
    }
    
    //ミニフェーズ
    bool PhaseManager::UpdateMiniHeight(float currentMaxMiniHeight)
    {
        if (!m_initialized)
        {
            return false;
        }

    }

    int PhaseManager::GetCurrentPhase() const noexcept
    {
        return m_currentPhase;
    }

    float PhaseManager::GetRewardMultiplier() const noexcept
    {
        return GetRewardMultiplierForPhase(m_currentPhase);
    }

    void PhaseManager::LogPhaseAdvance(float threshold, float currentHeight) const
    {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();

        std::ostringstream oss;
        oss << kLogPrefix << "{\"phase\":" << m_currentPhase
            << ",\"threshold\":" << threshold
            << ",\"height\":" << currentHeight
            << ",\"multiplier\":" << GetRewardMultiplier()
            << ",\"timeMs\":" << elapsedMs << "}\n";
        DebugTrace(oss.str());
    }

    float PhaseManager::GetRewardMultiplierForPhase(int phaseIndex) const noexcept
    {
        if (m_rewardMultipliers.empty())
        {
            return 1.0f;
        }

        if (phaseIndex < 0)
        {
            return m_rewardMultipliers.front();
        }

        const std::size_t index = static_cast<std::size_t>(phaseIndex);
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
