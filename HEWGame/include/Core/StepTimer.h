#pragma once

#include <chrono>
#include <cstdint>

namespace Core
{
        class StepTimer
        {
        public:
                StepTimer()
                {
                        Reset();
                }

                void Reset()
                {
                        m_lastTick = Clock::now();
                        m_hasLastTick = false;
                        m_deltaSeconds = 0.0f;
                        m_frameIndex = 0;
                }

                float Tick()
                {
                        const auto now = Clock::now();
                        if (!m_hasLastTick)
                        {
                                m_deltaSeconds = 0.0f;
                                m_hasLastTick = true;
                        }
                        else
                        {
                                m_deltaSeconds = std::chrono::duration<float>(now - m_lastTick).count();
                        }

                        m_lastTick = now;
                        ++m_frameIndex;
                        return m_deltaSeconds;
                }

                float DeltaSeconds() const
                {
                        return m_deltaSeconds;
                }

                std::uint64_t FrameIndex() const
                {
                        return m_frameIndex;
                }

        private:
                using Clock = std::chrono::steady_clock;

                Clock::time_point m_lastTick;
                bool m_hasLastTick = false;
                float m_deltaSeconds = 0.0f;
                std::uint64_t m_frameIndex = 0;
        };
}

