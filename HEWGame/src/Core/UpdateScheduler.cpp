#include "Core/UpdateScheduler.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <sstream>
#include <string>
#include <typeinfo>
#include <unordered_map>

#if !defined(UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS)
#if defined(_DEBUG)
#define UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS 0
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

                constexpr UpdatePhase kPhaseOrder[] = {
                        UpdatePhase::Input,
                        UpdatePhase::PrePhysics,
                        UpdatePhase::Physics,
                        UpdatePhase::PostPhysics,
                        UpdatePhase::Gameplay,
                        UpdatePhase::Camera,
                        UpdatePhase::UI,
                        UpdatePhase::Late
                };
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

                const UpdatePhase lhsPhase = lhs->GetPhase();
                const UpdatePhase rhsPhase = rhs->GetPhase();
                if (lhsPhase != rhsPhase)
                {
                        return static_cast<int>(lhsPhase) < static_cast<int>(rhsPhase);
                }

                if (lhs->GetPriority() != rhs->GetPriority())
                {
                        return lhs->GetPriority() < rhs->GetPriority();
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

        UpdateScheduler::ExecutionRecord UpdateScheduler::BuildExecutionRecord(const IUpdatable* node, std::size_t sortedIndex, std::size_t executionIndex) const
        {
                ExecutionRecord record;
                record.node = node;
                if (node)
                {
                        record.phase = node->GetPhase();
                        record.priority = node->GetPriority();
                        record.registrationOrder = GetRegistrationOrder(node);
                }
                record.sortedIndex = sortedIndex;
                record.executionIndex = executionIndex;
                record.invoked = false;
                return record;
        }

        void UpdateScheduler::SetExecutionLogListener(ExecutionLogListener listener)
        {
                m_executionLogListener = std::move(listener);
                m_executionPlanDirty = true;
        }

        bool UpdateScheduler::IsRegistered(const IUpdatable* node) const
        {
                if (!node)
                {
                        return false;
                }

                return std::find(m_nodes.begin(), m_nodes.end(), node) != m_nodes.end();
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

                m_nodes.push_back(&node);
                m_registrationOrder.emplace(&node, m_nextRegistrationOrder++);
                m_executionPlanDirty = true;

#if UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS
                std::ostringstream oss;
                oss << "[UpdateScheduler] Registered "
                    << DescribeNode(&node, node.GetPhase(), node.GetPriority())
                    << " order=" << (m_nextRegistrationOrder - 1)
                    << '\n';
                DebugLog(oss.str());
#endif
        }

        void UpdateScheduler::UnregisterInternal(IUpdatable& node)
        {
                auto it = std::remove(m_nodes.begin(), m_nodes.end(), &node);
                if (it == m_nodes.end())
                {
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] UnregisterInternal called for unknown node=" << &node << '\n';
                        DebugLog(oss.str());
                }
                else
                {
                        m_nodes.erase(it, m_nodes.end());
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

                m_executionPlanDirty = true;

#if UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS
                std::ostringstream oss;
                oss << "[UpdateScheduler] Unregistered "
                    << DescribeNode(&node, node.GetPhase(), node.GetPriority())
                    << '\n';
                DebugLog(oss.str());
#endif
        }

        void UpdateScheduler::SetOrderInternal(IUpdatable& node, UpdatePhase phase, int priority)
        {
                auto* updateNode = dynamic_cast<UpdateNode*>(&node);
                if (!updateNode)
                {
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] SetOrder ignored because node is not UpdateNode node=" << &node
                            << " phase=" << static_cast<int>(phase)
                            << " priority=" << priority << '\n';
                        DebugLog(oss.str());
                        return;
                }

                const bool phaseChanged = updateNode->GetPhase() != phase;
                const bool priorityChanged = updateNode->GetPriority() != priority;
                updateNode->SetPhase(phase);
                updateNode->SetPriority(priority);
                if (phaseChanged || priorityChanged)
                {
                        m_executionPlanDirty = true;
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
        }

        RegistrationHandle UpdateScheduler::Register(IUpdatable& node)
        {
#if UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS
                {
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] Register requested for "
                            << DescribeNode(&node, node.GetPhase(), node.GetPriority());
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
                        if (IsPendingUnregistration(&node))
                        {
                                auto it = std::remove(m_pendingUnregistrations.begin(), m_pendingUnregistrations.end(), &node);
                                if (it != m_pendingUnregistrations.end())
                                {
                                        m_pendingUnregistrations.erase(it, m_pendingUnregistrations.end());
                                        std::ostringstream oss;
                                        oss << "[UpdateScheduler] Pending unregistration cancelled due to re-registration node=" << &node << '\n';
                                        DebugLog(oss.str());
                                        return RegistrationHandle(this, &node);
                                }
                        }

                        if (IsRegistered(&node))
                        {
                                std::ostringstream oss;
                                oss << "[UpdateScheduler] Register ignored for already registered node=" << &node << '\n';
                                DebugLog(oss.str());
                                return {};
                        }

                        if (IsPendingRegistration(&node))
                        {
                                std::ostringstream oss;
                                oss << "[UpdateScheduler] Duplicate pending registration detected for node=" << &node << '\n';
                                DebugLog(oss.str());
                                return {};
                        }

                        m_pendingRegistrations.emplace_back(&node);
                        return RegistrationHandle(this, &node);
                }

                if (IsRegistered(&node))
                {
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] Register ignored for already registered node=" << &node << '\n';
                        DebugLog(oss.str());
                        return {};
                }

                RegisterInternal(node);
                return RegistrationHandle(this, &node);
        }

        void UpdateScheduler::Unregister(IUpdatable& node)
        {
#if UPDATE_SCHEDULER_ENABLE_DIAGNOSTICS
                {
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] Unregister requested for "
                            << DescribeNode(&node, node.GetPhase(), node.GetPriority());
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
                        return;
                }

                if (before.GetPhase() != after.GetPhase())
                {
                        std::ostringstream oss;
                        oss << "[UpdateScheduler] AddDependency ignored due to phase mismatch before="
                            << DescribeNode(&before, before.GetPhase(), before.GetPriority())
                            << " after=" << DescribeNode(&after, after.GetPhase(), after.GetPriority()) << '\n';
                        DebugLog(oss.str());
                        return;
                }

                Dependency candidate(&before, &after);
                auto it = std::find(m_dependencies.begin(), m_dependencies.end(), candidate);
                if (it == m_dependencies.end())
                {
                        m_dependencies.emplace_back(candidate);
                        m_executionPlanDirty = true;
                }
        }

        void UpdateScheduler::ClearDependencies()
        {
                if (!m_dependencies.empty())
                {
                        m_dependencies.clear();
                        m_executionPlanDirty = true;
                }
        }

        void UpdateScheduler::RebuildExecutionPlan()
        {
                m_executionPlan.clear();

                std::vector<Dependency> filteredDependencies;
                filteredDependencies.reserve(m_dependencies.size());
                for (const auto& dep : m_dependencies)
                {
                        if (!dep.first || !dep.second)
                        {
                                continue;
                        }
                        if (!IsRegistered(dep.first) || !IsRegistered(dep.second))
                        {
                                continue;
                        }
                        if (dep.first->GetPhase() != dep.second->GetPhase())
                        {
                                continue;
                        }
                        filteredDependencies.push_back(dep);
                }
                if (filteredDependencies.size() != m_dependencies.size())
                {
                        m_dependencies = std::move(filteredDependencies);
                }

                ExecutionLog executionLog;
                std::size_t executionIndex = 0;

                auto phaseLess = [this](const IUpdatable* lhs, const IUpdatable* rhs)
                {
                        if (!lhs)
                        {
                                return rhs != nullptr;
                        }
                        if (!rhs)
                        {
                                return false;
                        }
                        if (lhs->GetPriority() != rhs->GetPriority())
                        {
                                return lhs->GetPriority() < rhs->GetPriority();
                        }
                        return GetRegistrationOrder(lhs) < GetRegistrationOrder(rhs);
                };

                for (UpdatePhase phase : kPhaseOrder)
                {
                        std::vector<IUpdatable*> phaseNodes;
                        for (IUpdatable* node : m_nodes)
                        {
                                if (!node)
                                {
                                        continue;
                                }
                                if (node->GetPhase() == phase)
                                {
                                        phaseNodes.push_back(node);
                                }
                        }

                        if (phaseNodes.empty())
                        {
                                continue;
                        }

                        std::stable_sort(phaseNodes.begin(), phaseNodes.end(), phaseLess);

                        const std::size_t nodeCount = phaseNodes.size();
                        std::unordered_map<IUpdatable*, std::size_t> indexLookup;
                        indexLookup.reserve(nodeCount);
                        for (std::size_t i = 0; i < nodeCount; ++i)
                        {
                                indexLookup.emplace(phaseNodes[i], i);
                        }

                        std::vector<std::vector<std::size_t>> edges(nodeCount);
                        std::vector<std::size_t> indegree(nodeCount, 0);
                        for (const auto& dep : m_dependencies)
                        {
                                if (dep.first->GetPhase() != phase)
                                {
                                        continue;
                                }
                                auto beforeIt = indexLookup.find(dep.first);
                                auto afterIt = indexLookup.find(dep.second);
                                if (beforeIt == indexLookup.end() || afterIt == indexLookup.end())
                                {
                                        continue;
                                }

                                edges[beforeIt->second].push_back(afterIt->second);
                                ++indegree[afterIt->second];
                        }

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
                        std::vector<bool> processedFlags(nodeCount, false);

                        while (!ready.empty())
                        {
                                std::size_t current = ready.top();
                                ready.pop();
                                processed.push_back(current);
                                processedFlags[current] = true;

                                IUpdatable* node = phaseNodes[current];
                                m_executionPlan.push_back(node);
                                executionLog.emplace_back(BuildExecutionRecord(node, current, executionIndex++));

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
                                oss << "[UpdateScheduler] cycle detected in phase=" << ToString(phase)
                                    << " processed=" << processed.size() << " total=" << nodeCount << '\n';
                                DebugLog(oss.str());

                                for (std::size_t i = 0; i < nodeCount; ++i)
                                {
                                        if (processedFlags[i])
                                        {
                                                continue;
                                        }
                                        IUpdatable* node = phaseNodes[i];
                                        m_executionPlan.push_back(node);
                                        executionLog.emplace_back(BuildExecutionRecord(node, i, executionIndex++));
                                }
                        }
                }

                if (m_executionLogListener)
                {
                        m_executionLogListener(executionLog);
                }

                m_executionPlanDirty = false;
        }

        void UpdateScheduler::Tick(UpdateContext& ctx)
        {
                ApplyPendingOperations();
                m_isTicking = true;

                if (m_executionPlanDirty)
                {
                        RebuildExecutionPlan();
                }

                for (IUpdatable* node : m_executionPlan)
                {
                        if (!node)
                        {
                                continue;
                        }

                        if (node->IsEnabled())
                        {
                                node->OnUpdate(ctx);
                        }
                }

                m_isTicking = false;
                ApplyPendingOperations();
        }
}
