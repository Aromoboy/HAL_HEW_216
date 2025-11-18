// ============================================================================
// 概要: UpdateNode の挙動や関連ロジックを実装し、src／Core に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: src／Core 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#include "Core/UpdateNode.h"

namespace Core
{
        UpdateNode::UpdateNode(UpdatePhase phase, int priority)
                : m_phase(phase)
                , m_priority(priority)
                , m_enabled(true) // 生成直後は自動的に有効化しておく
        {
        }

        void UpdateNode::SetPhase(UpdatePhase phase)
        {
                // 例えばメニュー中だけフェーズをUIに切り替えるなど、柔軟な制御が可能だ
                m_phase = phase;
        }

        void UpdateNode::SetPriority(int priority)
        {
                // 同じフェーズで複数のノードが競合する場合はpriorityで細かく順序調整する
                m_priority = priority;
        }

        void UpdateNode::Enable(bool value)
        {
                // 一時停止やカットシーンなどで処理を止めたいときに利用する
                m_enabled = value;
        }

        UpdatePhase UpdateNode::GetPhase() const
        {
                return m_phase;
        }

        int UpdateNode::GetPriority() const
        {
                return m_priority;
        }

        bool UpdateNode::IsEnabled() const
        {
                // Scheduler側ではこのフラグを確認してからOnUpdateを呼ぶ
                return m_enabled;
        }
}

