#include "Core/UpdateScheduler.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <sstream>
#include <string>
#include <typeinfo>
#include <unordered_map>

#if !defined(UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS)
#if defined(_DEBUG)
#define UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS 1
#else
#define UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS 0
#endif
#endif

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

#include "Core/UpdateNode.h"



namespace Core
{
        namespace
        {
                void DebugLog(const std::string& message)
                {
#if UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS && defined(_WIN32)
                        ::OutputDebugStringA(message.c_str());
#else
                        (void)message;
#endif
                }

                std::string DescribeNode(const Core::IUpdatable* node, Core::UpdatePhase phase, int priority)
                {
                        std::ostringstream oss;
                        if (!node)
                        {
                                oss << "<null>";
                                return oss.str();
                        }

                        oss << "node=" << node
                                << " phase=" << Core::ToString(phase)
                                << '(' << static_cast<int>(phase) << ')'
                                << " priority=" << priority
                                << " enabled=" << node->IsEnabled();

#if defined(_CPPRTTI)
                        oss << " type=" << typeid(*node).name();
#endif

                        return oss.str();
                }
        }

        bool UpdateScheduler::NodeLess(const IUpdatable* lhs, const IUpdatable* rhs) const
        {
                if (lhs == rhs)
                {
                        return false;
                }

                if (!lhs)
                {
                        return rhs != nullptr;
                }
                if (!rhs)
                {
                        return false;
                }

                const NodeOrderOverride lhsOrder = GetEffectiveOrder(lhs);
                const NodeOrderOverride rhsOrder = GetEffectiveOrder(rhs);

                if (lhsOrder.phase != rhsOrder.phase)
                {
                        return static_cast<int>(lhsOrder.phase) < static_cast<int>(rhsOrder.phase);
                }
                if (lhsOrder.priority != rhsOrder.priority)
                {
                        return lhsOrder.priority < rhsOrder.priority;
                }

                const std::size_t lhsRegistrationOrder = GetRegistrationOrder(lhs);
                const std::size_t rhsRegistrationOrder = GetRegistrationOrder(rhs);
                if (lhsRegistrationOrder != rhsRegistrationOrder)
                {
                        return lhsRegistrationOrder < rhsRegistrationOrder;
                }

                return false;
        }

        std::size_t UpdateScheduler::GetRegistrationOrder(const IUpdatable* node) const
        {
                if (!node)
                {
                        return std::numeric_limits<std::size_t>::max();
                }

                auto it = m_registrationOrder.find(node);
                if (it != m_registrationOrder.end())
                {
                        return it->second;
                }

#if UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS && defined(_WIN32)
                std::ostringstream oss;
                oss << "[UpdateScheduler] registration order missing for node=" << node << "\n";
                ::OutputDebugStringA(oss.str().c_str());
#endif
                return std::numeric_limits<std::size_t>::max();
        }

        const UpdateScheduler::NodeOrderOverride* UpdateScheduler::FindOrderOverride(const IUpdatable* node) const
        {
                if (!node)
                {
                        return nullptr;
                }

                auto it = m_orderOverrides.find(node);
                if (it == m_orderOverrides.end())
                {
                        return nullptr;
                }

                return &it->second;
        }

        UpdateScheduler::NodeOrderOverride UpdateScheduler::GetEffectiveOrder(const IUpdatable* node) const
        {
                NodeOrderOverride result;
                if (!node)
                {
                        return result;
                }

                if (const NodeOrderOverride* overrideOrder = FindOrderOverride(node))
                {
                        return *overrideOrder;
                }

                result.phase = node->GetPhase();
                result.priority = node->GetPriority();
                return result;
        }

        void UpdateScheduler::SetExecutionLogListener(ExecutionLogListener listener)
        {
                m_executionLogListener = std::move(listener);
        }

        UpdateScheduler::NodeHandleList::iterator UpdateScheduler::FindNodeHandle(IUpdatable* node)
        {
                return std::find_if(m_nodes.begin(), m_nodes.end(),
                        [node](const NodeHandle& handle)
                        {
                                return handle.Get() == node;
                        });
        }

        UpdateScheduler::NodeHandleList::const_iterator UpdateScheduler::FindNodeHandle(const IUpdatable* node) const
        {
                return std::find_if(m_nodes.begin(), m_nodes.end(),
                        [node](const NodeHandle& handle)
                        {
                                return handle.Get() == node;
                        });
        }

        bool UpdateScheduler::IsRegistered(const IUpdatable* node) const
        {
                if (!node)
                {
                        return false;
                }

                auto it = FindNodeHandle(node);
                return it != m_nodes.end();
        }

