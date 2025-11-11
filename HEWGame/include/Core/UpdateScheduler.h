#pragma once

#include <utility>
#include <vector>

#include "Core/IUpdatable.h"

namespace Core
{
        //
        // UpdateScheduler は Scene 内の更新オブジェクトを一括管理するクラスです。
        // Register/Unregister で対象を登録し、Tick(ctx) を呼び出すとフェーズと
        // 優先度、さらに指定された依存関係を考慮して OnUpdate を実行します。
        //
        class UpdateScheduler
        {
        public:
                // ノードを登録。重複登録は弾くので同じインスタンスを渡しても安全です。
                void Register(IUpdatable& node);

                // ノードを登録解除。依存関係に含まれている場合もまとめて削除します。
                void Unregister(IUpdatable& node);

                // before → after の順で実行させたい場合に依存関係を追加します。
                void AddDependency(IUpdatable& before, IUpdatable& after);

                // 依存関係だけを一括でリセット。ノードの登録状況は維持されます。
                void ClearDependencies();

                // 毎フレーム呼び出されるメイン処理。内部でトポロジカルソートを行い、
                // 循環依存があってもできるだけ安全側で実行順を確定させます。
                void Tick(const UpdateContext& ctx);

        private:
                using Dependency = std::pair<IUpdatable*, IUpdatable*>;

                // sort や準備キューの比較で利用する共通関数。
                static bool NodeLess(const IUpdatable* lhs, const IUpdatable* rhs);

                std::vector<IUpdatable*> m_nodes;          // 登録済みノード
                std::vector<Dependency> m_dependencies;    // before -> after の矢印リスト
        };
}

