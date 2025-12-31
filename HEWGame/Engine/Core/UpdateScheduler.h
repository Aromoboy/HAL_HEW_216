#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Core/IUpdatable.h"
#include "Core/RegistrationHandle.h"

namespace Core
{
        //
        // UpdateSchedulerはScene内の更新オブジェクトを一括管理するクラスである
        // 登録/解除で対象を管理し、更新(ctx)を呼び出すとフェーズと
        // 優先度、さらに指定された依存関係を考慮して更新処理を実行する
        //
        class UpdateScheduler
        {
        public:
                using NodeId = std::uint64_t;

                class ExecutionLogListener
                {
                public:
                        virtual ~ExecutionLogListener() = default;
                        virtual void OnFrameBegin(std::uint64_t frameIndex) = 0;
                        virtual void OnNodeExecuted(std::uint64_t frameIndex, UpdatePhase phase, const IUpdatable* node, int priority) = 0;
                        virtual void OnFrameEnd(std::uint64_t frameIndex) = 0;
                };

                // ノードを登録 重複登録は弾くので同じインスタンスを渡しても安全だ
                RegistrationHandle Register(IUpdatable& node);

                // ノードを登録解除 依存関係に含まれている場合もまとめて削除する
                void Unregister(IUpdatable& node);

                // 実行順を動的に変更する
                void SetOrder(IUpdatable& node, UpdatePhase phase, int priority);

                // 前後関係の順で実行させたい場合に依存関係を追加する
                void AddDependency(IUpdatable& before, IUpdatable& after);

                // 依存関係だけを一括でリセット ノードの登録状況は維持される
                void ClearDependencies();

                // 毎フレーム呼び出されるメイン処理 内部でトポロジカルソートを行い、
                // 循環依存があってもできるだけ安全側で実行順を確定させる
                void Tick(const UpdateContext& ctx);

                void SetExecutionLogListener(ExecutionLogListener* listener);

        private:
                using Dependency = std::pair<IUpdatable*, IUpdatable*>;

                struct PendingOrderChange
                {
                        IUpdatable* node = nullptr;
                        UpdatePhase phase = UpdatePhase::Input;
                        int priority = 0;
                };

                struct PendingRegistration
                {
                        IUpdatable* node = nullptr;
                        NodeId nodeId = 0;
                        std::size_t registrationOrder = 0;
                };

                // ソートや準備キューの比較で利用する共通関数
                bool NodeLess(const IUpdatable* lhs, const IUpdatable* rhs) const;
                std::size_t GetRegistrationOrder(const IUpdatable* node) const;

                void RegisterInternal(IUpdatable& node);
                void UnregisterInternal(IUpdatable& node);
                void SetOrderInternal(IUpdatable& node, UpdatePhase phase, int priority);
                void AddDependencyInternal(IUpdatable& before, IUpdatable& after);
                void RebuildExecutionPlan();
                void ApplyPendingOperations();
                void ApplyPendingRegistration(const PendingRegistration& registration);
                void ApplyPendingUnregistration(IUpdatable* node);
                void ApplyPendingOrderChange(const PendingOrderChange& change);
                void ApplyPendingDependency(const Dependency& dep);
                bool IsRegistered(const IUpdatable* node) const;
                bool IsPendingRegistration(const IUpdatable* node) const;
                bool IsPendingUnregistration(const IUpdatable* node) const;
                PendingOrderChange* FindPendingOrderChange(const IUpdatable* node);
                PendingRegistration* FindPendingRegistration(const IUpdatable* node);

                void UnregisterById(NodeId nodeId);

                std::vector<IUpdatable*> m_nodes;          // 登録済みノード
                std::vector<Dependency> m_dependencies;    // 前後関係の矢印リスト
                std::unordered_map<const IUpdatable*, std::size_t> m_registrationOrder;
                std::unordered_map<const IUpdatable*, NodeId> m_nodeIds;
                std::unordered_map<NodeId, IUpdatable*> m_nodesById;
                std::size_t m_nextRegistrationOrder = 0;
                NodeId m_nextNodeId = 1;
                ExecutionLogListener* m_executionLogListener = nullptr;
                std::vector<IUpdatable*> m_executionPlan;
                bool m_executionPlanDirty = true;

                std::vector<PendingRegistration> m_pendingRegistrations;     // 更新中に要求された登録
                std::vector<IUpdatable*> m_pendingUnregistrations;   // 更新中に要求された登録解除
                std::vector<PendingOrderChange> m_pendingOrderChanges; // 更新中に要求された順序変更
                std::vector<Dependency> m_pendingDependencies;
                bool m_pendingClearDependencies = false;
                bool m_isTicking = false; // 更新再入を検知するフラグ

                friend class RegistrationHandle;
        };
}
