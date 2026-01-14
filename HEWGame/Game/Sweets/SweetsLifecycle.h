#pragma once

#include <memory>
#include <optional>
#include <unordered_set>

#include "Core/IUpdatable.h"
#include "Sweets/SweetsTypes.h"

namespace Core
{
    class UpdateScheduler;
}

namespace Systems
{
    class DrawSystem;
    class PhysicsMachine;
}

namespace SweetsSpace
{
    class SweetsContacts;
    class SweetsRegistry;
    class SweetsSpawner;

    class SweetsLifecycle
    {
    public:
        SweetsLifecycle(SweetsRegistry& registry,
            SweetsContacts& contacts,
            SweetsSpawner& spawner,
            Systems::DrawSystem* drawSystem,
            Systems::PhysicsMachine* physicsMachine,
            Core::UpdateScheduler* scheduler,
            bool* warnedMissingDrawSystem,
            bool* warnedMissingPhysicsMachine,
            bool* warnedMissingScheduler);

        void OnUpdate(const Core::UpdateContext& ctx);
        void OnLateUpdate(const Core::UpdateContext& ctx);

        void SetHorizontalBounds(float minX, float maxX, std::optional<float> margin = std::nullopt);
        std::optional<std::pair<float, float>> GetHorizontalBounds() const;

        void Erase(const std::shared_ptr<ActiveSweets>& sweets);
        void ClearAllSweets();

        bool TryMarkAsCarried(const std::shared_ptr<ActiveSweets>& sweets);
        bool ReleaseCarriedSweets(const std::shared_ptr<ActiveSweets>& sweets);

        bool RegisterWithSystems(const std::shared_ptr<ActiveSweets>& sweets);
        void RemoveSweetsEntry(const std::shared_ptr<ActiveSweets>& sweets,
            DeleteReason reason,
            const char* context,
            bool logWarnings,
            DeleteBatchContext* batchContext);

    private:
        void UnregisterSweetsFromSystems(const std::shared_ptr<ActiveSweets>& sweets, const char* context, bool logWarnings);

        SweetsRegistry* m_registry;
        SweetsContacts* m_contacts;
        SweetsSpawner* m_spawner;
        Systems::DrawSystem* m_drawSystem;
        Systems::PhysicsMachine* m_physicsMachine;
        Core::UpdateScheduler* m_scheduler;
        bool* m_warnedMissingDrawSystem;
        bool* m_warnedMissingPhysicsMachine;
        bool* m_warnedMissingScheduler;

        std::unordered_set<std::shared_ptr<ActiveSweets>> m_pendingLateDeletion;
        bool m_hasHorizontalBounds{ false };
        float m_minHorizontalBound{ 0.0f };
        float m_maxHorizontalBound{ 0.0f };
        float m_horizontalBoundsMargin{ 0.0f };
        bool m_warnedMissingHorizontalBounds{ false };
        std::unordered_set<ActiveSweets*> m_registeredPhysicsBodies;
    };
}
