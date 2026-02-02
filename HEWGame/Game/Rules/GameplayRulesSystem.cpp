#include "GameplayRulesSystem.h"

#include "SweetsManager.h"
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
    m_remainingCountdownSeconds = m_totalPlaySeconds;
    m_lastAnnouncedCountdownSeconds = static_cast<int>(std::ceil(m_totalPlaySeconds));
}

void GameplayRulesSystem::ResetSession()
{
    m_countdownTimer.Reset();
    m_totalPlaySeconds = m_defaultPlaySeconds;
    m_remainingCountdownSeconds = m_totalPlaySeconds;
    m_lastAnnouncedCountdownSeconds = static_cast<int>(std::ceil(m_totalPlaySeconds));
    m_currentTowerHeight = 0.0f;
    m_peakTowerHeight = 0.0f;
    m_maxTowerHeight = 0.0f;
    m_phaseUpCount = 0;
    //スコア
    m_scoreState = GameplayRules::ScoreState{};

    m_isGameClear = false;
    m_pendingTimeBonuses.clear();
}

void GameplayRulesSystem::SetScoreMultiplier(float multiplier)
{
    if (!(multiplier > 0.0f) || !std::isfinite(multiplier))
    {
        m_scoreMultiplier = 1.0f;
        return;
    }

    m_scoreMultiplier = multiplier;
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
        m_remainingCountdownSeconds,
        m_lastAnnouncedCountdownSeconds,
        m_pendingTimeBonuses
    };
    GameplayProgression::CountdownResult countdownResult =
        GameplayProgression::UpdateCountdown(countdownInput, std::move(countdownState));
    m_totalPlaySeconds = countdownResult.state.totalSeconds;
    m_remainingCountdownSeconds = countdownResult.state.remainingSeconds;
    m_lastAnnouncedCountdownSeconds = countdownResult.state.lastAnnouncedSeconds;
    m_pendingTimeBonuses = std::move(countdownResult.state.pendingBonuses);

    const GameplayProgression::TowerTelemetryInput towerTelemetryInput{
        input.sweetsManager,
        input.phaseManager,
        input.clearPhaseCount,
        m_remainingCountdownSeconds,
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

    //フェーズ到達時にスコア確定
    if (towerTelemetryResult.phaseAdvanced)
    {
        ComputeScore(input.phaseManager);
    }

    UpdateResult result{};
    result.timeUp = countdownResult.timeUp;
    result.phaseAdvanced = towerTelemetryResult.phaseAdvanced;
    result.previousPhase = towerTelemetryResult.previousPhase;
    result.currentPhase = towerTelemetryResult.currentPhase;
    return result;
}

//int GameplayRulesSystem::ComputeScore(GameFlow::PhaseManager* phaseManager)
//{
////    return ComputeScore(phaseManager, m_peakTowerHeight);
//    return ComputeScore(phaseManager);
//}

//int GameplayRulesSystem::ComputeScore(GameFlow::PhaseManager* phaseManager, float peakHeight)
int GameplayRulesSystem::ComputeScore(GameFlow::PhaseManager* phaseManager)
{
    //m_scoreState.peakTowerHeight = peakHeight;

    const GameplayRules::ScoreInput scoreInput{
        phaseManager,
        m_baseScorePerMeter,
        m_scoreMultiplier,
        m_log,
        m_logPrefix
    };

    GameplayRules::ScoreResult scoreResult =
        GameplayRules::ComputeScore(scoreInput, m_scoreState);
    //GameplayRules::ScoreState scoreState{
    //    peakHeight,
    //    m_lastScoreLogValue,
    //    m_lastScoreLogMultiplier
    //};
    //GameplayRules::ScoreResult scoreResult = GameplayRules::ComputeScore(scoreInput, scoreState);
    //m_lastScoreLogValue = scoreResult.state.lastScoreLogValue;
    //m_lastScoreLogMultiplier = scoreResult.state.lastScoreLogMultiplier;
    
    //状態を保存
    m_scoreState = scoreResult.state;
    
    return scoreResult.score;
}

float GameplayRulesSystem::GetTotalPlaySeconds() const
{
    return m_totalPlaySeconds;
}

float GameplayRulesSystem::GetRemainingCountdownSeconds() const
{
    return m_remainingCountdownSeconds;
}

int GameplayRulesSystem::GetLastAnnouncedCountdownSeconds() const
{
    return m_lastAnnouncedCountdownSeconds;
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

int GameplayRulesSystem::GetCurrentScore() const
{
    return m_scoreState.accumulatedPhaseBonus;
}