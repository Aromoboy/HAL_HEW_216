#pragma once

#include "GameplayProgression.h"
#include "Vector.h"
#include <deque>
#include <memory>
#include <unordered_set>

class GuidelineActor;

namespace SweetsSpace
{
    class SweetsManager;
}

class GuidelineSystem
{
public:
    struct Config
    {
        float heightMeters = 0.0f;
        float stepMeters = 0.0f;
        float thicknessPixels = 0.0f;
        float widthPaddingWorld = 0.0f;
        float zOffset = 0.0f;
        float3 spinAxis{ 0.0f, 0.0f, 0.0f };
        float spinSpeedDegPerSec = 0.0f;
        float cooldownDurationSeconds = 0.0f;
        float cooldownRemainingSeconds = 0.0f;
        float centerX = 0.0f;
        float widthWorld = 0.0f;
        float thicknessWorld = 0.0f;
    };

    struct UpdateInput
    {
        SweetsSpace::SweetsManager* sweetsManager = nullptr;
        float timeBonusSeconds = 0.0f;
        float globalCooldownSeconds = 0.0f;
        GameplayProgression::LogFn log;
        const char* logPrefix = "";
    };

    struct UpdateResult
    {
        std::deque<GameplayProgression::PendingTimeBonus> pendingBonuses;
        bool guidelineAdvanced = false;
        float guidelineHeightMeters = 0.0f;
    };

    GuidelineSystem() = default;

    void Configure(const Config& config, std::unique_ptr<GuidelineActor> actor);
    void ResetHeight(float heightMeters, float stepMeters);
    void ResetSession();
    void TickCooldowns(float deltaSeconds);
    UpdateResult Update(const UpdateInput& input,
                        std::deque<GameplayProgression::PendingTimeBonus> pendingBonuses);
#if defined(_DEBUG)
    UpdateResult DebugForceAdvance(const UpdateInput& input,
                                   std::deque<GameplayProgression::PendingTimeBonus> pendingBonuses,
                                   float triggerHeight);
#endif

    float GetHeightMeters() const;
    float GetStepMeters() const;
    float GetCenterX() const;
    float GetCooldownRemainingSeconds() const;
    float GetCooldownDurationSeconds() const;
    float GetTriggerCooldownSeconds() const;

    GuidelineActor* GetActor() const;

private:
    float m_heightMeters = 0.0f;
    float m_stepMeters = 0.0f;
    float m_thicknessPixels = 0.0f;
    float m_widthPaddingWorld = 0.0f;
    float m_zOffset = 0.0f;
    float m_centerX = 0.0f;
    float m_widthWorld = 0.0f;
    float m_thicknessWorld = 0.0f;
    float m_cooldownDurationSeconds = 0.0f;
    float m_cooldownRemainingSeconds = 0.0f;
    float m_triggerCooldownSeconds = 0.0f;
    int m_guidelineCrossCount = 0;
    std::unordered_set<SweetsSpace::ActiveSweets*> m_triggeredThisHeight;
    std::unique_ptr<GuidelineActor> m_guidelineActor;
};
