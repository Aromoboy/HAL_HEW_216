#pragma once

#include "Core/UpdateNode.h"

#include <vector>
#include <unordered_map>

#include "Entities/PhysicalEntity.h"

namespace Systems
{
    struct BodyActivityInfo
    {
        bool hadContact = false;
        bool impulseApplied = false;
        bool hasZOverlap = false;
        bool isActive = false;
    };

    class ContactLiteSystem : public Core::UpdateNode
    {
    public:
        ContactLiteSystem();
        ~ContactLiteSystem() override = default;

        void SetBodySource(const std::vector<Entities::PhysicalEntity*>* bodies);
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
        void SetAllowKinematicPlayerCollision(bool allow);

        const std::vector<Entities::PhysicalEntity*>* GetBodySource() const { return m_bodies; }
        bool IsIncludingKinematicBodies() const { return m_includeKinematicBodies; }
        bool IsAllowingKinematicPlayerCollision() const { return m_allowKinematicPlayerCollision; }

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

        bool HadAnyContactThisFrame(const Entities::CollisionEntity* body) const;
        bool HadImpulseAppliedThisFrame(const Entities::CollisionEntity* body) const;
        bool HasZOverlapWithAnyCollider(const Entities::CollisionEntity* body) const;
        bool IsBodyActive(const Entities::CollisionEntity* body) const;

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
        std::unordered_map<const Entities::CollisionEntity*, BodyActivityInfo> m_bodyActivityInfo;
        struct RestingState
        {
                int frames = 0;
                bool resting = false;
        };

        std::unordered_map<Entities::PhysicalEntity*, RestingState> m_restingFrames;
        int m_restingFrameThreshold;
        float m_restingSpeedThreshold;
        float m_wakeSpeedThreshold;
        bool m_includeKinematicBodies;
        bool m_allowKinematicPlayerCollision;
    };
}
