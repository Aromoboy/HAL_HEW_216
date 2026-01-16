#include "GameplayProgression.h"

#include "Core/StepTimer.h"
#include "GameFlow/PhaseManager.h"
#include "MeteorsManager.h"
#include "Meteors/BaseMeteors.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace GameplayProgression
{
    CountdownResult UpdateCountdown(const CountdownInput& input, CountdownState state)
    {
        CountdownResult result{};
        result.state = std::move(state);

        if (!input.isPlaying)
        {
            return result;
        }

        if (!input.timer)
        {
            return result;
        }

        if (!result.state.pendingBonuses.empty())
        {
            while (!result.state.pendingBonuses.empty())
            {
                const PendingTimeBonus bonus = result.state.pendingBonuses.front();
                result.state.pendingBonuses.pop_front();
                if (!(bonus.seconds > 0.0f))
                {
                    if (input.log)
                    {
                        std::ostringstream oss;
                        oss << input.logPrefix << "[Countdown] Ignored non-positive bonus event seconds="
                            << bonus.seconds << " queue=" << result.state.pendingBonuses.size() << "\n";
                        input.log(oss.str());
                    }
                    continue;
                }

                result.state.totalSeconds += bonus.seconds;

                if (input.log)
                {
                    std::ostringstream oss;
                    oss << input.logPrefix << "[Countdown] bonusApplied +=" << std::fixed << std::setprecision(2)
                        << bonus.seconds << "s height=" << std::fixed << std::setprecision(2)
                        << bonus.triggerHeight << "m total=" << std::fixed << std::setprecision(2)
                        << result.state.totalSeconds << "s queue=" << result.state.pendingBonuses.size() << "\n";
                    input.log(oss.str());
                }
            }
        }

        const float delta = input.timer->Tick(input.deltaSeconds);
        if (!(delta > 0.0f))
        {
            return result;
        }

        const float previousElapsed = result.state.elapsedSeconds;
        result.state.elapsedSeconds = std::max(0.0f, result.state.elapsedSeconds + delta);

        const int previousSeconds = static_cast<int>(std::floor(previousElapsed));
        const int currentSeconds = static_cast<int>(std::floor(result.state.elapsedSeconds));
        if (currentSeconds > previousSeconds)
        {
            const int intervalSeconds = std::max(1, static_cast<int>(input.logIntervalSeconds));
            const bool shouldLogInterval = (currentSeconds % intervalSeconds == 0);
            if (shouldLogInterval && currentSeconds != result.state.lastAnnouncedSeconds)
            {
                if (input.log)
                {
                    std::ostringstream oss;
                    oss << input.logPrefix << "[Countdown] elapsed=" << std::fixed << std::setprecision(2)
                        << result.state.elapsedSeconds << "s\n";
                    input.log(oss.str());
                }
                result.state.lastAnnouncedSeconds = currentSeconds;
            }
        }

        return result;
    }

    TowerTelemetryResult UpdateTowerHeightTelemetry(const TowerTelemetryInput& input,
                                                    TowerTelemetryState state)
    {
        TowerTelemetryResult result{};
        result.state = std::move(state);

        if (!input.meteorsManager)
        {
            if (input.log)
            {
                input.log("[ERROR] m_meteorsManager is nullptr!\n");
            }
            return result;
        }

        const MeteorsSpace::MeteorsState states[] = {
            MeteorsSpace::MeteorsState::Active,
            MeteorsSpace::MeteorsState::Stacked,
            MeteorsSpace::MeteorsState::Ground,
        };

        float highestTop = 0.0f;

        for (MeteorsSpace::MeteorsState stateValue : states)
        {
            auto& list = input.meteorsManager->GetByState(stateValue);
            for (const auto& meteors : list)
            {
                if (!meteors || meteors->IsPendingDestroy())
                {
                    continue;
                }

                meteors->SyncColliderBounds();
                const AABB bounds = meteors->GetAABB();
                highestTop = std::max(highestTop, bounds.max.y);
            }
        }

        result.state.currentHeight = highestTop;
        result.state.maxHeight = std::max(result.state.maxHeight, highestTop);

        if (input.phaseManager)
        {
            const int previousPhase = input.phaseManager->GetCurrentPhase();
            const GameFlow::PhaseManager::PhaseUpdateResult phaseUpdate =
                input.phaseManager->UpdateHeight(result.state.maxHeight);
            const int currentPhase = input.phaseManager->GetCurrentPhase();
            if (phaseUpdate.phaseAdvanced && currentPhase > previousPhase)
            {
                result.state.phaseUpCount += currentPhase - previousPhase;
                if (result.state.phaseUpCount >= input.clearPhaseCount)
                {
                    result.state.isGameClear = true;
                }
                result.phaseAdvanced = true;
                result.previousPhase = previousPhase;
                result.currentPhase = currentPhase;
            }
        }

        if (highestTop > result.state.peakHeight)
        {
            result.state.peakHeight = highestTop;
            if (input.log)
            {
                std::ostringstream oss;
                oss << input.logPrefix << "[Tower] newPeakTop=" << std::fixed << std::setprecision(2)
                    << result.state.peakHeight << "m elapsed=" << std::fixed << std::setprecision(2)
                    << input.elapsedPlaySeconds << "s\n";
                input.log(oss.str());
            }
        }

        return result;
    }
}
