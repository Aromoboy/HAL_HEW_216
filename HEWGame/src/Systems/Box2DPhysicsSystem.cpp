#include "Systems/Box2DPhysicsSystem.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <sstream>

#include "Systems/Box2DContactListener.h"

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

namespace
{
        constexpr float kClampSummaryIntervalSeconds = 5.0f;
        constexpr float kClampLogIntervalSeconds = 2.0f;

        void DebugTrace(const std::string& message)
        {
#if defined(_WIN32)
#if defined(_DEBUG)
                ::OutputDebugStringA(message.c_str());
#else
                (void)message;
#endif
#else
                (void)message;
#endif
        }
}

namespace Systems
{
        Box2DPhysicsSystem::Box2DPhysicsSystem(PhysicsMachine& physicsMachine,
                const PhysicsTuningParameters& tuning)
                : m_worldDef(b2DefaultWorldDef())
                , m_physicsMachine(&physicsMachine)
        {
                m_worldDef.gravity = b2Vec2{ 0.0f, -(std::isfinite(tuning.gravity) ? tuning.gravity : 9.8f) };
                m_worldDef.restitutionThreshold = std::max(0.0f, tuning.restitutionThreshold);
                if (tuning.hitEventThreshold > 0.0f)
                {
                        m_worldDef.hitEventThreshold = tuning.hitEventThreshold;
                }

                SetSubStepCount(tuning.subStepCount);
                m_maxAccumulatedSteps = std::clamp(tuning.maxAccumulatedSteps, 1, 120);
                m_maxStepDt = (std::isfinite(tuning.maxStepDt) && tuning.maxStepDt > 0.0f)
                        ? tuning.maxStepDt
                        : (1.0f / 60.0f);

                m_worldId = b2CreateWorld(&m_worldDef);
                if (!b2World_IsValid(m_worldId))
                {
                        DebugTrace("[Box2DPhysicsSystem] Failed to create Box2D world.\n");
                        m_loggedMissingWorld = true;
                }
        }

        Box2DPhysicsSystem::~Box2DPhysicsSystem()
        {
                DestroyWorld();
        }

        void Box2DPhysicsSystem::Apply(float dt)
        {
                if (!b2World_IsValid(m_worldId))
                {
                        if (!m_loggedMissingWorld)
                        {
                                DebugTrace("[Box2DPhysicsSystem] World invalid; skipping step.\n");
                                m_loggedMissingWorld = true;
                        }
                        return;
                }

                const float clampedInput = std::max(0.0f, dt);
                const float maxStep = m_maxStepDt > 0.0f ? m_maxStepDt : clampedInput;
                if (maxStep <= 0.0f)
                {
                        return;
                }

                const float maxAccumulatedTime = maxStep * static_cast<float>(m_maxAccumulatedSteps);
                const float accumulatedBeforeClamp = m_stepAccumulator + clampedInput;
                m_stepAccumulator = std::min(accumulatedBeforeClamp, maxAccumulatedTime);
                m_clampLogElapsed += clampedInput;

                int stepsSimulated = 0;
                while (m_stepAccumulator >= maxStep)
                {
                        b2World_Step(m_worldId, maxStep, m_subStepCount);
                        m_stepAccumulator -= maxStep;
                        ++stepsSimulated;
                }

                if (stepsSimulated == 0 && m_stepAccumulator > 0.0f)
                {
                        b2World_Step(m_worldId, m_stepAccumulator, m_subStepCount);
                        m_stepAccumulator = 0.0f;
                        ++stepsSimulated;
                }

                const bool clampedThisFrame = stepsSimulated > 1 || clampedInput > maxStep || accumulatedBeforeClamp > maxAccumulatedTime;
                HandleClampState(clampedThisFrame, clampedInput, maxStep, stepsSimulated, accumulatedBeforeClamp, maxAccumulatedTime);

        }