        bool UpdateScheduler::IsPendingRegistration(const IUpdatable* node) const
        {
                if (!node)
                {
                        return false;
                }

                return std::find(m_pendingRegistrations.begin(), m_pendingRegistrations.end(), node) != m_pendingRegistrations.end();
        }

        bool UpdateScheduler::IsPendingUnregistration(const IUpdatable* node) const
        {
                if (!node)
                {
                        return false;
                }

                return std::find(m_pendingUnregistrations.begin(), m_pendingUnregistrations.end(), node) != m_pendingUnregistrations.end();
        }

        UpdateScheduler::PendingOrderChange* UpdateScheduler::FindPendingOrderChange(const IUpdatable* node)
        {
                if (!node)
                {
                        return nullptr;
                }

                auto it = std::find_if(m_pendingOrderChanges.begin(), m_pendingOrderChanges.end(),
                        [node](const PendingOrderChange& change)
                        {
                                return change.node == node;
                        });

                if (it == m_pendingOrderChanges.end())
                {
                        return nullptr;
                }

                return &(*it);
        }

        void UpdateScheduler::RegisterInternal(IUpdatable& node)
        {
                if (IsRegistered(&node))
                {
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] RegisterInternal detected duplicate node=" << &node << '\n';
                        DebugLog(oss.str());
                        return;
                }

                auto sharedHandle = std::shared_ptr<IUpdatable>(std::addressof(node));
                m_nodes.push_back({ sharedHandle.get(), sharedHandle, sharedHandle });
                m_registrationOrder.emplace(&node, m_nextRegistrationOrder++);

#if UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS
                const NodeOrderOverride order = GetEffectiveOrder(&node);
                std::ostringstream oss;
                oss << "[UpdateScheduler] Registered "
                    << DescribeNode(&node, order.phase, order.priority)
                    << " order=" << (m_nextRegistrationOrder - 1)
                    << '\n';
                DebugLog(oss.str());
#endif
        }

        void UpdateScheduler::UnregisterInternal(IUpdatable& node)
        {
                auto it = FindNodeHandle(&node);
                if (it == m_nodes.end())
                {
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] UnregisterInternal called for unknown node=" << &node << '\n';
                        DebugLog(oss.str());
                }
                else
                {
                        it->Expire();
                        m_nodes.erase(it);
                }

                m_registrationOrder.erase(&node);

                auto depIt = std::remove_if(m_dependencies.begin(), m_dependencies.end(),
                        [&node](const Dependency& dep)
                        {
                                return dep.first == &node || dep.second == &node;
                        });
                if (depIt != m_dependencies.end())
                {
                        m_dependencies.erase(depIt, m_dependencies.end());
                }

                if (m_orderOverrides.erase(&node) > 0)
                {
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] Removed order override for unregistered node=" << &node << '\n';
                        DebugLog(oss.str());
                }

#if UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS
                std::ostringstream oss;
                const NodeOrderOverride order = GetEffectiveOrder(&node);
                oss << "[UpdateScheduler] Unregistered "
                    << DescribeNode(&node, order.phase, order.priority)
                    << '\n';
                DebugLog(oss.str());
