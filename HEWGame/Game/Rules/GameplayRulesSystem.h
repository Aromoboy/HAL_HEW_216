#pragma once

#include "GameplayProgression.h"
#include "GameplayRules.h"
#include "Core/StepTimer.h"

#include <deque>
#include <limits>

namespace GameFlow
{
    class PhaseManager;
}

namespace SweetsSpace
{
    class SweetsManager;
}

class GameplayRulesSystem
{
public:
    struct Config
    {
        float defaultPlaySeconds = 0.0f;
        float countdownLogIntervalSeconds = 0.0f;
        float baseScorePerMeter = 0.0f;
    };

    struct UpdateInput
    {
        bool isPlaying = false;
        float deltaSeconds = 0.0f;
        SweetsSpace::SweetsManager* sweetsManager = nullptr;
        GameFlow::PhaseManager* phaseManager = nullptr;
        int clearPhaseCount = 0;
    };

    struct UpdateResult
    {
        bool timeUp = false;
        bool phaseAdvanced = false;
        int previousPhase = 0;
        int currentPhase = 0;
    };

    GameplayRulesSystem();

    void Configure(const Config& config, GameplayProgression::LogFn log, const char* logPrefix);
    void ResetSession();
    void SetScoreMultiplier(float multiplier);
    void AddPendingBonuses(std::deque<GameplayProgression::PendingTimeBonus> pendingBonuses);
    UpdateResult Update(const UpdateInput& input);

    int ComputeScore(GameFlow::PhaseManager* phaseManager);
    int ComputeScore(GameFlow::PhaseManager* phaseManager, float peakHeight);

    float GetTotalPlaySeconds() const;
    float GetRemainingCountdownSeconds() const;
    int GetLastAnnouncedCountdownSeconds() const;
    float GetCurrentTowerHeight() const;
    float GetPeakTowerHeight() const;
    float GetMaxTowerHeight() const;
    int GetPhaseUpCount() const;
    bool IsGameClear() const;

private:
    float m_defaultPlaySeconds = 0.0f;
    float m_countdownLogIntervalSeconds = 0.0f;
    float m_baseScorePerMeter = 0.0f;
    float m_scoreMultiplier = 1.0f;
    GameplayProgression::LogFn m_log;
    const char* m_logPrefix = "";

    Core::StepTimer m_countdownTimer;
    float m_totalPlaySeconds = 0.0f;
    float m_remainingCountdownSeconds = 0.0f;
    int m_lastAnnouncedCountdownSeconds = 0;
    std::deque<GameplayProgression::PendingTimeBonus> m_pendingTimeBonuses;

    float m_currentTowerHeight = 0.0f;
    float m_peakTowerHeight = 0.0f;
    float m_maxTowerHeight = 0.0f;
    int m_phaseUpCount = 0;
    bool m_isGameClear = false;

    int m_lastScoreLogValue = std::numeric_limits<int>::min();
    float m_lastScoreLogMultiplier = std::numeric_limits<float>::quiet_NaN();
};
