#include "Core/StepTimer.h"

namespace Core
{
        StepTimer::StepTimer()
                : m_lastTick(clock::now())
                , m_hasLastTick(false)
                , m_deltaTime(0.0f)
                , m_totalTime(0.0)
                , m_frameIndex(0)
                , m_fixedStepInterval(0.0f)
                , m_useFixedStep(false)
        {
        }

        void StepTimer::Reset()
        {
                m_lastTick = clock::now();
                m_hasLastTick = false;
                m_deltaTime = 0.0f;
                m_totalTime = 0.0;
                m_frameIndex = 0;
        }

        void StepTimer::Tick()
        {
                auto now = clock::now();
                float dt = 0.0f;

                if (m_hasLastTick)
                {
                        dt = std::chrono::duration<float>(now - m_lastTick).count();
                        if (m_useFixedStep && m_fixedStepInterval > 0.0f)
                        {
                                dt = m_fixedStepInterval;
                        }
                }
                else
                {
                        m_hasLastTick = true;
                }

                m_lastTick = now;

                m_deltaTime = dt;
                m_totalTime += static_cast<double>(dt);
                ++m_frameIndex;
        }

        void StepTimer::SetFixedStep(float dtSeconds)
        {
                if (dtSeconds > 0.0f)
                {
                        m_useFixedStep = true;
                        m_fixedStepInterval = dtSeconds;
                }
                else
                {
                        m_useFixedStep = false;
                        m_fixedStepInterval = 0.0f;
                }
        }

        float StepTimer::DeltaTime() const
        {
                return m_deltaTime;
        }

        double StepTimer::TotalTime() const
        {
                return m_totalTime;
        }

        std::uint64_t StepTimer::FrameIndex() const
        {
                return m_frameIndex;
        }

        bool StepTimer::IsFixedStep() const
        {
                return m_useFixedStep;
        }

        float StepTimer::FixedStepInterval() const
        {
                return m_fixedStepInterval;
        }
}

