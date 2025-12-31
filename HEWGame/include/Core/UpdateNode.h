#pragma once

#include "Core/IUpdatable.h"
#include "Core/RegistrationHandle.h"

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
                void SetRegistrationHandle(RegistrationHandle handle);
                void ReleaseRegistrationHandle();

                // スケジューラが参照する基本情報 派生先で上書きする必要はない
                UpdatePhase GetPhase() const override;
                int GetPriority() const override;
                bool IsEnabled() const override;

        private:
                UpdatePhase m_phase; // 現在の更新フェーズ
                int m_priority;      // 同フェーズ内での優先度（数値が小さいほど先に実行）
                bool m_enabled;      // Updateを呼ぶかどうかのフラグ
                RegistrationHandle m_registrationHandle;
        };
}
