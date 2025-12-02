#pragma once

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Core/IUpdatable.h"

namespace Core
{
        //
        // UpdateSchedulerはScene内の更新オブジェクトを一括管理するクラスである
        // RegisterまたはUnregisterで対象を登録し、Tick(ctx)を呼び出すとフェーズと
        // 優先度、さらに指定された依存関係を考慮してOnUpdateを実行する
        //
        class UpdateScheduler
        {
        public:
                // ノードを登録。重複登録は弾くので同じインスタンスを渡しても安全だ
                void Register(IUpdatable& node);

                // ノードを登録解除。依存関係に含まれている場合もまとめて削除する
                void Unregister(IUpdatable& node);

                // 実行順を動的に変更する
                void SetOrder(IUpdatable& node, UpdatePhase phase, int priority);

                // before → afterの順で実行させたい場合に依存関係を追加する
                void AddDependency(IUpdatable& before, IUpdatable& after);

                // 依存関係だけを一括でリセット。ノードの登録状況は維持される
                void ClearDependencies();

                // 毎フレーム呼び出されるメイン処理。内部でトポロジカルソートを行い、
                // 循環依存があってもできるだけ安全側で実行順を確定させる
                void Tick(UpdateContext& ctx);

                struct ExecutionRecord
                {
                        const IUpdatable* node = nullptr;
                        UpdatePhase phase = UpdatePhase::Input;
                        int priority = 0;
                        std::size_t registrationOrder = 0;
                        std::size_t sortedIndex = 0;
                        std::size_t executionIndex = 0;
                        bool invoked = false;
                };
                using ExecutionLog = std::vector<ExecutionRecord>;
                using ExecutionLogListener = std::function<void(const ExecutionLog&)>;

                void SetExecutionLogListener(ExecutionLogListener listener);

        private:
                using Dependency = std::pair<IUpdatable*, IUpdatable*>;

                struct PendingOrderChange
                {
                        IUpdatable* node = nullptr;
                        UpdatePhase phase = UpdatePhase::Input;
                        int priority = 0;
                };

                struct NodeOrderOverride
                {
                        UpdatePhase phase = UpdatePhase::Input;
                        int priority = 0;
                };

                // sortや準備キューの比較で利用する共通関数
                bool NodeLess(const IUpdatable* lhs, const IUpdatable* rhs) const;
                std::size_t GetRegistrationOrder(const IUpdatable* node) const;
                NodeOrderOverride GetEffectiveOrder(const IUpdatable* node) const;
                const NodeOrderOverride* FindOrderOverride(const IUpdatable* node) const;

                void RegisterInternal(IUpdatable& node);
                void UnregisterInternal(IUpdatable& node);
                void SetOrderInternal(IUpdatable& node, UpdatePhase phase, int priority);
                void ApplyPendingOperations();
                void ApplyPendingRegistration(IUpdatable* node);
                void ApplyPendingUnregistration(IUpdatable* node);
                void ApplyPendingOrderChange(const PendingOrderChange& change);
                bool IsRegistered(const IUpdatable* node) const;
                bool IsPendingRegistration(const IUpdatable* node) const;
                bool IsPendingUnregistration(const IUpdatable* node) const;
                PendingOrderChange* FindPendingOrderChange(const IUpdatable* node);

                std::vector<IUpdatable*> m_nodes;          // 登録済みノード
                std::vector<Dependency> m_dependencies;    // before -> afterの矢印リスト
                std::unordered_map<const IUpdatable*, std::size_t> m_registrationOrder;
                std::size_t m_nextRegistrationOrder = 0;
                ExecutionLogListener m_executionLogListener;

                std::vector<IUpdatable*> m_pendingRegistrations;     // Tick中に要求された登録
                std::vector<IUpdatable*> m_pendingUnregistrations;   // Tick中に要求された登録解除
                std::vector<PendingOrderChange> m_pendingOrderChanges; // Tick中に要求された順序変更
                bool m_isTicking = false; // Tick再入を検知するフラグ

                std::unordered_map<const IUpdatable*, NodeOrderOverride> m_orderOverrides; // UpdateNode以外の順序設定
        };
}

