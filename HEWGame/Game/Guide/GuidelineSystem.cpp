#include "GuidelineSystem.h"

#include "GuidelineActor.h"
#include "SweetsManager.h"

#include <algorithm>

void GuidelineSystem::Configure(const Config& config, std::unique_ptr<GuidelineActor> actor)
{
    m_heightMeters = config.heightMeters;
    m_stepMeters = config.stepMeters;
    m_thicknessPixels = config.thicknessPixels;
    m_widthPaddingWorld = config.widthPaddingWorld;
    m_zOffset = config.zOffset;
    m_cooldownDurationSeconds = config.cooldownDurationSeconds;
    m_cooldownRemainingSeconds = config.cooldownRemainingSeconds;
    m_centerX = config.centerX;
    m_widthWorld = config.widthWorld;
    m_thicknessWorld = config.thicknessWorld;
    m_guidelineActor = std::move(actor);
    if (m_guidelineActor)
    {
        m_guidelineActor->SetSpin(config.spinAxis, config.spinSpeedDegPerSec);
    }
}

void GuidelineSystem::ResetHeight(float heightMeters, float stepMeters)
{
    m_heightMeters = heightMeters;
    m_stepMeters = stepMeters;
    if (m_guidelineActor)
    {
        auto& transform = m_guidelineActor->GetTransform();
        transform.position.x = m_centerX;
        transform.position.y = m_heightMeters;
    }
}

void GuidelineSystem::ResetSession()
{
    m_triggeredThisHeight.clear();
    m_cooldownRemainingSeconds = 0.0f;
    m_triggerCooldownSeconds = 0.0f;
}

void GuidelineSystem::TickCooldowns(float deltaSeconds)
{
    if (m_cooldownRemainingSeconds > 0.0f)
    {
        m_cooldownRemainingSeconds = std::max(0.0f, m_cooldownRemainingSeconds - deltaSeconds);
    }

    if (m_triggerCooldownSeconds > 0.0f)
    {
        m_triggerCooldownSeconds = std::max(0.0f, m_triggerCooldownSeconds - deltaSeconds);
    }

    if (m_guidelineActor)
    {
        m_guidelineActor->AdvanceSpin(deltaSeconds);
    }
}

GuidelineSystem::UpdateResult GuidelineSystem::Update(const UpdateInput& input,
                                                      std::deque<GameplayProgression::PendingTimeBonus> pendingBonuses)
{
    bool guidelineAdvanced = false;
    GameplayProgression::GuidelineState guidelineState{
        m_heightMeters,
        m_stepMeters,
        m_cooldownRemainingSeconds,
        m_cooldownDurationSeconds,
        m_triggerCooldownSeconds,
        m_triggeredThisHeight
    };

    const GameplayProgression::GuidelineInput guidelineInput{
        input.sweetsManager,
        m_guidelineActor.get(),
        m_centerX,
        input.timeBonusSeconds,
        input.globalCooldownSeconds,
        input.log,
        input.logPrefix
    };

    if (m_triggerCooldownSeconds <= 0.0f)
    {
        GameplayProgression::GuidelineCheckResult guidelineCheck =
            GameplayProgression::CheckGuidelineCrossing(guidelineInput, std::move(guidelineState));
        guidelineState = std::move(guidelineCheck.state);
        if (guidelineCheck.crossed)
        {
            GameplayProgression::GuidelineAdvanceResult guidelineAdvance =
                GameplayProgression::HandleGuidelineAdvance(guidelineInput,
                                                            std::move(guidelineState),
                                                            std::move(pendingBonuses),
                                                            guidelineCheck.triggerHeight);
            guidelineState = std::move(guidelineAdvance.state);
            pendingBonuses = std::move(guidelineAdvance.pendingBonuses);
            guidelineAdvanced = true;
        }
    }

    m_heightMeters = guidelineState.heightMeters;
    m_stepMeters = guidelineState.stepMeters;
    m_cooldownRemainingSeconds = guidelineState.cooldownRemainingSeconds;
    m_cooldownDurationSeconds = guidelineState.cooldownDurationSeconds;
    m_triggerCooldownSeconds = guidelineState.triggerCooldownSeconds;
    m_triggeredThisHeight = std::move(guidelineState.triggeredThisHeight);

    UpdateResult result{};
    result.pendingBonuses = std::move(pendingBonuses);
    result.guidelineAdvanced = guidelineAdvanced;
    result.guidelineHeightMeters = guidelineAdvanced ? m_heightMeters : 0.0f;
    return result;
}

#if defined(_DEBUG)
GuidelineSystem::UpdateResult GuidelineSystem::DebugForceAdvance(const UpdateInput& input,
                                                                 std::deque<GameplayProgression::PendingTimeBonus> pendingBonuses,
                                                                 float triggerHeight)
{
    GameplayProgression::GuidelineState guidelineState{
        m_heightMeters,
        m_stepMeters,
        m_cooldownRemainingSeconds,
        m_cooldownDurationSeconds,
        m_triggerCooldownSeconds,
        m_triggeredThisHeight
    };

    const GameplayProgression::GuidelineInput guidelineInput{
        input.sweetsManager,
        m_guidelineActor.get(),
        m_centerX,
        input.timeBonusSeconds,
        input.globalCooldownSeconds,
        input.log,
        input.logPrefix
    };

    float resolvedTriggerHeight = std::max(triggerHeight, m_heightMeters);
    if (!(resolvedTriggerHeight > 0.0f))
    {
        resolvedTriggerHeight = m_stepMeters;
    }

    GameplayProgression::GuidelineAdvanceResult guidelineAdvance =
        GameplayProgression::HandleGuidelineAdvance(guidelineInput,
                                                    std::move(guidelineState),
                                                    std::move(pendingBonuses),
                                                    resolvedTriggerHeight);
    guidelineState = std::move(guidelineAdvance.state);

    m_heightMeters = guidelineState.heightMeters;
    m_stepMeters = guidelineState.stepMeters;
    m_cooldownRemainingSeconds = guidelineState.cooldownRemainingSeconds;
    m_cooldownDurationSeconds = guidelineState.cooldownDurationSeconds;
    m_triggerCooldownSeconds = guidelineState.triggerCooldownSeconds;
    m_triggeredThisHeight = std::move(guidelineState.triggeredThisHeight);

    UpdateResult result{};
    result.pendingBonuses = std::move(guidelineAdvance.pendingBonuses);
    result.guidelineAdvanced = true;
    result.guidelineHeightMeters = m_heightMeters;
    return result;
}
#endif

float GuidelineSystem::GetHeightMeters() const
{
    return m_heightMeters;
}

float GuidelineSystem::GetStepMeters() const
{
    return m_stepMeters;
}

float GuidelineSystem::GetCenterX() const
{
    return m_centerX;
}

float GuidelineSystem::GetCooldownRemainingSeconds() const
{
    return m_cooldownRemainingSeconds;
}

float GuidelineSystem::GetCooldownDurationSeconds() const
{
    return m_cooldownDurationSeconds;
}

float GuidelineSystem::GetTriggerCooldownSeconds() const
{
    return m_triggerCooldownSeconds;
}

GuidelineActor* GuidelineSystem::GetActor() const
{
    return m_guidelineActor.get();
}
