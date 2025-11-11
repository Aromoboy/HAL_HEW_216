#ifndef HEW_CORE_UPDATESCHEDULER_H
#define HEW_CORE_UPDATESCHEDULER_H

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

class Scene;

namespace Core
{
        enum class UpdatePhase : std::uint8_t
        {
                Input = 0,
                Simulation,
                PostSimulation,
                Count
        };

        struct UpdateTimer
        {
                double totalSeconds = 0.0;
                unsigned long long frameIndex = 0;
        };

        struct UpdateInput
        {
                bool (*isKeyPress)(unsigned char) = nullptr;
                bool (*isKeyTrigger)(unsigned char) = nullptr;
                bool (*isKeyRelease)(unsigned char) = nullptr;
                bool (*isKeyRepeat)(unsigned char) = nullptr;

                bool IsValid() const
                {
                        return isKeyPress && isKeyTrigger && isKeyRelease && isKeyRepeat;
                }
        };

        struct UpdateContext
        {
                float deltaSeconds = 0.0f;
                UpdateTimer timer{};
                UpdateInput input{};
                Scene* activeScene = nullptr;
        };

        class IUpdatable
        {
        public:
                virtual ~IUpdatable() = default;
                virtual void OnUpdate(const UpdateContext& context) = 0;
        };

        class UpdateNode
        {
        public:
                UpdateNode(IUpdatable* target, UpdatePhase phase, int order);

                IUpdatable* GetTarget() const { return m_target; }
                UpdatePhase GetPhase() const { return m_phase; }
                int GetOrder() const { return m_order; }
                bool IsEnabled() const { return m_enabled; }

                void SetOrder(int order);
                void SetPhase(UpdatePhase phase);
                void SetEnabled(bool enabled);

        private:
                IUpdatable* m_target;
                UpdatePhase m_phase;
                int m_order;
                bool m_enabled;

                friend class UpdateScheduler;
        };

        class UpdateScheduler
        {
        public:
                UpdateScheduler();

                UpdateNode* Register(IUpdatable* target, UpdatePhase phase, int order = 0);
                void Unregister(IUpdatable* target);
                void SetOrder(IUpdatable* target, int order);
                void SetEnabled(IUpdatable* target, bool enabled);
                UpdateNode* GetNode(IUpdatable* target);
                const UpdateNode* GetNode(const IUpdatable* target) const;

                void Tick(const UpdateContext& context);

                void AddDependency(IUpdatable* before, IUpdatable* after);

        private:
                enum class PendingType
                {
                        Register,
                        Unregister,
                        SetOrder,
                        SetEnabled
                };

                struct PendingOperation
                {
                        PendingType type;
                        IUpdatable* target;
                        UpdatePhase phase;
                        int order;
                        bool enabled;
                };

                using Bucket = std::vector<UpdateNode*>;

                UpdateNode* ApplyRegister(IUpdatable* target, UpdatePhase phase, int order);
                void ApplyUnregister(IUpdatable* target);
                void ApplySetOrder(IUpdatable* target, int order);
                void ApplySetEnabled(IUpdatable* target, bool enabled);
                void FlushPending();
                void MarkDirty(UpdatePhase phase);
                static std::size_t PhaseIndex(UpdatePhase phase);
                void RemoveFromBucket(UpdateNode* node, UpdatePhase phase);
                void AddToBucket(UpdateNode* node, UpdatePhase phase);

        private:
                std::array<Bucket, static_cast<std::size_t>(UpdatePhase::Count)> m_buckets;
                std::unordered_map<IUpdatable*, std::unique_ptr<UpdateNode>> m_nodes;
                std::vector<PendingOperation> m_pending;
                std::array<bool, static_cast<std::size_t>(UpdatePhase::Count)> m_dirty{};
                bool m_isTicking;
        };
}

#endif // HEW_CORE_UPDATESCHEDULER_H
