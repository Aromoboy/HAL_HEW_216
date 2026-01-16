#ifndef __GAMEPLAY_PROGRESSION_H__
#define __GAMEPLAY_PROGRESSION_H__

#include <deque>
#include <functional>
#include <string>
#include "Meteors/MeteorsTypes.h"

namespace Core
{
    class StepTimer;
}

namespace GameFlow
{
    class PhaseManager;
}

namespace MeteorsSpace
{
    class MeteorsManager;
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
        float elapsedSeconds = 0.0f;
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
    };

    CountdownResult UpdateCountdown(const CountdownInput& input, CountdownState state);

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
        MeteorsSpace::MeteorsManager* meteorsManager = nullptr;
        GameFlow::PhaseManager* phaseManager = nullptr;
        int clearPhaseCount = 0;
        float elapsedPlaySeconds = 0.0f;
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
