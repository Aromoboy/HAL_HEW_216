#include "Core/UpdateScheduler.h"

#include <algorithm>
#if defined(CORE_UPDATE_TRACE)
#       include <sstream>
#       include <windows.h>
#endif

namespace
{
        using Core::UpdateNode;
        using Core::UpdatePhase;
#if defined(CORE_UPDATE_TRACE)
        void TraceOperation(const char* action, const UpdateNode* node)
        {
                if (!node)
                {
                        return;
                }

                std::ostringstream oss;
                oss << "[Core::UpdateScheduler] " << action
                    << " target=" << node->GetTarget()
                    << " phase=" << static_cast<int>(node->GetPhase())
                    << " order=" << node->GetOrder()
                    << " enabled=" << (node->IsEnabled() ? "true" : "false")
                    << "\n";
                OutputDebugStringA(oss.str().c_str());
        }

        void TraceOperation(const char* action, const Core::IUpdatable* target)
        {
                std::ostringstream oss;
                oss << "[Core::UpdateScheduler] " << action
                    << " target=" << target
                    << "\n";
                OutputDebugStringA(oss.str().c_str());
        }
#endif
}

namespace Core
{
        namespace
        {
                constexpr std::size_t PhaseCount()
                {
                        return static_cast<std::size_t>(UpdatePhase::Count);
                }
        }

        UpdateNode::UpdateNode(IUpdatable* target, UpdatePhase phase, int order)
                : m_target(target)
                , m_phase(phase)
                , m_order(order)
                , m_enabled(true)
        {
        }

        void UpdateNode::SetOrder(int order)
        {
                m_order = order;
        }

        void UpdateNode::SetPhase(UpdatePhase phase)
        {
                m_phase = phase;
        }

        void UpdateNode::SetEnabled(bool enabled)
        {
                m_enabled = enabled;
        }

        UpdateScheduler::UpdateScheduler()
                : m_isTicking(false)
        {
                m_dirty.fill(true);
        }

        std::size_t UpdateScheduler::PhaseIndex(UpdatePhase phase)
        {
                return static_cast<std::size_t>(phase);
        }

        void UpdateScheduler::MarkDirty(UpdatePhase phase)
        {
                m_dirty[PhaseIndex(phase)] = true;
        }

        void UpdateScheduler::AddToBucket(UpdateNode* node, UpdatePhase phase)
        {
                auto index = PhaseIndex(phase);
                m_buckets[index].push_back(node);
                MarkDirty(phase);
        }

        void UpdateScheduler::RemoveFromBucket(UpdateNode* node, UpdatePhase phase)
        {
                auto index = PhaseIndex(phase);
                auto& bucket = m_buckets[index];
                bucket.erase(std::remove(bucket.begin(), bucket.end(), node), bucket.end());
                MarkDirty(phase);
        }

        UpdateNode* UpdateScheduler::ApplyRegister(IUpdatable* target, UpdatePhase phase, int order)
        {
                if (!target)
                {
                        return nullptr;
                }

                auto found = m_nodes.find(target);
                if (found != m_nodes.end())
                {
                        UpdateNode* node = found->second.get();
                        auto previousPhase = node->GetPhase();
                        if (previousPhase != phase)
                        {
                                RemoveFromBucket(node, previousPhase);
                                node->SetPhase(phase);
                                AddToBucket(node, phase);
                        }
                        if (node->GetOrder() != order)
                        {
                                node->SetOrder(order);
                                MarkDirty(node->GetPhase());
                        }
#if defined(CORE_UPDATE_TRACE)
                        TraceOperation("updated", node);
#endif
                        return node;
                }

                auto node = std::make_unique<UpdateNode>(target, phase, order);
                UpdateNode* raw = node.get();
                m_nodes.emplace(target, std::move(node));
                AddToBucket(raw, phase);
#if defined(CORE_UPDATE_TRACE)
                TraceOperation("registered", raw);
#endif
                return raw;
        }

        void UpdateScheduler::ApplyUnregister(IUpdatable* target)
        {
                if (!target)
                {
                        return;
                }

                auto found = m_nodes.find(target);
                if (found == m_nodes.end())
                {
                        return;
                }

                UpdateNode* node = found->second.get();
                RemoveFromBucket(node, node->GetPhase());
#if defined(CORE_UPDATE_TRACE)
                TraceOperation("unregistered", node);
#endif
                m_nodes.erase(found);
        }

        void UpdateScheduler::ApplySetOrder(IUpdatable* target, int order)
        {
                if (!target)
                {
                        return;
                }

                auto found = m_nodes.find(target);
                if (found == m_nodes.end())
                {
                        return;
                }

                UpdateNode* node = found->second.get();
                if (node->GetOrder() != order)
                {
                        node->SetOrder(order);
                        MarkDirty(node->GetPhase());
#if defined(CORE_UPDATE_TRACE)
                        TraceOperation("set-order", node);
#endif
                }
        }

