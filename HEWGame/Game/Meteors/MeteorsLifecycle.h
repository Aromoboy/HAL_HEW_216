#pragma once

#include <memory>
#include <optional>
#include <unordered_set>

#include "Core/IUpdatable.h"
#include "Meteors/MeteorsTypes.h"

namespace Core
{
    class UpdateScheduler;
}

namespace Systems
{
    class DrawSystem;
    class PhysicsMachine;
}

namespace MeteorsSpace
{
    class MeteorsContacts;
    class MeteorsRegistry;
    class MeteorsSpawner;

    class MeteorsLifecycle
    {
    public:
        MeteorsLifecycle(MeteorsRegistry& registry,
            MeteorsContacts& contacts,
            MeteorsSpawner& spawner,
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

        void Erase(const std::shared_ptr<ActiveMeteors>& meteors);
        void ClearAllMeteors();

        bool TryMarkAsCarried(const std::shared_ptr<ActiveMeteors>& meteors);
        bool ReleaseCarriedMeteors(const std::shared_ptr<ActiveMeteors>& meteors);

        bool RegisterWithSystems(const std::shared_ptr<ActiveMeteors>& meteors);
        void RemoveMeteorsEntry(const std::shared_ptr<ActiveMeteors>& meteors,
            DeleteReason reason,
            const char* context,
            bool logWarnings,
            DeleteBatchContext* batchContext);

    private:
        void UnregisterMeteorsFromSystems(const std::shared_ptr<ActiveMeteors>& meteors, const char* context, bool logWarnings);

        MeteorsRegistry* m_registry;
        MeteorsContacts* m_contacts;
        MeteorsSpawner* m_spawner;
        Systems::DrawSystem* m_drawSystem;
        Systems::PhysicsMachine* m_physicsMachine;
        Core::UpdateScheduler* m_scheduler;
        bool* m_warnedMissingDrawSystem;
        bool* m_warnedMissingPhysicsMachine;
        bool* m_warnedMissingScheduler;

        std::unordered_set<std::shared_ptr<ActiveMeteors>> m_pendingLateDeletion;
        bool m_hasHorizontalBounds{ false };
        float m_minHorizontalBound{ 0.0f };
        float m_maxHorizontalBound{ 0.0f };
        float m_horizontalBoundsMargin{ 0.0f };
        bool m_warnedMissingHorizontalBounds{ false };
        std::unordered_set<ActiveMeteors*> m_registeredPhysicsBodies;
    };
}
