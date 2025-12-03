//ドロスミハイル
//このファイルにて変更が必要な場合、まず相談しておいてください
#pragma once

#include <algorithm>
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
            m_deltaTime = 0.0f;
            m_fixedStep = 0.0f;
            m_useFixedStep = false;
            m_frameIndex = 0;
        }

        float Tick()
        {
            const auto now = Clock::now();
            if (!m_hasLastTick)
            {
                m_deltaTime = 0.0f;
                m_hasLastTick = true;
            }
            else
            {
                if (m_useFixedStep)
                {
                    m_deltaTime = m_fixedStep;
                }
                else
                {
                    m_deltaTime = std::chrono::duration<float>(now - m_lastTick).count();
                }
            }

            m_lastTick = now;
            ++m_frameIndex;
            return m_deltaTime;
        }

        float Tick(float overrideDeltaTime)
        {
            m_deltaTime = std::max(0.0f, overrideDeltaTime);
            m_lastTick = Clock::now();
            m_hasLastTick = true;
            ++m_frameIndex;
            return m_deltaTime;
        }

        float DeltaTime() const
        {
            return m_deltaTime;
        }

        void SetFixedStep(float dt, bool enable)
        {
            m_fixedStep = std::max(0.0f, dt);
            m_useFixedStep = enable && m_fixedStep > 0.0f;
        }

        std::uint64_t FrameIndex() const
        {
            return m_frameIndex;
        }

    private:
        using Clock = std::chrono::steady_clock;

        Clock::time_point m_lastTick;
        bool m_hasLastTick = false;
        float m_deltaTime = 0.0f;
        float m_fixedStep = 0.0f;
        bool m_useFixedStep = false;
        std::uint64_t m_frameIndex = 0;
    };
};

