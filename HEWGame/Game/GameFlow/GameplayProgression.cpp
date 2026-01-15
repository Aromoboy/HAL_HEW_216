#include "GameplayProgression.h"

#include "Core/StepTimer.h"
#include "GameFlow/PhaseManager.h"
#include "GuidelineActor.h"
#include "SweetsManager.h"
#include "Sweets/BaseSweets.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace
{
    constexpr float kGuidelineHeightIncrementMeters = 1.75f;
}

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

                result.state.remainingSeconds = std::max(0.0f, result.state.remainingSeconds);
                result.state.remainingSeconds += bonus.seconds;
                result.state.totalSeconds += bonus.seconds;

                if (input.log)
                {
                    std::ostringstream oss;
                    oss << input.logPrefix << "[Countdown] bonusApplied +=" << std::fixed << std::setprecision(2)
                        << bonus.seconds << "s height=" << std::fixed << std::setprecision(2)
                        << bonus.triggerHeight << "m remaining=" << std::fixed << std::setprecision(2)
                        << result.state.remainingSeconds << "s queue=" << result.state.pendingBonuses.size() << "\n";
                    input.log(oss.str());
                }
            }
        }

        const float delta = input.timer->Tick(input.deltaSeconds);
        if (!(delta > 0.0f))
        {
            return result;
        }

        const float previousRemaining = result.state.remainingSeconds;
        result.state.remainingSeconds = std::max(0.0f, result.state.remainingSeconds - delta);

        const int previousSeconds = static_cast<int>(std::ceil(previousRemaining));
        const int currentSeconds = static_cast<int>(std::ceil(result.state.remainingSeconds));
        if (currentSeconds < previousSeconds)
        {
            const int intervalSeconds = std::max(1, static_cast<int>(input.logIntervalSeconds));
            const bool shouldLogInterval = (currentSeconds % intervalSeconds == 0);
            const bool shouldLogWarning = currentSeconds <= 3;
            if ((shouldLogInterval || shouldLogWarning) && currentSeconds != result.state.lastAnnouncedSeconds)
            {
                if (input.log)
                {
                    std::ostringstream oss;
                    oss << input.logPrefix << "[Countdown] remaining=" << std::fixed << std::setprecision(2)
                        << result.state.remainingSeconds << "s\n";
                    input.log(oss.str());
                }
                result.state.lastAnnouncedSeconds = currentSeconds;
            }
        }

        if (result.state.remainingSeconds <= 0.0f)
        {
            result.timeUp = true;
        }

        return result;
    }

    GuidelineCheckResult CheckGuidelineCrossing(const GuidelineInput& input, GuidelineState state)
    {
        GuidelineCheckResult result{};
        result.state = std::move(state);
        result.triggerHeight = 0.0f;

        if (!input.sweetsManager)
        {
            return result;
        }

        if (!(result.state.heightMeters > 0.0f))
        {
            return result;
        }

        if (result.state.cooldownRemainingSeconds > 0.0f)
        {
            return result;
        }

        const SweetsSpace::SweetsState states[] = {
            SweetsSpace::SweetsState::Active,
            SweetsSpace::SweetsState::Stacked,
            SweetsSpace::SweetsState::Ground,
            SweetsSpace::SweetsState::Carried,
        };

        std::shared_ptr<SweetsSpace::ActiveSweets> crossedSweets;

        for (SweetsSpace::SweetsState stateValue : states)
        {
            if (stateValue == SweetsSpace::SweetsState::Carried)
            {
                continue;
            }

            auto& list = input.sweetsManager->GetByState(stateValue);
            for (const auto& sweets : list)
            {
                if (!sweets || sweets->IsPendingDestroy())
                {
                    continue;
                }

                if (!input.sweetsManager->HasEverCollidedWithSweets(*sweets))
                {
                    continue;
                }

                bool foundationOnly = false;
                if (!input.sweetsManager->HasSupportingContact(*sweets, &foundationOnly))
                {
                    continue;
                }

                if (!foundationOnly)
                {
                    continue;
                }

                sweets->SyncColliderBounds();
                const AABB bounds = sweets->GetAABB();
                if (bounds.max.y >= result.state.heightMeters)
                {
                    if (result.state.triggeredThisHeight.insert(sweets.get()).second)
                    {
                        crossedSweets = sweets;
                        result.triggerHeight = bounds.max.y;
                        break;
                    }
                }
            }

            if (crossedSweets)
            {
                break;
            }
        }

        if (crossedSweets)
        {
            result.state.cooldownRemainingSeconds = result.state.cooldownDurationSeconds;
            input.sweetsManager->OnSweetsPassedGuideline(crossedSweets);
            result.crossed = true;
            return result;
        }

        return result;
    }

    GuidelineState OnGuidelineCrossed(const GuidelineInput& input, GuidelineState state)
    {
        if (!(state.heightMeters > 0.0f) || !(state.stepMeters > 0.0f))
        {
            state.triggerCooldownSeconds = input.globalCooldownSeconds;
            return state;
        }

        state.heightMeters += kGuidelineHeightIncrementMeters;
        if (input.guidelineActor)
        {
            auto& transform = input.guidelineActor->GetTransform();
            transform.position.x = input.guidelineCenterX;
            transform.position.y = state.heightMeters;
        }
        state.triggeredThisHeight.clear();
        state.cooldownRemainingSeconds = 0.0f;
        state.triggerCooldownSeconds = input.globalCooldownSeconds;
        return state;
    }

    std::deque<PendingTimeBonus> ApplyGuidelineBonus(const GuidelineInput& input,
                                                     std::deque<PendingTimeBonus> pendingBonuses,
                                                     float triggerHeight)
    {
        if (!(input.timeBonusSeconds > 0.0f))
        {
            return pendingBonuses;
        }

        PendingTimeBonus bonus{};
        bonus.seconds = input.timeBonusSeconds;
        bonus.triggerHeight = triggerHeight;
        pendingBonuses.push_back(bonus);

        if (input.log)
        {
            std::ostringstream oss;
            oss << input.logPrefix << "[Countdown] guidelineBonus +=" << std::fixed << std::setprecision(2)
                << bonus.seconds << "s triggerHeight=" << triggerHeight
                << "m queue=" << pendingBonuses.size() << "\n";
            input.log(oss.str());
        }

        return pendingBonuses;
    }

    GuidelineAdvanceResult HandleGuidelineAdvance(const GuidelineInput& input,
                                                  GuidelineState state,
                                                  std::deque<PendingTimeBonus> pendingBonuses,
                                                  float triggerHeight)
    {
        GuidelineAdvanceResult result{};
        result.pendingBonuses = ApplyGuidelineBonus(input, std::move(pendingBonuses), triggerHeight);
        result.state = OnGuidelineCrossed(input, std::move(state));
        return result;
    }

    TowerTelemetryResult UpdateTowerHeightTelemetry(const TowerTelemetryInput& input,
                                                    TowerTelemetryState state)
    {
        TowerTelemetryResult result{};
        result.state = std::move(state);

        if (!input.sweetsManager)
        {
            if (input.log)
            {
                input.log("[ERROR] m_sweetsManager is nullptr!\n");
            }
            return result;
        }

        const SweetsSpace::SweetsState states[] = {
            SweetsSpace::SweetsState::Active,
            SweetsSpace::SweetsState::Stacked,
            SweetsSpace::SweetsState::Ground,
        };

        float highestTop = 0.0f;

        for (SweetsSpace::SweetsState stateValue : states)
        {
            auto& list = input.sweetsManager->GetByState(stateValue);
            for (const auto& sweets : list)
            {
                if (!sweets || sweets->IsPendingDestroy())
                {
                    continue;
                }

                sweets->SyncColliderBounds();
                const AABB bounds = sweets->GetAABB();
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
                    << result.state.peakHeight << "m remaining=" << std::fixed << std::setprecision(2)
                    << input.remainingCountdownSeconds << "s\n";
                input.log(oss.str());
            }
        }

        return result;
    }
}