#endif
        }

        void UpdateScheduler::SetOrderInternal(IUpdatable& node, UpdatePhase phase, int priority)
        {
                auto* updateNode = dynamic_cast<UpdateNode*>(&node);
                if (!updateNode)
                {
                        NodeOrderOverride& entry = m_orderOverrides[&node];
                        entry.phase = phase;
                        entry.priority = priority;

#if UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] Applied order override to non-UpdateNode node=" << &node
                            << " requestedPhase=" << static_cast<int>(phase)
                            << " requestedPriority=" << priority
                            << " currentPhase=" << static_cast<int>(node.GetPhase())
                            << " currentPriority=" << node.GetPriority() << '\n';
                        DebugLog(oss.str());
#endif
                        return;
                }

                updateNode->SetPhase(phase);
                updateNode->SetPriority(priority);
                if (m_orderOverrides.erase(&node) > 0)
                {
#if UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] Cleared redundant order override for UpdateNode node=" << &node
                            << " phase=" << static_cast<int>(phase)
                            << " priority=" << priority << '\n';
                        DebugLog(oss.str());
#endif
                }
        }

        void UpdateScheduler::ApplyPendingRegistration(IUpdatable* node)
        {
                if (!node)
                {
                        DebugLog("[UpdateScheduler] Null node encountered in pending registration\n");
                        return;
                }

                if (IsRegistered(node))
                {
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] Pending registration ignored because node already registered node=" << node << '\n';
                        DebugLog(oss.str());
                        return;
                }

                RegisterInternal(*node);
        }

        void UpdateScheduler::ApplyPendingUnregistration(IUpdatable* node)
        {
                if (!node)
                {
                        DebugLog("[UpdateScheduler] Null node encountered in pending unregistration\n");
                        return;
                }

                if (!IsRegistered(node))
                {
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] Pending unregistration ignored because node is not registered node=" << node << '\n';
                        DebugLog(oss.str());
                        return;
                }

                UnregisterInternal(*node);
        }

        void UpdateScheduler::ApplyPendingOrderChange(const PendingOrderChange& change)
        {
                IUpdatable* node = change.node;
                if (!node)
                {
                        DebugLog("[UpdateScheduler] Null node encountered in pending order change\n");
                        return;
                }

                if (!IsRegistered(node))
                {
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] Pending order change ignored because node is not registered node=" << node
                            << " phase=" << static_cast<int>(change.phase)
                            << " priority=" << change.priority << '\n';
                        DebugLog(oss.str());
                        return;
                }

                SetOrderInternal(*node, change.phase, change.priority);
        }

        void UpdateScheduler::ApplyPendingOperations()
        {
                if (!m_pendingUnregistrations.empty())
                {
                        for (IUpdatable* node : m_pendingUnregistrations)
                        {
                                ApplyPendingUnregistration(node);
                        }
                        m_pendingUnregistrations.clear();
                }

                if (!m_pendingRegistrations.empty())
                {
                        for (IUpdatable* node : m_pendingRegistrations)
                        {
                                ApplyPendingRegistration(node);
                        }
                        m_pendingRegistrations.clear();
                }

                if (!m_pendingOrderChanges.empty())
                {
                        for (const auto& change : m_pendingOrderChanges)
                        {
                                ApplyPendingOrderChange(change);
                        }
                        m_pendingOrderChanges.clear();
                }

                auto cleanupNodeReferences = [this](IUpdatable* raw)
                {
                        if (!raw)
                        {
                                return;
                        }

                        m_registrationOrder.erase(raw);
                        m_orderOverrides.erase(raw);
                        auto depIt = std::remove_if(m_dependencies.begin(), m_dependencies.end(),
                                [raw](const Dependency& dep)
                                {
                                        return dep.first == raw || dep.second == raw;
                                });
                        if (depIt != m_dependencies.end())
                        {
                                m_dependencies.erase(depIt, m_dependencies.end());
                        }
                };

                for (auto& handle : m_nodes)
                {
                        if (handle.raw && handle.weak.expired())
                        {
                                cleanupNodeReferences(handle.raw);
                                handle.Expire();
                        }
                }

                auto removeExpiredHandle = [this](const NodeHandle& handle)
                {
                        if (!handle.IsExpired())
                        {
                                return false;
                        }

                        IUpdatable* raw = handle.Get();
                        cleanupNodeReferences(raw);

                        return true;
                };

                auto expiredIt = std::remove_if(m_nodes.begin(), m_nodes.end(), removeExpiredHandle);
                if (expiredIt != m_nodes.end())
                {
                        m_nodes.erase(expiredIt, m_nodes.end());
                }
        }

        void UpdateScheduler::Register(IUpdatable& node)
        {
#if UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS
                {
                        const NodeOrderOverride order = GetEffectiveOrder(&node);
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] Register requested for "
                            << DescribeNode(&node, order.phase, order.priority);
                        if (m_isTicking)
                        {
                                oss << " (deferred)";
                        }
                        oss << '\n';
                        DebugLog(oss.str());
                }