        void Box2DPhysicsSystem::HandleClampState(bool clamped, float clampedInput, float maxStep, int stepsSimulated, float accumulatedBeforeClamp, float maxAccumulatedTime)
        {
                if (clamped)
                {
                        if (!m_clampActive)
                        {
                                if (m_clampLogElapsed >= kClampLogIntervalSeconds)
                                {
                                        std::ostringstream oss;
                                        oss << "[Box2DPhysicsSystem] Entering clamped stepping: dt=" << clampedInput
                                                << " -> " << maxStep
                                                << " (accumulator=" << accumulatedBeforeClamp
                                                << "/" << maxAccumulatedTime << ") with " << stepsSimulated << " step(s)\n";
                                        DebugTrace(oss.str());
                                        m_clampLogElapsed = 0.0f;
                                }

                                m_clampActive = true;
                                ++m_summaryClampActivations;
                                m_currentClampFrames = 0;
                                m_currentClampSteps = 0;
                        }

                        ++m_currentClampFrames;
                        m_currentClampSteps += stepsSimulated;
                        ++m_summaryClampFrames;
                        m_summaryClampSteps += stepsSimulated;
                }
                else if (m_clampActive)
                {
                        if (m_clampLogElapsed >= kClampLogIntervalSeconds)
                        {
                                std::ostringstream oss;
                                oss << "[Box2DPhysicsSystem] Returning to unclamped stepping after "
                                        << m_currentClampFrames << " frame(s) and "
                                        << m_currentClampSteps << " step(s); accumulator=" << m_stepAccumulator << "\n";
                                DebugTrace(oss.str());
                                m_clampLogElapsed = 0.0f;
                        }

                        m_clampActive = false;
                        m_currentClampFrames = 0;
                        m_currentClampSteps = 0;
                }
        }

        void Box2DPhysicsSystem::AdvanceClampSummary(float dt)
        {
                const float clampedDt = std::max(0.0f, dt);
                m_summaryElapsed += clampedDt;

                while (m_summaryElapsed >= kClampSummaryIntervalSeconds)
                {
                        if (m_summaryClampActivations > 0 || m_summaryClampFrames > 0 || m_summaryClampSteps > 0)
                        {
                                std::ostringstream oss;
                                oss << "[Box2DPhysicsSystem] Clamp summary (" << kClampSummaryIntervalSeconds << "s): activations="
                                        << m_summaryClampActivations
                                        << " frames=" << m_summaryClampFrames
                                        << " steps=" << m_summaryClampSteps
                                        << (m_clampActive ? " (clamp active)" : "") << "\n";
                                DebugTrace(oss.str());
                        }

                        m_summaryElapsed -= kClampSummaryIntervalSeconds;
                        m_summaryClampActivations = 0;
                        m_summaryClampFrames = 0;
                        m_summaryClampSteps = 0;
                }
        }

        void Box2DPhysicsSystem::HandleTimerReset(std::uint64_t frameIndex)
        {
                if (!m_hasTimerFrame || frameIndex < m_lastTimerFrame)
                {
                        ResetSimulationState();
                }

                m_lastTimerFrame = frameIndex;
                m_hasTimerFrame = true;
        }

        void Box2DPhysicsSystem::ResetSimulationState()
        {
                m_stepAccumulator = 0.0f;
                m_clampActive = false;
                m_clampLogElapsed = 0.0f;
                m_currentClampFrames = 0;
                m_currentClampSteps = 0;
                m_summaryClampActivations = 0;
                m_summaryClampFrames = 0;
                m_summaryClampSteps = 0;
                m_summaryElapsed = 0.0f;
        }

        void Box2DPhysicsSystem::PrepareFrame(std::uint64_t frameIndex)
        {
                HandleTimerReset(frameIndex);
        }

        void Box2DPhysicsSystem::SetGravity(float gravity)
        {
                const float sanitized = std::isfinite(gravity) ? gravity : 9.8f;
                m_worldDef.gravity = b2Vec2{ 0.0f, -sanitized };
                if (b2World_IsValid(m_worldId))
                {
                        b2World_SetGravity(m_worldId, m_worldDef.gravity);
                        m_loggedMissingWorld = false;
                }
        }

        void Box2DPhysicsSystem::SetSubStepCount(int subStepCount)
        {
                const int clamped = std::clamp(subStepCount, 1, 24);
                m_subStepCount = clamped;
        }

        b2WorldId Box2DPhysicsSystem::GetWorldId() const
        {
                return m_worldId;
        }

        void Box2DPhysicsSystem::SetContactListener(std::shared_ptr<Box2DContactListener> listener)
        {
                m_contactListener = std::move(listener);

                if (m_contactListener)
                {
                        m_contactListener->SetWorld(m_worldId);
                }
        }

        PhysicsMachine* Box2DPhysicsSystem::GetPhysicsMachine() const
        {
                return m_physicsMachine;
        }

        void Box2DPhysicsSystem::DestroyWorld()
        {
                if (b2World_IsValid(m_worldId))
                {
                        b2DestroyWorld(m_worldId);
                        m_worldId = b2_nullWorldId;
                }

                if (m_contactListener)
                {
                        m_contactListener->SetWorld(b2_nullWorldId);
                }

                ResetSimulationState();
                m_hasTimerFrame = false;
                m_lastTimerFrame = 0;
        }
}
