#include "SweetsManager.h"

#include "Sweets.h"

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
        (void)ctx;
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
