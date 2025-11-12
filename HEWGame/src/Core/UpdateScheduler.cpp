#include "Core/UpdateScheduler.h"

#include <algorithm>
#include <unordered_map>

namespace Core
{
        namespace
        {
                struct SortHelper
                {
                        static bool Less(const IUpdatable* lhs, const IUpdatable* rhs)
                        {
                                if (lhs == rhs) { return false; }
                                if (lhs->GetPhase() != rhs->GetPhase())
                                {
                                        return static_cast<int>(lhs->GetPhase()) < static_cast<int>(rhs->GetPhase());
                                }
                                if (lhs->GetPriority() != rhs->GetPriority())
                                {
                                        return lhs->GetPriority() > rhs->GetPriority();
                                }
                                return lhs < rhs;
                        }
                };
        }

        bool UpdateScheduler::NodeLess(const IUpdatable* lhs, const IUpdatable* rhs)
        {
                return SortHelper::Less(lhs, rhs);
        }

        void UpdateScheduler::Register(IUpdatable& node)
        {
                // 重複登録を避けるために線形検索を行う。ノード数は大きくない前提なのでOK
                auto it = std::find(m_nodes.begin(), m_nodes.end(), &node);
                if (it == m_nodes.end())
                {
                        m_nodes.emplace_back(&node);
                }
        }

        void UpdateScheduler::Unregister(IUpdatable& node)
        {
                // ノードリストから削除し、同時に依存関係の矢印も破棄する
                auto it = std::remove(m_nodes.begin(), m_nodes.end(), &node);
                if (it != m_nodes.end())
                {
                        m_nodes.erase(it, m_nodes.end());
                }

                auto depIt = std::remove_if(m_dependencies.begin(), m_dependencies.end(),
                        [&node](const Dependency& dep)
                        {
                                return dep.first == &node || dep.second == &node;
                        });
                if (depIt != m_dependencies.end())
                {
                        m_dependencies.erase(depIt, m_dependencies.end());
                }
        }

        void UpdateScheduler::AddDependency(IUpdatable& before, IUpdatable& after)
        {
                if (&before == &after)
                {
                        // 自己依存は無意味なので無視
                        return;
                }

                Dependency candidate(&before, &after);
                auto it = std::find(m_dependencies.begin(), m_dependencies.end(), candidate);
                if (it == m_dependencies.end())
                {
                        m_dependencies.emplace_back(candidate);
                }
        }

        void UpdateScheduler::ClearDependencies()
        {
                m_dependencies.clear();
        }

        void UpdateScheduler::Tick(UpdateContext& ctx)
        {
                if (m_nodes.empty())
                {
                        return;
                }

                // 更新リストをコピーしてソート用に使用する
                std::vector<IUpdatable*> sortedNodes = m_nodes;
                sortedNodes.erase(std::remove(sortedNodes.begin(), sortedNodes.end(), nullptr), sortedNodes.end());
                std::sort(sortedNodes.begin(), sortedNodes.end(), NodeLess);

                const std::size_t nodeCount = sortedNodes.size();
                std::unordered_map<IUpdatable*, std::size_t> indexLookup;
                indexLookup.reserve(nodeCount);
                for (std::size_t i = 0; i < nodeCount; ++i)
                {
                        indexLookup.emplace(sortedNodes[i], i);
                }

                std::vector<std::vector<std::size_t>> edges(nodeCount);
                std::vector<std::size_t> indegree(nodeCount, 0);
                for (const auto& dep : m_dependencies)
                {
                        auto beforeIt = indexLookup.find(dep.first);
                        auto afterIt = indexLookup.find(dep.second);
                        if (beforeIt == indexLookup.end() || afterIt == indexLookup.end())
                        {
                                // 未登録ノードの依存は無視。登録順によっては起こり得る
                                continue;
                        }

                        edges[beforeIt->second].push_back(afterIt->second);
                        ++indegree[afterIt->second];
                }

                // indegree が0のノードは依存を持たないので即時に実行候補とする
                std::vector<std::size_t> ready;
                ready.reserve(nodeCount);
                for (std::size_t i = 0; i < nodeCount; ++i)
                {
                        if (indegree[i] == 0)
                        {
                                ready.push_back(i);
                        }
                }

                std::vector<std::size_t> processed;
                processed.reserve(nodeCount);

                // ready キューから「フェーズ・優先度が最も早いもの」を選び、安定した順序を維持する
                auto popBestReady = [&ready, &sortedNodes]() -> std::size_t
                {
                        auto bestIt = ready.begin();
                        for (auto it = ready.begin(); it != ready.end(); ++it)
                        {
                                if (UpdateScheduler::NodeLess(sortedNodes[*it], sortedNodes[*bestIt]))
                                {
                                        bestIt = it;
                                }
                        }
                        std::size_t value = *bestIt;
                        ready.erase(bestIt);
                        return value;
                };

                while (!ready.empty())
                {
                        std::size_t current = popBestReady();
                        processed.push_back(current);

                        IUpdatable* node = sortedNodes[current];
                        if (node && node->IsEnabled())
                        {
                                node->OnUpdate(ctx);
                        }

                        for (std::size_t next : edges[current])
                        {
                                if (--indegree[next] == 0)
                                {
                                        ready.push_back(next);
                                }
                        }
                }

                if (processed.size() == nodeCount)
                {
                        return;
                }

                // ここに到達するのは循環依存があったケース。強制的に残りを順番に実行する
                std::vector<bool> isProcessed(nodeCount, false);
                for (std::size_t index : processed)
                {
                        isProcessed[index] = true;
                }

                for (std::size_t i = 0; i < nodeCount; ++i)
                {
                        if (isProcessed[i])
                        {
                                continue;
                        }

                        IUpdatable* node = sortedNodes[i];
                        if (node && node->IsEnabled())
                        {
                                node->OnUpdate(ctx);
                        }
                }
        }
}

