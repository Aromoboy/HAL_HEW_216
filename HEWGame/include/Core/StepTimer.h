// ============================================================================
// 概要: StepTimer に関するインターフェースやデータ構造を宣言し、include／Core から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include／Core 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

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

                float Tick(float overrideDeltaSeconds)
                {
                        m_deltaSeconds = std::max(0.0f, overrideDeltaSeconds);
                        m_lastTick = Clock::now();
                        m_hasLastTick = true;
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

