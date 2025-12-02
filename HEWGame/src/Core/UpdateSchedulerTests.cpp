#include "Core/UpdateScheduler.h"
#include "Core/UpdateNode.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"

#include <cassert>

namespace
{
        class PlainUpdatable final : public Core::IUpdatable
        {
        public:
                PlainUpdatable(Core::UpdatePhase phase, int priority)
                        : m_phase(phase)
                        , m_priority(priority)
                {
                }

                void OnUpdate(Core::UpdateContext&) override
                {
                        ++m_updateCount;
                }

                Core::UpdatePhase GetPhase() const override
                {
                        return m_phase;
                }

                int GetPriority() const override
                {
                        return m_priority;
                }

                bool IsEnabled() const override
                {
                        return true;
                }

                int UpdateCount() const
                {
                        return m_updateCount;
                }

        private:
                Core::UpdatePhase m_phase;
                int m_priority;
                int m_updateCount = 0;
        };

        class CountingUpdateNode final : public Core::UpdateNode
        {
        public:
                CountingUpdateNode(Core::UpdatePhase phase, int priority)
                        : Core::UpdateNode(phase, priority)
                {
                }

                void OnUpdate(Core::UpdateContext&) override
                {
                        ++m_updateCount;
                }

                int UpdateCount() const
                {
                        return m_updateCount;
                }

        private:
                int m_updateCount = 0;
        };

        class SelfReRegisteringNode final : public Core::UpdateNode
        {
        public:
                explicit SelfReRegisteringNode(Core::UpdateScheduler& scheduler)
                        : Core::UpdateNode(Core::UpdatePhase::Gameplay, 0)
                        , m_scheduler(scheduler)
                {
                }

                void OnUpdate(Core::UpdateContext&) override
                {
                        ++m_updateCount;
                        if (!m_hasAttemptedReregister)
                        {
                                m_hasAttemptedReregister = true;
                                m_scheduler.Unregister(*this);
                                m_scheduler.Register(*this);
                        }
                }

                int UpdateCount() const
                {
                        return m_updateCount;
                }

        private:
                Core::UpdateScheduler& m_scheduler;
                bool m_hasAttemptedReregister = false;
                int m_updateCount = 0;
        };

        void RunPlainIUpdatableOrderOverrideTest()
        {
                Core::UpdateScheduler scheduler;

                PlainUpdatable plainNode(Core::UpdatePhase::Gameplay, 5);
                CountingUpdateNode derivedNode(Core::UpdatePhase::Gameplay, 0);

                scheduler.Register(derivedNode);
                scheduler.Register(plainNode);

                scheduler.SetOrder(plainNode, Core::UpdatePhase::Input, -10);
                scheduler.SetOrder(derivedNode, Core::UpdatePhase::Gameplay, 0);

                Core::UpdateScheduler::ExecutionLog capturedLog;
                scheduler.SetExecutionLogListener(
                        [&capturedLog](const Core::UpdateScheduler::ExecutionLog& log)
                        {
                                capturedLog = log;
                        });

                Core::StepTimer timer;
                Core::Input dummyInput;
                Core::UpdateContext ctx(0.0f, timer, dummyInput, nullptr);

                scheduler.Tick(ctx);

#if defined(_DEBUG)
                assert(capturedLog.size() == 2);
                assert(capturedLog.front().node == &plainNode);
                assert(capturedLog.front().phase == Core::UpdatePhase::Input);
                assert(capturedLog.front().priority == -10);
                assert(plainNode.UpdateCount() == 1);
                assert(derivedNode.UpdateCount() == 1);
#endif
        }

