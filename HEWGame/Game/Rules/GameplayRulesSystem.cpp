#include "GameplayRulesSystem.h"

#include "MeteorsManager.h"
#include "GameFlow/PhaseManager.h"

#include <cmath>

GameplayRulesSystem::GameplayRulesSystem() = default;

void GameplayRulesSystem::Configure(const Config& config, GameplayProgression::LogFn log, const char* logPrefix)
{
    m_defaultPlaySeconds = config.defaultPlaySeconds;
    m_countdownLogIntervalSeconds = config.countdownLogIntervalSeconds;
    m_baseScorePerMeter = config.baseScorePerMeter;
    m_log = std::move(log);
    m_logPrefix = logPrefix;

    m_totalPlaySeconds = m_defaultPlaySeconds;
    m_elapsedPlaySeconds = 0.0f;
    m_lastAnnouncedElapsedSeconds = 0;
}

void GameplayRulesSystem::ResetSession()
{
    m_countdownTimer.Reset();
    m_totalPlaySeconds = m_defaultPlaySeconds;
    m_elapsedPlaySeconds = 0.0f;
    m_lastAnnouncedElapsedSeconds = 0;
    m_currentTowerHeight = 0.0f;
    m_peakTowerHeight = 0.0f;
    m_maxTowerHeight = 0.0f;
    m_phaseUpCount = 0;
    m_isGameClear = false;
    m_pendingTimeBonuses.clear();
}

void GameplayRulesSystem::AddPendingBonuses(std::deque<GameplayProgression::PendingTimeBonus> pendingBonuses)
{
    if (pendingBonuses.empty())
    {
        return;
    }

    if (m_pendingTimeBonuses.empty())
    {
        m_pendingTimeBonuses = std::move(pendingBonuses);
        return;
    }

    while (!pendingBonuses.empty())
    {
        m_pendingTimeBonuses.push_back(std::move(pendingBonuses.front()));
        pendingBonuses.pop_front();
    }
}

GameplayRulesSystem::UpdateResult GameplayRulesSystem::Update(const UpdateInput& input)
{
    const GameplayProgression::CountdownInput countdownInput{
        input.isPlaying,
        input.deltaSeconds,
        &m_countdownTimer,
        m_countdownLogIntervalSeconds,
        m_log,
        m_logPrefix
    };
    GameplayProgression::CountdownState countdownState{
        m_totalPlaySeconds,
        m_elapsedPlaySeconds,
        m_lastAnnouncedElapsedSeconds,
        m_pendingTimeBonuses
    };
    GameplayProgression::CountdownResult countdownResult =
        GameplayProgression::UpdateCountdown(countdownInput, std::move(countdownState));
    m_totalPlaySeconds = countdownResult.state.totalSeconds;
    m_elapsedPlaySeconds = countdownResult.state.elapsedSeconds;
    m_lastAnnouncedElapsedSeconds = countdownResult.state.lastAnnouncedSeconds;
    m_pendingTimeBonuses = std::move(countdownResult.state.pendingBonuses);

    const GameplayProgression::TowerTelemetryInput towerTelemetryInput{
        input.meteorsManager,
        input.phaseManager,
        input.clearPhaseCount,
        m_elapsedPlaySeconds,
        m_log,
        m_logPrefix
    };
    GameplayProgression::TowerTelemetryState towerTelemetryState{
        m_currentTowerHeight,
        m_peakTowerHeight,
        m_maxTowerHeight,
        m_phaseUpCount,
        m_isGameClear
    };
    GameplayProgression::TowerTelemetryResult towerTelemetryResult =
        GameplayProgression::UpdateTowerHeightTelemetry(towerTelemetryInput, std::move(towerTelemetryState));
    m_currentTowerHeight = towerTelemetryResult.state.currentHeight;
    m_peakTowerHeight = towerTelemetryResult.state.peakHeight;
    m_maxTowerHeight = towerTelemetryResult.state.maxHeight;
    m_phaseUpCount = towerTelemetryResult.state.phaseUpCount;
    m_isGameClear = towerTelemetryResult.state.isGameClear;

    UpdateResult result{};
    result.phaseAdvanced = towerTelemetryResult.phaseAdvanced;
    result.previousPhase = towerTelemetryResult.previousPhase;
    result.currentPhase = towerTelemetryResult.currentPhase;
    return result;
}

int GameplayRulesSystem::ComputeScore(GameFlow::PhaseManager* phaseManager)
{
    return ComputeScore(phaseManager, m_peakTowerHeight);
}

int GameplayRulesSystem::ComputeScore(GameFlow::PhaseManager* phaseManager, float peakHeight)
{
    const GameplayRules::ScoreInput scoreInput{
        phaseManager,
        m_baseScorePerMeter,
        m_log,
        m_logPrefix
    };
    GameplayRules::ScoreState scoreState{
        peakHeight,
        m_lastScoreLogValue,
        m_lastScoreLogMultiplier
    };
    GameplayRules::ScoreResult scoreResult = GameplayRules::ComputeScore(scoreInput, scoreState);
    m_lastScoreLogValue = scoreResult.state.lastScoreLogValue;
    m_lastScoreLogMultiplier = scoreResult.state.lastScoreLogMultiplier;
    return scoreResult.score;
}

float GameplayRulesSystem::GetTotalPlaySeconds() const
{
    return m_totalPlaySeconds;
}

float GameplayRulesSystem::GetElapsedPlaySeconds() const
{
    return m_elapsedPlaySeconds;
}

int GameplayRulesSystem::GetLastAnnouncedElapsedSeconds() const
{
    return m_lastAnnouncedElapsedSeconds;
}

float GameplayRulesSystem::GetCurrentTowerHeight() const
{
    return m_currentTowerHeight;
}

float GameplayRulesSystem::GetPeakTowerHeight() const
{
    return m_peakTowerHeight;
}

float GameplayRulesSystem::GetMaxTowerHeight() const
{
    return m_maxTowerHeight;
}

int GameplayRulesSystem::GetPhaseUpCount() const
{
    return m_phaseUpCount;
}

bool GameplayRulesSystem::IsGameClear() const
{
    return m_isGameClear;
}
