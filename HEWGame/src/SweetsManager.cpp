#include "SweetsManager.h"

#include <sstream>
#include <vector>

#include "Sweets.h"

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    void DebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr const char* kLogPrefix = "[SweetsManager] ";
    constexpr float kRestVelocityThresholdSq = 0.05f * 0.05f;
    constexpr float kGroundHeightThreshold = 0.05f;

    const char* ToString(SweetsSpace::SweetsState state)
    {
        switch (state)
        {
        case SweetsSpace::SweetsState::Active:
            return "Active";
        case SweetsSpace::SweetsState::Stacked:
            return "Stacked";
        case SweetsSpace::SweetsState::Ground:
            return "Ground";
        default:
            return "Unknown";
        }
    }

    float ComputeSpeedSq(const Physics::Physical& physical)
    {
        const auto& v = physical.velocity;
        return (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
    }

    SweetsSpace::SweetsState EvaluateState(SweetsSpace::BaseSweets& sweets, SweetsSpace::SweetsState currentState)
    {
        (void)currentState;

        const float speedSq = ComputeSpeedSq(sweets.GetPhysical());
        const float height = sweets.GetTransform().position.y;

        if (height <= kGroundHeightThreshold && speedSq <= kRestVelocityThresholdSq)
        {
            return SweetsSpace::SweetsState::Ground;
        }

        if (speedSq <= kRestVelocityThresholdSq)
        {
            return SweetsSpace::SweetsState::Stacked;
        }

        return SweetsSpace::SweetsState::Active;
    }
}

namespace SweetsSpace
{
    SweetsManager::SweetsManager()
    {
        m_byState.emplace(SweetsState::Active, SweetsList{});
        m_byState.emplace(SweetsState::Stacked, SweetsList{});
        m_byState.emplace(SweetsState::Ground, SweetsList{});
    }

    SweetsManager::~SweetsManager() = default;

    void SweetsManager::OnUpdate(Core::UpdateContext& ctx)
    {
        struct PendingStateChange
        {
            std::shared_ptr<BaseSweets> sweets;
            SweetsState state;
        };

        std::vector<PendingStateChange> stateChanges;

        for (auto& entry : m_byState)
        {
            SweetsState bucketState = entry.first;
            auto& sweetsList = entry.second;

            auto it = sweetsList.begin();
            while (it != sweetsList.end())
            {
                std::shared_ptr<BaseSweets> sweets = *it;

                if (!sweets)
                {
#if defined(_DEBUG)
                    std::ostringstream oss;
                    oss << kLogPrefix << "Purging null sweets entry from state "
                        << ToString(bucketState) << "\n";
                    DebugTrace(oss.str());
#endif
                    it = sweetsList.erase(it);
                    continue;
                }

                if (!sweets->IsEnabled())
                {
#if defined(_DEBUG)
                    std::ostringstream oss;
                    oss << kLogPrefix << "Purging sweets ptr=" << sweets.get()
                        << " from state " << ToString(bucketState)
                        << " because update node was disabled.\n";
                    DebugTrace(oss.str());
#endif
                    m_lookup.erase(sweets);
                    it = sweetsList.erase(it);
                    continue;
                }

                sweets->OnUpdate(ctx);

                const SweetsState desiredState = EvaluateState(*sweets, bucketState);
                if (desiredState != bucketState)
                {
                    stateChanges.push_back({ sweets, desiredState });
                }

                ++it;
            }
        }

        for (const auto& change : stateChanges)
        {
            auto lookup = m_lookup.find(change.sweets);
            const SweetsState previousState = (lookup != m_lookup.end()) ? lookup->second : SweetsState::Active;
            SetState(change.sweets, change.state);
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << kLogPrefix << "State change ptr=" << change.sweets.get()
                << " " << ToString(previousState) << " -> "
                << ToString(change.state) << "\n";
            DebugTrace(oss.str());
#endif
        }
    }

    void SweetsManager::DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<CameraSpace::ICamera>& camera)
    {
        if (!camera)
        {
            return;
        }

        for (auto& entry : m_byState)
        {
            SweetsList& sweetsList = entry.second;
            for (auto& sweets : sweetsList)
            {
                if (sweets)
                {
                    sweets->Draw(renderer, camera);
                }
            }
        }
    }

    SweetsManager::SweetsList& SweetsManager::GetByState(SweetsState state)
    {
        return m_byState[state];
    }

    const SweetsManager::SweetsList& SweetsManager::GetByState(SweetsState state) const
    {
        static const SweetsList kEmpty;
        auto it = m_byState.find(state);
        if (it == m_byState.end())
        {
            return kEmpty;
        }
        return it->second;
    }

    void SweetsManager::SetState(const std::shared_ptr<BaseSweets>& sweets, SweetsState state)
    {
        if (!sweets)
        {
            return;
        }

        auto found = m_lookup.find(sweets);
        if (found != m_lookup.end())
        {
            if (found->second == state)
            {
                return;
            }

            auto& currentList = GetByState(found->second);
            currentList.remove(sweets);
        }

        auto& targetList = GetByState(state);
        targetList.push_back(sweets);
        m_lookup[sweets] = state;
    }

    void SweetsManager::Erase(const std::shared_ptr<BaseSweets>& sweets)
    {
        if (!sweets)
        {
            return;
        }

        auto found = m_lookup.find(sweets);
        if (found == m_lookup.end())
        {
            return;
        }

        auto& currentList = GetByState(found->second);
        currentList.remove(sweets);
        m_lookup.erase(found);
    }

    std::shared_ptr<BaseSweets> SweetsManager::Spawn(const std::string& kind, Transform transform)
    {
        auto it = m_spawner.find(kind);
        if (it == m_spawner.end())
        {
            return nullptr;
        }

        auto sweets = it->second(transform);
        if (sweets)
        {
            SetState(sweets, SweetsState::Active);
        }

        return sweets;
    }
}
