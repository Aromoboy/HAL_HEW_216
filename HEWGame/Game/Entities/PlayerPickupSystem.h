#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "PickupConfig.h"
#include "Sweets/SweetsTypes.h"
#include "Vector.h"

namespace Core
{
    struct UpdateContext;
    class Input;
}

namespace SweetsSpace
{
    class BaseSweets;
    class SweetsManager;
}

namespace Entities
{
    class Player;
    class PlayerModel;

    class PlayerPickupSystem
    {
    public:
        explicit PlayerPickupSystem(SweetsSpace::SweetsManager* manager = nullptr);

        void SetSweetsManager(SweetsSpace::SweetsManager* manager);
        bool IsCarryingSweets() const;
        const std::vector<std::shared_ptr<SweetsSpace::BaseSweets>>& GetCarriedSweets() const;
        void ClearCarriedSweets();

        void SetPickupBox(float width, float height);
        void SetPickupArea(float width, float height, const Stage::PickupOffset& offset);
        void SetPickupMaxDistance(float maxDistance);
        void SetPickupDepthThreshold(float depthThreshold);
        void SetPickupOriginOffset(const float3& offset);
        void SetColliderFootprint(const float3& halfExtents, const float3& centerOffset);
        void SetFacingSign(float facingSign);
        void SetMaxCarry(int maxCarry);

        const SweetsSpace::PickupQueryResult& GetLastPickupResult() const noexcept;

        void HandleCarryInput(Player& player, PlayerModel& model, const Core::UpdateContext& ctx, Core::Input& input);
        void UpdateCarriedSweets(Player& player); 

    private:
        struct ColliderFootprint
        {
            float3 halfExtents{ 0.0f, 0.0f, 0.0f };
            float3 centerOffset{ 0.0f, 0.0f, 0.0f };
        };

        struct PickupOrigin
        {
            float3 center{ 0.0f, 0.0f, 0.0f };
            float halfDepth = 0.05f;
            const char* source = "Unknown";
        };
        //掴み判定
        struct  PickupBox
        {
            float3 center;
            float halfWidth;
            float halfHeight;
        };

        float3 ComputeCarryAnchor(Player& player) const;
        PickupOrigin ComputePickupOrigin(Player& player) const;
        void TryPickupNearest(Player& player);
        void DropCarriedSweets(Player& player);
        //掴み判定の枠
        PickupBox MakePickupBox(Player& player) const;
        bool IsInsidePickupBox(const PickupBox& box, const float3& pos) const;

        SweetsSpace::SweetsManager* m_sweetsManager = nullptr;
        std::vector<std::shared_ptr<SweetsSpace::BaseSweets>> m_carrieds;
        std::unordered_map<SweetsSpace::BaseSweets*, float3> m_carriedRotations;
        int m_maxCarry = 1;
        float m_pickupHalfWidth = 0.0f;
        float m_pickupHalfHeight = 0.0f;
        Stage::PickupOffset m_pickupOffset{};
        float m_pickupMaxDistance = 0.5f;
        float m_pickupDepthThreshold = 0.0f;
        float3 m_pickupOriginOffset{ 0.0f, 0.0f, 0.0f };
        float3 m_carryOffset{ 2.0f, 0.0f, 0.5f };
        float m_dropUpwardBoost = 4.0f;
        float m_facingSign = 1.0f;
        std::optional<ColliderFootprint> m_colliderFootprint;
        SweetsSpace::PickupQueryResult m_lastPickupResult{};
        size_t m_lastCarryInputLoggedCount = 0;
    };
}
