#include "Meteors/MeteorsRegistry.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "AABB.h"
#include "Meteors/BaseMeteors.h"

namespace MeteorsSpace
{
    MeteorsRegistry::MeteorsRegistry()
    {
        m_byState.emplace(MeteorsState::Active, MeteorsList{});
        m_byState.emplace(MeteorsState::Stacked, MeteorsList{});
        m_byState.emplace(MeteorsState::Ground, MeteorsList{});
        m_byState.emplace(MeteorsState::Carried, MeteorsList{});
    }

    MeteorsRegistry::MeteorsList& MeteorsRegistry::GetByState(MeteorsState state)
    {
        return m_byState[state];
    }

    const MeteorsRegistry::MeteorsList& MeteorsRegistry::GetByState(MeteorsState state) const
    {
        static const MeteorsList kEmpty;
        auto it = m_byState.find(state);
        if (it == m_byState.end())
        {
            return kEmpty;
        }
        return it->second;
    }

    std::optional<MeteorsState> MeteorsRegistry::GetState(const std::shared_ptr<ActiveMeteors>& meteors) const
    {
        auto it = m_lookup.find(meteors);
        if (it == m_lookup.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    std::optional<MeteorsState> MeteorsRegistry::SetState(const std::shared_ptr<ActiveMeteors>& meteors, MeteorsState state)
    {
        if (!meteors)
        {
            return std::nullopt;
        }

        auto found = m_lookup.find(meteors);
        const MeteorsState previousState = (found != m_lookup.end()) ? found->second : MeteorsState::Active;
        if (found != m_lookup.end())
        {
            if (found->second == state)
            {
                return std::nullopt;
            }

            auto& currentList = GetByState(found->second);
            currentList.remove(meteors);
        }

        auto& targetList = GetByState(state);
        targetList.push_back(meteors);
        m_lookup[meteors] = state;

        return previousState;
    }

    void MeteorsRegistry::Remove(const std::shared_ptr<ActiveMeteors>& meteors)
    {
        if (!meteors)
        {
            return;
        }

        auto found = m_lookup.find(meteors);
        if (found == m_lookup.end())
        {
            return;
        }

        auto& currentList = GetByState(found->second);
        currentList.remove(meteors);
        m_lookup.erase(found);
    }

    int MeteorsRegistry::CountAll() const
    {
        int count = 0;
        for (const auto& pair : m_byState)
        {
            count += static_cast<int>(pair.second.size());
        }
        return count;
    }

    std::unordered_map<MeteorsState, MeteorsRegistry::MeteorsList, MeteorsRegistry::MeteorsStateHash>&
    MeteorsRegistry::MutableStateMap()
    {
        return m_byState;
    }

    const std::unordered_map<MeteorsState, MeteorsRegistry::MeteorsList, MeteorsRegistry::MeteorsStateHash>&
    MeteorsRegistry::StateMap() const
    {
        return m_byState;
    }

    std::unordered_map<std::shared_ptr<ActiveMeteors>, MeteorsState>& MeteorsRegistry::Lookup()
    {
        return m_lookup;
    }

    const std::unordered_map<std::shared_ptr<ActiveMeteors>, MeteorsState>& MeteorsRegistry::Lookup() const
    {
        return m_lookup;
    }

    PickupQueryResult MeteorsRegistry::FindNearestCarryable(
        const float3& pickupCenter,
        float halfWidth,
        float halfHeight,
        float halfDepth,
        float depthThreshold,
        const std::vector<std::shared_ptr<ActiveMeteors>>& exclude,
        float maxDistance) const
    {
        PickupQueryResult result{};
        result.failReason = PickupFailReason::InvalidConfiguration;

        const auto sanitizeCoordinate = [](float value)
        {
            return std::isfinite(value) ? value : 0.0f;
        };

        const float3 sanitizedCenter
        {
            sanitizeCoordinate(pickupCenter.x),
            sanitizeCoordinate(pickupCenter.y),
            sanitizeCoordinate(pickupCenter.z),
        };
        if (!(halfWidth > 0.0f) || !(halfHeight > 0.0f))
        {
            return result;
        }
        const float sanitizedHalfDepth = std::max(0.0f, std::isfinite(halfDepth) ? halfDepth : 0.0f);
        const float sanitizedDepthThreshold = (std::isfinite(depthThreshold) && depthThreshold >= 0.0f)
            ? depthThreshold
            : std::numeric_limits<float>::infinity();
        const float sanitizedMaxDistance = (std::isfinite(maxDistance) && maxDistance > 0.0f)
            ? maxDistance
            : std::numeric_limits<float>::infinity();

        const float broadPhaseHalfWidth = halfWidth + sanitizedMaxDistance;
        const float broadPhaseHalfHeight = halfHeight + sanitizedMaxDistance;
        const float broadPhaseHalfDepth = std::max(sanitizedHalfDepth, sanitizedDepthThreshold);

        AABB pickupBounds{};
        pickupBounds.min.x = sanitizedCenter.x - broadPhaseHalfWidth;
        pickupBounds.max.x = sanitizedCenter.x + broadPhaseHalfWidth;
        pickupBounds.min.y = sanitizedCenter.y - broadPhaseHalfHeight;
        pickupBounds.max.y = sanitizedCenter.y + broadPhaseHalfHeight;
        pickupBounds.min.z = sanitizedCenter.z - broadPhaseHalfDepth;
        pickupBounds.max.z = sanitizedCenter.z + broadPhaseHalfDepth;

        const auto intersects = [&](const AABB& bounds)
        {
            const float overlapX = std::min(bounds.max.x, pickupBounds.max.x) - std::max(bounds.min.x, pickupBounds.min.x);
            const float overlapY = std::min(bounds.max.y, pickupBounds.max.y) - std::max(bounds.min.y, pickupBounds.min.y);
            const float overlapZ = std::min(bounds.max.z, pickupBounds.max.z) - std::max(bounds.min.z, pickupBounds.min.z);
            return overlapX >= 0.0f && overlapY >= 0.0f && overlapZ >= 0.0f;
        };

        const auto computeDepthDelta = [&](const AABB& bounds)
        {
            const float frontExcess = bounds.min.z - (sanitizedCenter.z + sanitizedHalfDepth);
            const float backExcess = (sanitizedCenter.z - sanitizedHalfDepth) - bounds.max.z;
            const float outside = std::max(frontExcess, backExcess);
            return outside > 0.0f ? outside : 0.0f;
        };

        const auto isExcluded = [&](const std::shared_ptr<ActiveMeteors>& meteors)
        {
            return std::find(exclude.begin(), exclude.end(), meteors) != exclude.end();
        };

        std::shared_ptr<ActiveMeteors> best;
        float bestDistanceSq = std::numeric_limits<float>::max();
        float nearestMeasuredDepthDelta = std::numeric_limits<float>::quiet_NaN();

        const auto scanState = [&](MeteorsState state)
        {
            auto it = m_byState.find(state);
            if (it == m_byState.end())
            {
                return;
            }

            for (const auto& meteors : it->second)
            {
                if (!meteors || meteors->IsPendingDestroy())
                {
                    continue;
                }

                if (isExcluded(meteors))
                {
                    continue;
                }

                meteors->SyncColliderBounds();
                const AABB bounds = meteors->GetAABB();
                if (!intersects(bounds))
                {
                    continue;
                }

                const float centerX = (bounds.min.x + bounds.max.x) * 0.5f;
                const float centerY = (bounds.min.y + bounds.max.y) * 0.5f;
                const float dx = centerX - sanitizedCenter.x;
                const float dy = centerY - sanitizedCenter.y;
                const float distanceSq = (dx * dx) + (dy * dy);
                if (distanceSq <= bestDistanceSq)
                {
                    bestDistanceSq = distanceSq;
                    best = meteors;
                    nearestMeasuredDepthDelta = computeDepthDelta(bounds);
                }
            }
        };

        scanState(MeteorsState::Ground);
        scanState(MeteorsState::Active);
        scanState(MeteorsState::Stacked);

        if (bestDistanceSq == std::numeric_limits<float>::max())
        {
            result.failReason = PickupFailReason::NoCandidates;
            return result;
        }

        const float bestDistance = std::sqrt(bestDistanceSq);
        result.distance = bestDistance;
        result.depthDelta = nearestMeasuredDepthDelta;

        if (bestDistance > sanitizedMaxDistance)
        {
            result.failReason = PickupFailReason::DistanceExceeded;
            return result;
        }

        if (nearestMeasuredDepthDelta > sanitizedDepthThreshold)
        {
            result.failReason = PickupFailReason::DepthExceeded;
            return result;
        }

        result.result = PickupResult::Success;
        result.failReason = PickupFailReason::None;
        result.meteors = best;
        return result;
    }

    float MeteorsRegistry::GetHighestStackedHeight() const
    {
        float maxHeight = 0.0f;
        const auto& stacked = GetByState(MeteorsState::Stacked);
        for (const auto& meteors : stacked)
        {
            if (!meteors || meteors->IsPendingDestroy())
            {
                continue;
            }

            meteors->SyncColliderBounds();
            const AABB bounds = meteors->GetAABB();
            float meteorsTop = bounds.max.y;

            maxHeight = std::max(maxHeight, meteorsTop);
        }

        return maxHeight;
    }
}
