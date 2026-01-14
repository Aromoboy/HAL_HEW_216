#pragma once

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <vector>

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
        void OnUpdate(float dt);
        void ConfigureCreamAdhesion(const SceneGameConfig::CreamAdhesionSettings& settings);

        bool HasSupportingContact(const ActiveSweets& sweets, bool* foundationOnly = nullptr) const;
        bool HasEverCollidedWithSweets(const ActiveSweets& sweets) const;

        void ClearContactState(ActiveSweets* sweets);
        void ClearSweetsContact(ActiveSweets* sweets);
        void ClearPendingAdhesionsForSweets(ActiveSweets* sweets);
        void ClearPendingAdhesionsForEntity(Entities::PhysicalEntity* entity);
        void RequestCreamWeldBreak(ActiveSweets* cream, Entities::PhysicalEntity* other, const char* context);
        void Reset();

        void RemoveCreamWeldJointsForSweets(ActiveSweets* sweets, const char* context);
        void ClearCreamWeldJoints(const char* context);
        void RemoveCementClusterForSweets(ActiveSweets* sweets, const char* context);
        void ClearCementClusters(const char* context);

        void OnStateChanged(ActiveSweets* sweets, SweetsState previousState, SweetsState newState);

    private:
        struct ContactState
        {
            int foundationContacts{ 0 };
            int stageContacts{ 0 };
            int sweetsContacts{ 0 };
        };

        struct CementCluster
        {
            std::unordered_set<ActiveSweets*> sweets;
            std::unordered_set<Entities::PhysicalEntity*> members;
            std::vector<b2JointId> joints;
            Entities::PhysicalEntity* anchor{ nullptr };
            bool anchoredToCake{ false };
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
            bool awaitingRestore{ false };
            float graceSecondsRemaining{ 0.0f };
        };
        struct PendingBreak
        {
            ActiveSweets* cream{ nullptr };
            Entities::PhysicalEntity* other{ nullptr };
            float elapsedSeconds{ 0.0f };
            bool stillSeparated{ false };
        };

        void UpdateContactStateBegin(ActiveSweets* sweets, Entities::PhysicalEntity* other);
        void UpdateContactStateEnd(ActiveSweets* sweets, Entities::PhysicalEntity* other);
        bool IsAdhesionTargetAllowed(const ActiveSweets& cream, const Entities::PhysicalEntity& other) const;
        void TryCreateCreamWeldJoint(ActiveSweets* cream, Entities::PhysicalEntity* other);
        void BreakCreamWeldJoint(ActiveSweets* cream, Entities::PhysicalEntity* other, const char* context);
        void ClearPendingBreaksForSweets(ActiveSweets* sweets);
        void CleanupInvalidContacts();
        const ContactState* FindContactState(const ActiveSweets& sweets) const;
        bool IsCementedOrLocked(const Entities::PhysicalEntity& entity) const;
        void UpdateCementAdjacencyBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void UpdateCementAdjacencyEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void RemoveCementAdjacencyForEntity(Entities::PhysicalEntity* entity);
        std::shared_ptr<CementCluster> BuildCementCluster(ActiveSweets* cream, Entities::PhysicalEntity* other);
        void RemoveCementCluster(const std::shared_ptr<CementCluster>& cluster, const char* context);
        void CreateCementClusterJoints(CementCluster& cluster, const char* context);
        bool IsCementClusterCandidate(const Entities::PhysicalEntity& entity) const;
        bool IsCakeEntity(const Entities::PhysicalEntity& entity) const;

        Systems::PhysicsMachine* m_physicsMachine;
        bool* m_warnedMissingPhysicsMachine;
        bool m_loggedContactNotification{ false };
        float m_creamAdhesionDelaySeconds{ 0.3f };
        float m_creamAdhesionGraceSeconds{ 0.08f };
        bool m_creamRequireSupport{ true };
        bool m_creamRequireCarried{ true };
        bool m_creamAllowAirAdhesionToSweets{ false };
        float m_creamWeldLinearHertz{ 6.0f };
        float m_creamWeldAngularHertz{ 6.0f };
        float m_creamWeldLinearDampingRatio{ 0.7f };
        float m_creamWeldAngularDampingRatio{ 0.7f };
        float m_creamWeldBreakImpulseThreshold{ 0.0f };
        std::unordered_set<std::string> m_creamAllowedTags;
        std::unordered_set<std::string> m_creamBlockedTags;

        std::unordered_map<ActiveSweets*, ContactState> m_contactStates;
        std::unordered_set<ActiveSweets*> m_hasSweetsContact;
        std::unordered_map<JointPairKey, b2JointId, JointPairHash> m_creamWeldJoints;
        std::unordered_map<Entities::PhysicalEntity*, std::unordered_set<Entities::PhysicalEntity*>> m_cementAdjacency;
        std::unordered_set<std::shared_ptr<CementCluster>> m_cementClusters;
        std::unordered_map<ActiveSweets*, std::shared_ptr<CementCluster>> m_cementClusterLookup;
        // NOTE: Pending maps store non-owning pointers; clear them whenever a sweets/target entity is deleted.
        std::unordered_map<JointPairKey, PendingAdhesion, JointPairHash> m_pendingAdhesions;
        std::unordered_map<JointPairKey, PendingBreak, JointPairHash> m_pendingBreaks;
    };
}