        void UpdateScheduler::ApplySetEnabled(IUpdatable* target, bool enabled)
        {
                if (!target)
                {
                        return;
                }

                auto found = m_nodes.find(target);
                if (found == m_nodes.end())
                {
                        return;
                }

                UpdateNode* node = found->second.get();
                if (node->IsEnabled() != enabled)
                {
                        node->SetEnabled(enabled);
#if defined(CORE_UPDATE_TRACE)
                        TraceOperation(enabled ? "enabled" : "disabled", node);
#endif
                }
        }

        void UpdateScheduler::FlushPending()
        {
                if (m_pending.empty())
                {
                        return;
                }

                auto operations = m_pending;
                m_pending.clear();
                for (const auto& op : operations)
                {
                        switch (op.type)
                        {
                        case PendingType::Register:
                                ApplyRegister(op.target, op.phase, op.order);
                                break;
                        case PendingType::Unregister:
                                ApplyUnregister(op.target);
                                break;
                        case PendingType::SetOrder:
                                ApplySetOrder(op.target, op.order);
                                break;
                        case PendingType::SetEnabled:
                                ApplySetEnabled(op.target, op.enabled);
                                break;
                        }
                }
        }

        UpdateNode* UpdateScheduler::Register(IUpdatable* target, UpdatePhase phase, int order)
        {
                if (m_isTicking)
                {
                        m_pending.push_back({PendingType::Register, target, phase, order, true});
#if defined(CORE_UPDATE_TRACE)
                        TraceOperation("queue-register", target);
#endif
                        return GetNode(target);
                }
                return ApplyRegister(target, phase, order);
        }

        void UpdateScheduler::Unregister(IUpdatable* target)
        {
                if (m_isTicking)
                {
                        m_pending.push_back({PendingType::Unregister, target, UpdatePhase::Input, 0, false});
#if defined(CORE_UPDATE_TRACE)
                        TraceOperation("queue-unregister", target);
#endif
                        return;
                }
                ApplyUnregister(target);
        }

        void UpdateScheduler::SetOrder(IUpdatable* target, int order)
        {
                if (m_isTicking)
                {
                        m_pending.push_back({PendingType::SetOrder, target, UpdatePhase::Input, order, false});
#if defined(CORE_UPDATE_TRACE)
                        TraceOperation("queue-set-order", target);
#endif
                        return;
                }
                ApplySetOrder(target, order);
        }

        void UpdateScheduler::SetEnabled(IUpdatable* target, bool enabled)
        {
                if (m_isTicking)
                {
                        m_pending.push_back({PendingType::SetEnabled, target, UpdatePhase::Input, 0, enabled});
#if defined(CORE_UPDATE_TRACE)
                        TraceOperation(enabled ? "queue-enable" : "queue-disable", target);
#endif
                        return;
                }
                ApplySetEnabled(target, enabled);
        }

        UpdateNode* UpdateScheduler::GetNode(IUpdatable* target)
        {
                auto found = m_nodes.find(target);
                if (found == m_nodes.end())
                {
                        return nullptr;
                }
                return found->second.get();
        }

        const UpdateNode* UpdateScheduler::GetNode(const IUpdatable* target) const
        {
                auto found = m_nodes.find(const_cast<IUpdatable*>(target));
                if (found == m_nodes.end())
                {
                        return nullptr;
                }
                return found->second.get();
        }

        void UpdateScheduler::Tick(const UpdateContext& context)
        {
                m_isTicking = true;

                for (std::size_t index = 0; index < PhaseCount(); ++index)
                {
                        auto& bucket = m_buckets[index];
                        if (m_dirty[index])
                        {
                                std::stable_sort(bucket.begin(), bucket.end(), [](const UpdateNode* lhs, const UpdateNode* rhs) {
                                        if (lhs->GetOrder() == rhs->GetOrder())
                                        {
                                                return lhs < rhs;
                                        }
                                        return lhs->GetOrder() < rhs->GetOrder();
                                });
                                m_dirty[index] = false;
                        }

                        for (UpdateNode* node : bucket)
                        {
                                if (!node->IsEnabled() || node->GetTarget() == nullptr)
                                {
                                        continue;
                                }
                                node->GetTarget()->OnUpdate(context);
                        }
                }

                m_isTicking = false;
                FlushPending();
        }

        void UpdateScheduler::AddDependency(IUpdatable* before, IUpdatable* after)
        {
                (void)before;
                (void)after;
#if defined(CORE_UPDATE_TRACE)
                OutputDebugStringA("[Core::UpdateScheduler] AddDependency is not implemented yet\n");
#endif
        }
}
