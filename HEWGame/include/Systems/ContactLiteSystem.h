#pragma once

#include "Core/UpdateNode.h"

#include <vector>

#include "Entities/PhysicalEntity.h"

namespace Systems
{
    class ContactLiteSystem : public Core::UpdateNode
    {
    public:
        ContactLiteSystem();
        ~ContactLiteSystem() override = default;

        void SetBodySource(const std::vector<Entities::PhysicalEntity*>* bodies);
        void SetPenetrationTolerance(float tolerance);
        void SetRestitution(float restitution);
        void SetStableSupportThreshold(float threshold);
        void SetEdgeSlideBoost(float boost);
        void SetUnstableVerticalDamping(float damping);

        float GetPenetrationTolerance() const { return m_penetrationTolerance; }
        float GetRestitution() const { return m_restitution; }
        float GetStableSupportThreshold() const { return m_stableSupportThreshold; }
        float GetEdgeSlideBoost() const { return m_edgeSlideBoost; }
        float GetUnstableVerticalDamping() const { return m_unstableVerticalDamping; }

        void OnUpdate(Core::UpdateContext& ctx) override;

    private:
        const std::vector<Entities::PhysicalEntity*>* m_bodies;
        float m_penetrationTolerance;
        float m_restitution;
        float m_stableSupportThreshold;
        float m_edgeSlideBoost;
        float m_unstableVerticalDamping;
        bool m_loggedMissingBodyList;
        bool m_loggedPlayerPresence;
        bool m_loggedPlayerAbsence;
        bool m_loggedPlayerPlatformContact;
        bool m_loggedPlayerSweetsContact;
        bool m_loggedSuppressedSweetsFloor;
        bool m_loggedPlanarZSeparation;
        bool m_loggedPlanarZIgnore;
    };
}
