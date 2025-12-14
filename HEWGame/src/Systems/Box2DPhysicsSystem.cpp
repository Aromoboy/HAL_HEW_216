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
        Box2DPhysicsSystem::Box2DPhysicsSystem(const PhysicsTuningParameters& tuning)
                : Core::UpdateNode(Core::UpdatePhase::Physics, 0)
                , m_worldDef(b2DefaultWorldDef())
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

                if (stepsSimulated > 1 || clampedInput > maxStep || accumulatedBeforeClamp > maxAccumulatedTime)
                {
                        std::ostringstream oss;
                        oss << "[Box2DPhysicsSystem] Clamped dt from " << clampedInput
                                << " to " << maxStep
                                << " over " << stepsSimulated << " steps"
                                << " (accumulator=" << m_stepAccumulator
                                << "/" << maxAccumulatedTime << ")\n";
                        DebugTrace(oss.str());
                }

                if (m_contactListener)
                {
                        m_contactListener->ProcessContacts();
                }
        }

        void Box2DPhysicsSystem::OnUpdate(Core::UpdateContext& ctx)
        {
                Apply(ctx.dt);

                if (m_physicsMachine)
                {
                        m_physicsMachine->SyncBodies(ctx.dt);
                }
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

        void Box2DPhysicsSystem::SetPhysicsMachine(PhysicsMachine* physicsMachine)
        {
                m_physicsMachine = physicsMachine;
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
        }
}

