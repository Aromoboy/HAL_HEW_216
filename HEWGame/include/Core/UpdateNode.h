#pragma once

#include "Core/IUpdatable.h"

namespace Core
{
        //
        // UpdateNode は IUpdatable の基本実装です。
        // フェーズ・優先度・有効フラグという三つの軸をメンバーとして保持し、
        // 継承先は OnUpdate だけを実装すればよいシンプルな形にしています。
        //
        class UpdateNode : public IUpdatable
        {
        public:
                // phase と priority はデフォルト値を用意しておくことで、
                // 「とりあえず動かしたい」段階でも簡単に派生クラスを作れます。
                explicit UpdateNode(UpdatePhase phase = UpdatePhase::Gameplay, int priority = 0);

                // ランタイムで実行順を変更したい場合に備えてセッターを公開しています。
                void SetPhase(UpdatePhase phase);
                void SetPriority(int priority);
                void Enable(bool value);

                // スケジューラが参照する基本情報。派生先で上書きする必要はありません。
                UpdatePhase GetPhase() const override;
                int GetPriority() const override;
                bool IsEnabled() const override;

        private:
                UpdatePhase m_phase; // 現在の更新フェーズ
                int m_priority;      // 同フェーズ内での優先度（数値が大きいほど先に実行）
                bool m_enabled;      // Update を呼ぶかどうかのフラグ
        };
}

