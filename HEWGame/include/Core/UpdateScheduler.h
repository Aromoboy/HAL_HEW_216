#pragma once

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

                // before → afterの順で実行させたい場合に依存関係を追加する
                void AddDependency(IUpdatable& before, IUpdatable& after);

                // 依存関係だけを一括でリセット。ノードの登録状況は維持される
                void ClearDependencies();

                // 毎フレーム呼び出されるメイン処理。内部でトポロジカルソートを行い、
                // 循環依存があってもできるだけ安全側で実行順を確定させる
                void Tick(const UpdateContext& ctx);

        private:
                using Dependency = std::pair<IUpdatable*, IUpdatable*>;

                // sortや準備キューの比較で利用する共通関数
                static bool NodeLess(const IUpdatable* lhs, const IUpdatable* rhs);

                std::vector<IUpdatable*> m_nodes;          // 登録済みノード
                std::vector<Dependency> m_dependencies;    // before -> afterの矢印リスト
        };
}

