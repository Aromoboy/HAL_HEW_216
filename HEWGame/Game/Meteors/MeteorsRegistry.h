#pragma once

#include <list>
#include <memory>
#include <optional>
#include <unordered_map>

#include "Meteors/MeteorsTypes.h"

namespace MeteorsSpace
{
    class MeteorsRegistry
    {
        struct MeteorsStateHash
        {
            std::size_t operator()(MeteorsState state) const noexcept
            {
                return static_cast<std::size_t>(state);
            }
        };

    public:
        using MeteorsList = std::list<std::shared_ptr<ActiveMeteors>>;

        MeteorsRegistry();

        MeteorsList& GetByState(MeteorsState state);
        const MeteorsList& GetByState(MeteorsState state) const;

        std::optional<MeteorsState> GetState(const std::shared_ptr<ActiveMeteors>& meteors) const;
        std::optional<MeteorsState> SetState(const std::shared_ptr<ActiveMeteors>& meteors, MeteorsState state);
        void Remove(const std::shared_ptr<ActiveMeteors>& meteors);

        int CountAll() const;

        std::unordered_map<MeteorsState, MeteorsList, MeteorsStateHash>& MutableStateMap();
        const std::unordered_map<MeteorsState, MeteorsList, MeteorsStateHash>& StateMap() const;
        std::unordered_map<std::shared_ptr<ActiveMeteors>, MeteorsState>& Lookup();
        const std::unordered_map<std::shared_ptr<ActiveMeteors>, MeteorsState>& Lookup() const;

        PickupQueryResult FindNearestCarryable(
            const float3& pickupCenter,
            float halfWidth,
            float halfHeight,
            float halfDepth,
            float depthThreshold,
            const std::vector<std::shared_ptr<ActiveMeteors>>& exclude,
            float maxDistance) const;

        float GetHighestStackedHeight() const;

    private:
        std::unordered_map<MeteorsState, MeteorsList, MeteorsStateHash> m_byState;
        std::unordered_map<std::shared_ptr<ActiveMeteors>, MeteorsState> m_lookup;
    };
}
