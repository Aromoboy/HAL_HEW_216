// 物理エンジンのステップを進める役目を持つ
// 物理機械が更新フェーズで呼び出す

#pragma once

#include "Systems/Physics/PhysicsMachine.h"

#include <memory>
#include <cstdint>

#include <box2d/box2d.h>

namespace Systems
{
        class Box2DContactListener;
        class PhysicsMachine;

        class Box2DPhysicsSystem : public IPhysicsSystem
        {
        public:
                explicit Box2DPhysicsSystem(PhysicsMachine& physicsMachine,
                    const PhysicsTuningParameters& tuning = {});
                ~Box2DPhysicsSystem() override;

                void Apply(float dt) override;
                void PrepareFrame(std::uint64_t frameIndex);
                void AdvanceClampSummary(float dt);

                void SetGravity(float gravity);
                void SetSubStepCount(int subStepCount);
                b2WorldId GetWorldId() const;
                void SetContactListener(std::shared_ptr<Box2DContactListener> listener);
                PhysicsMachine* GetPhysicsMachine() const;

        private:
                void DestroyWorld();
                void ResetSimulationState();
                void HandleClampState(bool clamped, float clampedInput, float maxStep, int stepsSimulated, float accumulatedBeforeClamp, float maxAccumulatedTime);
                void HandleTimerReset(std::uint64_t frameIndex);

                b2WorldDef m_worldDef{};
                b2WorldId m_worldId{ b2_nullWorldId };
                std::shared_ptr<Box2DContactListener> m_contactListener;
                PhysicsMachine* m_physicsMachine = nullptr;
                float m_maxStepDt = 1.0f / 60.0f;
                float m_stepAccumulator = 0.0f;
                int m_subStepCount = 4;
                int m_maxAccumulatedSteps = 8;
                bool m_loggedMissingWorld = false;
                bool m_clampActive = false;
                float m_clampLogElapsed = 0.0f;
                int m_currentClampFrames = 0;
                int m_currentClampSteps = 0;
                int m_summaryClampActivations = 0;
                int m_summaryClampFrames = 0;
                int m_summaryClampSteps = 0;
                float m_summaryElapsed = 0.0f;
                std::uint64_t m_lastTimerFrame = 0;
                bool m_hasTimerFrame = false;
        };
}
