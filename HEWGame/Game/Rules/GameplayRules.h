// ゲームのルール計算に必要なデータをまとめている
// ルール判定やスコア計算から参照される

#ifndef __GAMEPLAY_RULES_H__
#define __GAMEPLAY_RULES_H__

#include <functional>
#include <string>

namespace GameFlow
{
    class PhaseManager;
}

namespace GameplayRules
{
    using LogFn = std::function<void(const std::string&)>;

    struct ScoreState
    {
        float peakTowerHeight = 0.0f;
        int lastScoreLogValue = 0;
        float lastScoreLogMultiplier = 0.0f;
    };

    struct ScoreInput
    {
        GameFlow::PhaseManager* phaseManager = nullptr;
        float baseScorePerMeter = 0.0f;
        LogFn log;
        const char* logPrefix = "";
    };

    struct ScoreResult
    {
        int score = 0;
        ScoreState state;
    };

    ScoreResult ComputeScore(const ScoreInput& input, ScoreState state);
}

#endif
