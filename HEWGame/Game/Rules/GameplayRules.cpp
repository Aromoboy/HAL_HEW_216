#include "GameplayRules.h"

#include "GameFlow/PhaseManager.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace GameplayRules
{
 //   ScoreResult ComputeScore(const ScoreInput& input, ScoreState state)
 //   {
 //       ScoreResult result{};
 //       result.state = std::move(state);

 //       if (!input.phaseManager)
 //       {
 //           result.score = 0;
 //           return result;
 //       }
 //       const int currentPhase = input.phaseManager->GetCurrentPhase();
 //       const int phaseDelta = currentPhase - state.lastCountedPhase;

 //       if (phaseDelta <= 0) return result;

 //       const float rawPhaseMultiplier = input.pointMultiplier;
 ////       const float rawPhaseMultiplier = input.phaseManager->GetRewardMultiplier();
 //       const float phaseMultiplier = (std::isfinite(rawPhaseMultiplier) && rawPhaseMultiplier > 0.0f)
 //           ? rawPhaseMultiplier
 //           : 1.0f;
 //       const float rawPointMultiplier = input.pointMultiplier;
 //       const float pointMultiplier = (std::isfinite(rawPointMultiplier) && rawPointMultiplier > 0.0f)
 //           ? rawPointMultiplier
 //           : 1.0f;
 //       const float totalMultiplier = input.baseScorePerMeter * phaseMultiplier * pointMultiplier;
 //       result.score = static_cast<int>(std::lround(result.state.peakTowerHeight * totalMultiplier));

 //       //   const float totalScoreMultiplier = phaseMultiplier * pointMultiplier;
 //       const float totalScoreMultiplier = input.baseScorePerMeter * phaseMultiplier * pointMultiplier;

 //       //フェーズ超過ボーナス
 //       const int currentPhase = input.phaseManager->GetCurrentPhase();
 //       if (currentPhase > result.state.lastCountedPhase)
 //       {
 //           const int phaseDelta = currentPhase - result.state.lastCountedPhase;

 //           constexpr int kPhaseBonusBase = 200;
 //           const int add =
 //               static_cast<int>(std::lround(phaseDelta * kPhaseBonusBase * pointMultiplier));

 //           result.state.accumulatedPhaseBonus += add;
 //           result.state.lastCountedPhase = currentPhase;
 //       }


 //       //高さスコア
 //       const int heightScore = static_cast<int>(std::lround(result.state.peakTowerHeight * totalMultiplier));
 //       
 //       //最終スコア
 //       result.score = heightScore + result.state.accumulatedPhaseBonus;;
 //       const bool multiplierChanged = !std::isfinite(result.state.lastScoreLogMultiplier)
 //           || std::fabs(result.state.lastScoreLogMultiplier - totalScoreMultiplier) > std::numeric_limits<float>::epsilon();
 //       if (result.score != result.state.lastScoreLogValue || multiplierChanged)
 //       {
 //           if (input.log)
 //           {
 //               std::ostringstream oss;
 //               oss << input.logPrefix << "[Score] peak=" << std::fixed << std::setprecision(2) << result.state.peakTowerHeight
 //                   << "m phase=" << input.phaseManager->GetCurrentPhase()
 //                   << " phaseMultiplier=" << phaseMultiplier
 //                   << " pointMultiplier=" << pointMultiplier
 //                   << " basePerMeter=" << input.baseScorePerMeter
 //                   << " totalMultiplier=" << totalMultiplier
 //                   << " score=" << result.score << "\n";
 //               input.log(oss.str());
 //           }
 //           result.state.lastScoreLogValue = result.score;
 //           result.state.lastScoreLogMultiplier = totalScoreMultiplier;
 //       }

 //       return result;
 //   }
    ScoreResult ComputeScore(const ScoreInput& input, ScoreState state)
    {
        ScoreResult result{};
        result.state = state;

        if (!input.phaseManager)
        {
            result.score = state.accumulatedPhaseBonus;;
            return result;
        }
        const int currentPhase = input.phaseManager->GetCurrentPhase();
        const int phaseDelta = currentPhase - result.state.lastCountedPhase;

        if (phaseDelta <= 0) 
        {
            result.score = result.state.accumulatedPhaseBonus;
            return result;
        }

        const float rawPointMultiplier = input.pointMultiplier;
        const float pointMultiplier = (std::isfinite(rawPointMultiplier) && rawPointMultiplier > 0.0f)
        ? rawPointMultiplier
        : 1.0f;

        constexpr int kPhaseBonusBase = 200;
        const int add = static_cast<int>(std::lround(phaseDelta * kPhaseBonusBase * pointMultiplier));
        //   const float totalScoreMultiplier = phaseMultiplier * pointMultiplier;
        result.state.accumulatedPhaseBonus += add;
        result.state.lastCountedPhase = currentPhase;

        result.score = result.state.accumulatedPhaseBonus;

        //const bool multiplierChanged = !std::isfinite(result.state.lastScoreLogMultiplier)
        //    || std::fabs(result.state.lastScoreLogMultiplier - totalScoreMultiplier) > std::numeric_limits<float>::epsilon();
        //if (result.score != result.state.lastScoreLogValue || multiplierChanged)
        //{
        //    if (input.log)
        //    {
        //        std::ostringstream oss;
        //        oss << input.logPrefix << "[Score] peak=" << std::fixed << std::setprecision(2) << result.state.peakTowerHeight
        //            << "m phase=" << input.phaseManager->GetCurrentPhase()
        //            << " phaseMultiplier=" << phaseMultiplier
        //            << " pointMultiplier=" << pointMultiplier
        //            << " basePerMeter=" << input.baseScorePerMeter
        //            << " totalMultiplier=" << totalMultiplier
        //            << " score=" << result.score << "\n";
        //        input.log(oss.str());
        //    }
        //    result.state.lastScoreLogValue = result.score;
        //    result.state.lastScoreLogMultiplier = totalScoreMultiplier;
        //}

        return result;
    }
}
