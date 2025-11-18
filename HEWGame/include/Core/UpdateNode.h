// ============================================================================
// 概要: UpdateNode に関するインターフェースやデータ構造を宣言し、include／Core から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include／Core 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once

#include "Core/IUpdatable.h"

namespace Core
{
        //
        // UpdateNodeはIUpdatableの基本実装
        // フェーズ・優先度・有効フラグという三つの軸をメンバーとして保持し、
        // 継承先はOnUpdateだけを実装すればよいシンプルな形にしている
        //
        class UpdateNode : public IUpdatable
        {
        public:
                // phaseとpriorityはデフォルト値を用意しておくことで、
                //「とりあえず動かしたい」段階でも簡単に派生クラスを作れる
                explicit UpdateNode(UpdatePhase phase = UpdatePhase::Gameplay, int priority = 0);

                // ランタイムで実行順を変更したい場合に備えてセッターを公開している
                void SetPhase(UpdatePhase phase);
                void SetPriority(int priority);
                void Enable(bool value);

                // スケジューラが参照する基本情報。派生先で上書きする必要はない
                UpdatePhase GetPhase() const override;
                int GetPriority() const override;
                bool IsEnabled() const override;

        private:
                UpdatePhase m_phase; // 現在の更新フェーズ
                int m_priority;      // 同フェーズ内での優先度（数値が小さいほど先に実行）
                bool m_enabled;      // Updateを呼ぶかどうかのフラグ
        };
}

