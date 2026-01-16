#include "GameplayRules.h"

#include "GameFlow/PhaseManager.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace GameplayRules
{
    ScoreResult ComputeScore(const ScoreInput& input, ScoreState state)
    {
        ScoreResult result{};
        result.state = std::move(state);

        if (!input.phaseManager)
        {
            result.score = 0;
            return result;
        }

        const float rawPhaseMultiplier = input.phaseManager->GetRewardMultiplier();
        const float phaseMultiplier = (std::isfinite(rawPhaseMultiplier) && rawPhaseMultiplier > 0.0f)
            ? rawPhaseMultiplier
            : 1.0f;
        const float totalMultiplier = input.baseScorePerMeter * phaseMultiplier;
        const float sanitizedElapsedSeconds = (std::isfinite(input.elapsedSeconds) && input.elapsedSeconds > 0.0f)
            ? input.elapsedSeconds
            : 0.0f;
        constexpr float kMinTimeMultiplier = 1.0f;
        constexpr float kMaxTimeMultiplier = 1.0e6f;
        const float rawTimeMultiplier = std::pow(12.0f, std::floor(sanitizedElapsedSeconds / 12.0f));
        const float timeMultiplier = std::clamp(rawTimeMultiplier, kMinTimeMultiplier, kMaxTimeMultiplier);
        const float baseScore = (result.state.peakTowerHeight * totalMultiplier) + static_cast<float>(input.killScore);
        result.score = static_cast<int>(std::lround(baseScore * timeMultiplier));

        constexpr float kLogMultiplierEpsilon = 0.01f;
        constexpr int kLogScoreDeltaThreshold = 25;
        const bool phaseMultiplierChanged = !std::isfinite(result.state.lastScoreLogMultiplier)
            || std::fabs(result.state.lastScoreLogMultiplier - phaseMultiplier) > kLogMultiplierEpsilon;
        const bool timeMultiplierChanged = !std::isfinite(result.state.lastScoreLogTimeMultiplier)
            || std::fabs(result.state.lastScoreLogTimeMultiplier - timeMultiplier) > kLogMultiplierEpsilon;
        const bool killScoreChanged = result.state.lastScoreLogKillScore != input.killScore;
        const int scoreDelta = std::abs(result.score - result.state.lastScoreLogValue);
        if (scoreDelta >= kLogScoreDeltaThreshold || phaseMultiplierChanged || timeMultiplierChanged || killScoreChanged)
        {
            if (input.log)
            {
                std::ostringstream oss;
                oss << input.logPrefix << "[Score] peak=" << std::fixed << std::setprecision(2) << result.state.peakTowerHeight
                    << "m phase=" << input.phaseManager->GetCurrentPhase()
                    << " phaseMultiplier=" << phaseMultiplier
                    << " timeMultiplier=" << timeMultiplier
                    << " basePerMeter=" << input.baseScorePerMeter
                    << " killScore=" << input.killScore
                    << " totalMultiplier=" << totalMultiplier
                    << " baseScore=" << baseScore
                    << " score=" << result.score << "\n";
                input.log(oss.str());
            }
            result.state.lastScoreLogValue = result.score;
            result.state.lastScoreLogMultiplier = phaseMultiplier;
            result.state.lastScoreLogTimeMultiplier = timeMultiplier;
            result.state.lastScoreLogKillScore = input.killScore;
        }

        return result;
    }
}
