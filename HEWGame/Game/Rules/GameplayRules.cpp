// ゲームのルール計算に必要なデータをまとめている
// ルール判定やスコア計算から参照される

#include "GameplayRules.h"

#include "GameFlow/PhaseManager.h"

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
        result.score = static_cast<int>(std::lround(result.state.peakTowerHeight * totalMultiplier));

        const bool multiplierChanged = !std::isfinite(result.state.lastScoreLogMultiplier)
            || std::fabs(result.state.lastScoreLogMultiplier - phaseMultiplier) > std::numeric_limits<float>::epsilon();
        if (result.score != result.state.lastScoreLogValue || multiplierChanged)
        {
            if (input.log)
            {
                std::ostringstream oss;
                oss << input.logPrefix << "[Score] peak=" << std::fixed << std::setprecision(2) << result.state.peakTowerHeight
                    << "m phase=" << input.phaseManager->GetCurrentPhase()
                    << " phaseMultiplier=" << phaseMultiplier
                    << " basePerMeter=" << input.baseScorePerMeter
                    << " totalMultiplier=" << totalMultiplier
                    << " score=" << result.score << "\n";
                input.log(oss.str());
            }
            result.state.lastScoreLogValue = result.score;
            result.state.lastScoreLogMultiplier = phaseMultiplier;
        }

        return result;
    }
}