#endif

                if (m_isTicking)
                {
                        bool cancelledPendingUnregistration = false;
                        if (IsPendingUnregistration(&node))
                        {
                                auto it = std::remove(m_pendingUnregistrations.begin(), m_pendingUnregistrations.end(), &node);
                                if (it != m_pendingUnregistrations.end())
                                {
                                        m_pendingUnregistrations.erase(it, m_pendingUnregistrations.end());
                                        cancelledPendingUnregistration = true;
                                        std::ostringstream oss;
                                        oss << "[UpdateScheduler] Pending unregistration cancelled due to re-registration node=" << &node << '\n';
                                        DebugLog(oss.str());
                                        auto handleIt = FindNodeHandle(&node);
                                        if (handleIt != m_nodes.end() && handleIt->IsExpired())
                                        {
                                                auto revived = std::shared_ptr<IUpdatable>(std::addressof(node));
                                                handleIt->raw = revived.get();
                                                handleIt->owner = revived;
                                                handleIt->weak = revived;
                                        }
                                }
                        }

                        if (IsRegistered(&node))
                        {
                                if (cancelledPendingUnregistration)
                                {
                                        std::ostringstream oss;
                                        oss << "[UpdateScheduler] Re-register preserved node in current frame node=" << &node << '\n';
                                        DebugLog(oss.str());
                                }
                                else
                                {
                                        std::ostringstream oss;
                                        oss << "[UpdateScheduler] Register ignored for already registered node=" << &node << '\n';
                                        DebugLog(oss.str());
                                }
                                return;
                        }

                        if (IsPendingRegistration(&node))
                        {
                                std::ostringstream oss;
                                oss << "[UpdateScheduler] Duplicate pending registration detected for node=" << &node << '\n';
                                DebugLog(oss.str());
                                return;
                        }

                        m_pendingRegistrations.emplace_back(&node);
                        return;
                }

                RegisterInternal(node);
        }

        void UpdateScheduler::Unregister(IUpdatable& node)
        {
#if UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS
                {
                        const NodeOrderOverride order = GetEffectiveOrder(&node);
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] Unregister requested for "
                            << DescribeNode(&node, order.phase, order.priority);
                        if (m_isTicking)
                        {
                                oss << " (deferred)";
                        }
                        oss << '\n';
                        DebugLog(oss.str());
                }
