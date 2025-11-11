#pragma once

#include <chrono>
#include <cstdint>

namespace Core
{
        class StepTimer
        {
        public:
                using clock = std::chrono::steady_clock;

                StepTimer();

                void Reset();
                void Tick();
                void SetFixedStep(float dtSeconds);

                float DeltaTime() const;
                double TotalTime() const;
                std::uint64_t FrameIndex() const;

                bool IsFixedStep() const;
                float FixedStepInterval() const;

        private:
                clock::time_point m_lastTick;
                bool m_hasLastTick;

                float m_deltaTime;
                double m_totalTime;
                std::uint64_t m_frameIndex;

                float m_fixedStepInterval;
                bool m_useFixedStep;
        };
}

