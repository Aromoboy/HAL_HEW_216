#pragma once

#include <list>
#include <memory>
#include <optional>
#include <unordered_map>

#include "Sweets/SweetsTypes.h"

namespace SweetsSpace
{
    class SweetsRegistry
    {
    public:
        using SweetsList = std::list<std::shared_ptr<ActiveSweets>>;

        SweetsRegistry();

        SweetsList& GetByState(SweetsState state);
        const SweetsList& GetByState(SweetsState state) const;

        std::optional<SweetsState> GetState(const std::shared_ptr<ActiveSweets>& sweets) const;
        std::optional<SweetsState> SetState(const std::shared_ptr<ActiveSweets>& sweets, SweetsState state);
        void Remove(const std::shared_ptr<ActiveSweets>& sweets);

        int CountAll() const;

        std::unordered_map<SweetsState, SweetsList>& MutableStateMap();
        const std::unordered_map<SweetsState, SweetsList>& StateMap() const;
        std::unordered_map<std::shared_ptr<ActiveSweets>, SweetsState>& Lookup();
        const std::unordered_map<std::shared_ptr<ActiveSweets>, SweetsState>& Lookup() const;

        PickupQueryResult FindNearestCarryable(
            const float3& pickupCenter,
            float halfWidth,
            float halfHeight,
            float halfDepth,
            float depthThreshold,
            const std::vector<std::shared_ptr<ActiveSweets>>& exclude,
            float maxDistance) const;

        float GetHighestStackedHeight() const;

    private:
        struct SweetsStateHash
        {
            std::size_t operator()(SweetsState state) const noexcept
            {
                return static_cast<std::size_t>(state);
            }
        };

        std::unordered_map<SweetsState, SweetsList, SweetsStateHash> m_byState;
        std::unordered_map<std::shared_ptr<ActiveSweets>, SweetsState> m_lookup;
    };
}
