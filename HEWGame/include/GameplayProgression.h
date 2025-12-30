#ifndef __GAMEPLAY_PROGRESSION_H__
#define __GAMEPLAY_PROGRESSION_H__

#include <deque>
#include <functional>
#include <string>
#include <unordered_set>

namespace Core
{
    class StepTimer;
}

namespace GameFlow
{
    class PhaseManager;
}

class GuidelineActor;

namespace SweetsSpace
{
    class SweetsManager;
    class BaseSweets;
    using ActiveSweets = BaseSweets;
}

namespace GameplayProgression
{
    using LogFn = std::function<void(const std::string&)>;

    struct PendingTimeBonus
    {
        float seconds = 0.0f;
        float triggerHeight = 0.0f;
    };

    struct CountdownState
    {
        float totalSeconds = 0.0f;
        float remainingSeconds = 0.0f;
        int lastAnnouncedSeconds = 0;
        std::deque<PendingTimeBonus> pendingBonuses;
    };

    struct CountdownInput
    {
        bool isPlaying = false;
        float deltaSeconds = 0.0f;
        Core::StepTimer* timer = nullptr;
        float logIntervalSeconds = 0.0f;
        LogFn log;
        const char* logPrefix = "";
    };

    struct CountdownResult
    {
        CountdownState state;
        bool timeUp = false;
    };

    CountdownResult UpdateCountdown(const CountdownInput& input, CountdownState state);

    struct GuidelineState
    {
        float heightMeters = 0.0f;
        float stepMeters = 0.0f;
        float cooldownRemainingSeconds = 0.0f;
        float cooldownDurationSeconds = 0.0f;
        float triggerCooldownSeconds = 0.0f;
        std::unordered_set<SweetsSpace::ActiveSweets*> triggeredThisHeight;
    };

    struct GuidelineInput
    {
        SweetsSpace::SweetsManager* sweetsManager = nullptr;
        GuidelineActor* guidelineActor = nullptr;
        float guidelineCenterX = 0.0f;
        float timeBonusSeconds = 0.0f;
        float globalCooldownSeconds = 0.0f;
        LogFn log;
        const char* logPrefix = "";
    };

    struct GuidelineCheckResult
    {
        GuidelineState state;
        bool crossed = false;
        float triggerHeight = 0.0f;
    };

    GuidelineCheckResult CheckGuidelineCrossing(const GuidelineInput& input, GuidelineState state);
    GuidelineState OnGuidelineCrossed(const GuidelineInput& input, GuidelineState state);
    std::deque<PendingTimeBonus> ApplyGuidelineBonus(const GuidelineInput& input,
                                                     std::deque<PendingTimeBonus> pendingBonuses,
                                                     float triggerHeight);

    struct GuidelineAdvanceResult
    {
        GuidelineState state;
        std::deque<PendingTimeBonus> pendingBonuses;
    };

    GuidelineAdvanceResult HandleGuidelineAdvance(const GuidelineInput& input,
                                                  GuidelineState state,
                                                  std::deque<PendingTimeBonus> pendingBonuses,
                                                  float triggerHeight);

    struct TowerTelemetryState
    {
        float currentHeight = 0.0f;
        float peakHeight = 0.0f;
        float maxHeight = 0.0f;
        int phaseUpCount = 0;
        bool isGameClear = false;
    };

    struct TowerTelemetryInput
    {
        SweetsSpace::SweetsManager* sweetsManager = nullptr;
        GameFlow::PhaseManager* phaseManager = nullptr;
        int clearPhaseCount = 0;
        float remainingCountdownSeconds = 0.0f;
        LogFn log;
        const char* logPrefix = "";
    };

    struct TowerTelemetryResult
    {
        TowerTelemetryState state;
        bool phaseAdvanced = false;
        int previousPhase = 0;
        int currentPhase = 0;
    };

    TowerTelemetryResult UpdateTowerHeightTelemetry(const TowerTelemetryInput& input,
                                                    TowerTelemetryState state);
}

#endif