#endif

                if (m_isTicking)
                {
                        if (IsPendingRegistration(&node))
                        {
                                auto it = std::remove(m_pendingRegistrations.begin(), m_pendingRegistrations.end(), &node);
                                if (it != m_pendingRegistrations.end())
                                {
                                        m_pendingRegistrations.erase(it, m_pendingRegistrations.end());
                                        std::ostringstream oss;
                                        oss << "[UpdateScheduler] Pending registration removed because node requested self-unregister node=" << &node << '\n';
                                        DebugLog(oss.str());
                                }
                                return;
                        }

                        if (!IsRegistered(&node))
                        {
                                std::ostringstream oss;
                                oss << "[UpdateScheduler] Unregister ignored for unknown node=" << &node << '\n';
                                DebugLog(oss.str());
                                return;
                        }

                        if (IsPendingUnregistration(&node))
                        {
                                std::ostringstream oss;
                                oss << "[UpdateScheduler] Duplicate pending unregistration detected for node=" << &node << '\n';
                                DebugLog(oss.str());
                                return;
                        }

                        m_pendingUnregistrations.emplace_back(&node);
                        return;
                }

                UnregisterInternal(node);
        }

        void UpdateScheduler::SetOrder(IUpdatable& node, UpdatePhase phase, int priority)
        {
                if (m_isTicking)
                {
                        if (!IsRegistered(&node) && !IsPendingRegistration(&node))
                        {
                                std::ostringstream oss;
                                oss << "[UpdateScheduler] SetOrder ignored because node is not registered node=" << &node
                                    << " phase=" << static_cast<int>(phase)
                                    << " priority=" << priority << '\n';
                                DebugLog(oss.str());
                                return;
                        }

                        PendingOrderChange* pending = FindPendingOrderChange(&node);
                        if (pending)
                        {
                                pending->phase = phase;
                                pending->priority = priority;
                                std::ostringstream oss;
                                oss << "[UpdateScheduler] Pending order change updated for node=" << &node
                                    << " phase=" << static_cast<int>(phase)
                                    << " priority=" << priority << '\n';
                                DebugLog(oss.str());
                                return;
                        }

                        m_pendingOrderChanges.push_back({ &node, phase, priority });
                        return;
                }

                if (!IsRegistered(&node))
                {
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] SetOrder called for node that is not registered node=" << &node
                            << " phase=" << static_cast<int>(phase)
                            << " priority=" << priority << '\n';
                        DebugLog(oss.str());
                }

                SetOrderInternal(node, phase, priority);
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
                ApplyPendingOperations();
                m_isTicking = true;

                if (!m_nodes.empty())
                {
                        // 更新リストをコピーしてソート用に使用する
                        std::vector<IUpdatable*> sortedNodes;
                        sortedNodes.reserve(m_nodes.size());
                        for (const auto& handle : m_nodes)
                        {
                                if (handle.IsExpired())
                                {
                                        continue;
                                }

                                sortedNodes.push_back(handle.Get());
                        }
                        sortedNodes.erase(std::remove(sortedNodes.begin(), sortedNodes.end(), nullptr), sortedNodes.end());
                        std::stable_sort(sortedNodes.begin(), sortedNodes.end(),
                                [this](const IUpdatable* lhs, const IUpdatable* rhs)
                                {
                                        return NodeLess(lhs, rhs);
                                });

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
                        using ReadyQueue = std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<std::size_t>>;
                        ReadyQueue ready;
                        for (std::size_t i = 0; i < nodeCount; ++i)
                        {
                                if (indegree[i] == 0)
                                {
                                        ready.push(i);
                                }
                        }

                        std::vector<std::size_t> processed;
                        processed.reserve(nodeCount);

                        // ready キューから「フェーズ・優先度が最も早いもの」を選び、安定した順序を維持する
                        auto popBestReady = [&ready]() -> std::size_t
                        {
                                std::size_t value = ready.top();
                                ready.pop();
                                return value;
                        };

                        ExecutionLog executionLog;
                        executionLog.reserve(nodeCount);

                        ExecutionLog phasePriorityPlan;
                        phasePriorityPlan.reserve(nodeCount);

                        auto appendExecutionRecord = [this, &executionLog](IUpdatable* node, bool invoked, std::size_t sortedIndex, std::size_t executionIndex)
                        {
                                if (!node)
                                {
                                        return;
                                }

                                ExecutionRecord record;
                                record.node = node;
                                const NodeOrderOverride order = GetEffectiveOrder(node);
                                record.phase = order.phase;
                                record.priority = order.priority;
                                record.registrationOrder = GetRegistrationOrder(node);
                                record.sortedIndex = sortedIndex;
                                record.executionIndex = executionIndex;
                                record.invoked = invoked;
                                executionLog.emplace_back(record);
                        };

                        auto appendPhasePriorityRecord = [this, &phasePriorityPlan](IUpdatable* node, std::size_t sortedIndex)
                        {
                                if (!node)
                                {
                                        return;
                                }

                                ExecutionRecord record;
                                record.node = node;
                                const NodeOrderOverride order = GetEffectiveOrder(node);
                                record.phase = order.phase;
                                record.priority = order.priority;
                                record.registrationOrder = GetRegistrationOrder(node);
                                record.sortedIndex = sortedIndex;
                                record.executionIndex = sortedIndex;
                                phasePriorityPlan.emplace_back(record);
                        };

                        for (std::size_t i = 0; i < sortedNodes.size(); ++i)
                        {
                                appendPhasePriorityRecord(sortedNodes[i], i);
                        }

                        while (!ready.empty())
                        {
                                std::size_t current = popBestReady();
                                processed.push_back(current);

                                IUpdatable* node = sortedNodes[current];
                                bool invoked = false;
                                if (node && node->IsEnabled())
                                {
                                        node->OnUpdate(ctx);
                                        invoked = true;
                                }
                                appendExecutionRecord(node, invoked, current, executionLog.size());

                                for (std::size_t next : edges[current])
                                {
                                        if (--indegree[next] == 0)
                                        {
                                                ready.push(next);
                                        }
                                }
                        }

                        if (processed.size() != nodeCount)
                        {
                                std::ostringstream oss;
                                oss << "[UpdateScheduler] cycle detected; executing remaining nodes in registration order. processed="
                                    << processed.size() << " total=" << nodeCount << '\n';
                                DebugLog(oss.str());
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
                                        bool invoked = false;
                                        if (node && node->IsEnabled())
                                        {
                                                node->OnUpdate(ctx);
                                                invoked = true;
                                        }
                                        appendExecutionRecord(node, invoked, i, executionLog.size());
                                }
                        }

                        if (m_executionLogListener)
                        {
                                m_executionLogListener(executionLog);
                        }
#if defined(_DEBUG) && defined(_WIN32)
                        if (!executionLog.empty())
                        {
                                std::ostringstream oss;
                                oss << "[UpdateScheduler] phase/priority plan (phase, priority, order):\n";
                                for (const auto& record : phasePriorityPlan)
                                {
                                        oss << "  node=" << record.node
                                            << " phase=" << static_cast<int>(record.phase)
                                            << " priority=" << record.priority
                                            << " order=" << record.registrationOrder
                                            << " sortedIndex=" << record.sortedIndex
                                            << '\n';
                                }

                                oss << "[UpdateScheduler] execution order (phase, priority, order, invoked):\n";
                                for (const auto& record : executionLog)
                                {
                                        oss << "  node=" << record.node
                                            << " phase=" << static_cast<int>(record.phase)
                                            << " priority=" << record.priority
                                            << " order=" << record.registrationOrder
                                            << " sortedIndex=" << record.sortedIndex
                                            << " execIndex=" << record.executionIndex
                                            << " invoked=" << (record.invoked ? 1 : 0) << '\n';
                                }
                                ::OutputDebugStringA(oss.str().c_str());
                        }
#endif
                }

                m_isTicking = false;
                ApplyPendingOperations();
        }
}

