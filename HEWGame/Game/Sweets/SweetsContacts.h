#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>

#include <box2d/box2d.h>

#include "Sweets/SweetsTypes.h"

namespace Entities
{
    class PhysicalEntity;
}

namespace Systems
{
    class PhysicsMachine;
}

namespace SceneGameConfig
{
    struct CreamAdhesionSettings;
}

namespace SweetsSpace
{
    class SweetsContacts
    {
    public:
        SweetsContacts(Systems::PhysicsMachine* physicsMachine, bool* warnedMissingPhysicsMachine);

        void OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void OnHitContact(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b, float normalImpulse, float tangentImpulse);
        void OnUpdate(float dt);
        void ConfigureCreamAdhesion(const SceneGameConfig::CreamAdhesionSettings& settings);

        bool HasSupportingContact(const ActiveSweets& sweets, bool* foundationOnly = nullptr) const;
        bool HasEverCollidedWithSweets(const ActiveSweets& sweets) const;

        void ClearContactState(ActiveSweets* sweets);
        void ClearSweetsContact(ActiveSweets* sweets);
        void ClearPendingAdhesionsForSweets(ActiveSweets* sweets);
        void Reset();

        void RemoveCreamWeldJointsForSweets(ActiveSweets* sweets, const char* context);
        void ClearCreamWeldJoints(const char* context);

        void OnStateChanged(ActiveSweets* sweets, SweetsState previousState, SweetsState newState);

    private:
        struct ContactState
        {
            int foundationContacts{ 0 };
            int stageContacts{ 0 };
            int sweetsContacts{ 0 };
        };

        struct JointPairKey
        {
            Entities::PhysicalEntity* first{ nullptr };
            Entities::PhysicalEntity* second{ nullptr };

            static JointPairKey Make(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
            {
                JointPairKey key{};
                if (a < b)
                {
                    key.first = a;
                    key.second = b;
                }
                else
                {
                    key.first = b;
                    key.second = a;
                }
                return key;
            }

            bool operator==(const JointPairKey& other) const noexcept
            {
                return first == other.first && second == other.second;
            }
        };

        struct JointPairHash
        {
            std::size_t operator()(const JointPairKey& key) const noexcept
            {
                std::size_t h1 = std::hash<Entities::PhysicalEntity*>{}(key.first);
                std::size_t h2 = std::hash<Entities::PhysicalEntity*>{}(key.second);
                return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
            }
        };

        struct PendingAdhesion
        {
            ActiveSweets* cream{ nullptr };
            Entities::PhysicalEntity* other{ nullptr };
            float elapsedSeconds{ 0.0f };
            bool stillTouching{ false };
        };

        void UpdateContactStateBegin(ActiveSweets* sweets, Entities::PhysicalEntity* other);
        void UpdateContactStateEnd(ActiveSweets* sweets, Entities::PhysicalEntity* other);
        bool IsAdhesionTargetAllowed(const ActiveSweets& cream, const Entities::PhysicalEntity& other) const;
        void TryCreateCreamWeldJoint(ActiveSweets* cream, Entities::PhysicalEntity* other);
        const ContactState* FindContactState(const ActiveSweets& sweets) const;
        bool IsCementedOrLocked(const Entities::PhysicalEntity& entity) const;
        bool HasWeldJointForEntity(const Entities::PhysicalEntity& entity) const;
        void ApplyCementedState(ActiveSweets& sweets) const;
        void RestoreCementedStateIfFree(Entities::PhysicalEntity& entity);

        Systems::PhysicsMachine* m_physicsMachine;
        bool* m_warnedMissingPhysicsMachine;
        bool m_loggedContactNotification{ false };
        float m_creamAdhesionDelaySeconds{ 0.3f };
        float m_creamBreakImpulseThreshold{ 0.0f };
        float m_creamLockedGravityScale{ 0.0f };
        float m_creamLockedLinearDamping{ 0.0f };
        float m_creamLockedAngularDamping{ 0.0f };
        std::unordered_set<std::string> m_creamAllowedTags;
        std::unordered_set<std::string> m_creamBlockedTags;

        std::unordered_map<ActiveSweets*, ContactState> m_contactStates;
        std::unordered_set<ActiveSweets*> m_hasSweetsContact;
        std::unordered_map<JointPairKey, b2JointId, JointPairHash> m_creamWeldJoints;
        std::unordered_map<JointPairKey, PendingAdhesion, JointPairHash> m_pendingAdhesions;
    };
}