        void RunDependencyOrderingTest()
        {
                Core::UpdateScheduler scheduler;

                CountingUpdateNode first(Core::UpdatePhase::Gameplay, 0);
                CountingUpdateNode second(Core::UpdatePhase::Gameplay, 0);
                CountingUpdateNode third(Core::UpdatePhase::Gameplay, 0);

                scheduler.Register(first);
                scheduler.Register(second);
                scheduler.Register(third);

                scheduler.AddDependency(first, second);
                scheduler.AddDependency(second, third);

                Core::UpdateScheduler::ExecutionLog capturedLog;
                scheduler.SetExecutionLogListener(
                        [&capturedLog](const Core::UpdateScheduler::ExecutionLog& log)
                        {
                                capturedLog = log;
                        });

                Core::StepTimer timer;
                Core::Input dummyInput;
                Core::UpdateContext ctx(0.0f, timer, dummyInput, nullptr);

                scheduler.Tick(ctx);

#if defined(_DEBUG)
                assert(capturedLog.size() == 3);
                assert(capturedLog[0].node == &first);
                assert(capturedLog[1].node == &second);
                assert(capturedLog[2].node == &third);
#endif
        }

        void RunReadyQueueOrderingStabilityTest()
        {
                Core::UpdateScheduler scheduler;

                CountingUpdateNode late(Core::UpdatePhase::Late, 0);
                CountingUpdateNode gameplayHighPriority(Core::UpdatePhase::Gameplay, -5);
                CountingUpdateNode gameplayLowPriority(Core::UpdatePhase::Gameplay, 5);
                CountingUpdateNode camera(Core::UpdatePhase::Camera, 0);

                scheduler.Register(gameplayLowPriority);
                scheduler.Register(late);
                scheduler.Register(camera);
                scheduler.Register(gameplayHighPriority);

                Core::UpdateScheduler::ExecutionLog capturedLog;
                scheduler.SetExecutionLogListener(
                        [&capturedLog](const Core::UpdateScheduler::ExecutionLog& log)
                        {
                                capturedLog = log;
                        });

                Core::StepTimer timer;
                Core::Input dummyInput;
                Core::UpdateContext ctx(0.0f, timer, dummyInput, nullptr);

                scheduler.Tick(ctx);

#if defined(_DEBUG)
                assert(capturedLog.size() == 4);
                assert(capturedLog[0].node == &gameplayHighPriority);
                assert(capturedLog[1].node == &gameplayLowPriority);
                assert(capturedLog[2].node == &camera);
                assert(capturedLog[3].node == &late);
#endif
        }

        void RunPendingUnregistrationCancellationTest()
        {
                Core::UpdateScheduler scheduler;

                SelfReRegisteringNode node(scheduler);
                scheduler.Register(node);

                Core::StepTimer timer;
                Core::Input dummyInput;
                Core::UpdateContext ctx(0.0f, timer, dummyInput, nullptr);

                scheduler.Tick(ctx);
                scheduler.Tick(ctx);

#if defined(_DEBUG)
                assert(node.UpdateCount() == 2);
#endif
        }

        void RunStepTimerFixedStepTest()
        {
                Core::StepTimer timer;

                constexpr float kFixedDelta = 0.016f;
                timer.SetFixedStep(kFixedDelta, true);

                const float initialDelta = timer.Tick();
                const float fixedDelta = timer.Tick();

#if defined(_DEBUG)
                assert(initialDelta == 0.0f);
                assert(fixedDelta == kFixedDelta);
#endif

                timer.SetFixedStep(0.0f, false);

                const float overrideDelta = timer.Tick(0.033f);

#if defined(_DEBUG)
                assert(overrideDelta == 0.033f);
#endif
        }

        struct UpdateSchedulerTestRunner
        {
                UpdateSchedulerTestRunner()
                {
                        RunPlainIUpdatableOrderOverrideTest();
                        RunDependencyOrderingTest();
                        RunReadyQueueOrderingStabilityTest();
                        RunPendingUnregistrationCancellationTest();
                        RunStepTimerFixedStepTest();
                }
        };

        static UpdateSchedulerTestRunner g_updateSchedulerTestRunner;
}

