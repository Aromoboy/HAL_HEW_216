#pragma once

#include "Core/UpdateNode.h"

#include <vector>
#include <unordered_set>

#include "Entities/PhysicalEntity.h"

namespace Systems
{
    class ContactLiteSystem : public Core::UpdateNode
    {
    public:
        ContactLiteSystem();
        ~ContactLiteSystem() override = default;

        void SetBodySource(const std::vector<Entities::PhysicalEntity*>* bodies);
        const std::vector<Entities::PhysicalEntity*>* GetBodySource() const { return m_bodies; }
        void SetPenetrationTolerance(float tolerance);
        void SetRestitution(float restitution);
        void SetFriction(float staticCoeff, float dynamicCoeff);
        void SetMaxSlopeCos(float maxSlopeCos);
        void SetVerticalSnapCos(float verticalSnapCos);
        void SetGroundMinCos(float groundMinCos);
        void SetMaxRestitutionVelocity(float maxRestitutionVelocity);
        void SetStableSupportThreshold(float threshold);
        void SetEdgeSlideBoost(float boost);
        void SetUnstableVerticalDamping(float damping);
        void SetBaumgarteFactor(float factor);
        void SetMaxPenetrationCorrection(float maxCorrection);
        void SetMinLinearSpeed(float minSpeed);
        void SetMinVerticalSpeed(float minSpeed);
        void SetMinTangentialSpeed(float minSpeed);
        void SetIncludeKinematicBodies(bool include);
        bool IsIncludingKinematicBodies() const { return m_includeKinematicBodies; }
        void SetAllowKinematicPlayerCollision(bool allow);
        bool IsAllowingKinematicPlayerCollision() const { return m_allowKinematicPlayerCollision; }

        bool HadAnyContactThisFrame(const Entities::PhysicalEntity* body) const;
        bool HasZOverlapWithAnyCollider(const Entities::PhysicalEntity* body) const;

        float GetPenetrationTolerance() const { return m_penetrationTolerance; }
        float GetRestitution() const { return m_restitution; }
        float GetStaticFriction() const { return m_staticFriction; }
        float GetDynamicFriction() const { return m_dynamicFriction; }
        float GetMaxSlopeCos() const { return m_maxSlopeCos; }
        float GetVerticalSnapCos() const { return m_verticalSnapCos; }
        float GetGroundMinCos() const { return m_groundMinCos; }
        float GetMaxRestitutionVelocity() const { return m_maxRestitutionVelocity; }
        float GetStableSupportThreshold() const { return m_stableSupportThreshold; }
        float GetEdgeSlideBoost() const { return m_edgeSlideBoost; }
        float GetUnstableVerticalDamping() const { return m_unstableVerticalDamping; }
        float GetBaumgarteFactor() const { return m_baumgarteFactor; }
        float GetMaxPenetrationCorrection() const { return m_maxPenetrationCorrection; }
        float GetMinLinearSpeed() const { return m_minLinearSpeed; }
        float GetMinVerticalSpeed() const { return m_minVerticalSpeed; }
        float GetMinTangentialSpeed() const { return m_minTangentialSpeed; }

        void OnUpdate(Core::UpdateContext& ctx) override;

    private:
        const std::vector<Entities::PhysicalEntity*>* m_bodies;
        float m_penetrationTolerance;
        float m_restitution;
        float m_staticFriction;
        float m_dynamicFriction;
        float m_maxSlopeCos;
        float m_verticalSnapCos;
        float m_groundMinCos;
        float m_maxRestitutionVelocity;
        float m_stableSupportThreshold;
        float m_edgeSlideBoost;
        float m_unstableVerticalDamping;
        float m_baumgarteFactor;
        float m_maxPenetrationCorrection;
        float m_minLinearSpeed;
        float m_minVerticalSpeed;
        float m_minTangentialSpeed;
        bool m_includeKinematicBodies;
        bool m_allowKinematicPlayerCollision;
        bool m_loggedMissingBodyList;
        bool m_loggedBodySnapshot;
        bool m_loggedPlayerPresence;
        bool m_loggedPlayerAbsence;
        bool m_loggedPlayerPlatformContact;
        bool m_loggedPlayerSweetsContact;
        bool m_loggedKinematicPlayerFilter;
        bool m_loggedKinematicFilter;
        bool m_loggedSuppressedSweetsFloor;
        bool m_loggedPlanarZSeparation;
        bool m_loggedPlanarZIgnore;
        std::unordered_set<const Entities::PhysicalEntity*> m_contactedBodies;
    };
}
