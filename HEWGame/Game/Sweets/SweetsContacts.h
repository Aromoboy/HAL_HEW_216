#pragma once

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <string_view>
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
    class SweetsRegistry;

    class SweetsContacts
    {
    public:
        SweetsContacts(Systems::PhysicsMachine* physicsMachine,
            SweetsRegistry* registry,
            bool* warnedMissingPhysicsMachine);

        void RegisterSweetsHandle(const std::shared_ptr<ActiveSweets>& sweets);
        void UnregisterSweetsHandle(ActiveSweets* sweets);
        void RegisterAdhesionTarget(const std::shared_ptr<Entities::PhysicalEntity>& entity);
        void UnregisterAdhesionTarget(Entities::PhysicalEntity* entity);
        void ClearAdhesionTargets();

        void OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void OnContactLost(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void OnContactHit(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b, float approachSpeed);
        void OnUpdate(float dt);
        void ConfigureCreamAdhesion(const SceneGameConfig::CreamAdhesionSettings& settings);
        void SetResetting(bool resetting);
        float GetCreamReleaseCooldownSeconds() const noexcept
        {
            return m_creamReleaseCooldownSeconds;
        }

        bool HasSupportingContact(const ActiveSweets& sweets, bool* foundationOnly = nullptr) const;
        bool HasEverCollidedWithSweets(const ActiveSweets& sweets) const;

        void ClearContactState(ActiveSweets* sweets);
        void ClearSweetsContact(ActiveSweets* sweets);
        void ClearPendingAdhesionsForSweets(ActiveSweets* sweets);
        void ClearPendingAdhesionsForEntity(Entities::PhysicalEntity* entity);
        void RequestCreamWeldBreak(ActiveSweets* cream, Entities::PhysicalEntity* other, const char* context);
        void Reset();
        void NotifyUnregisterOrRetry();

        void RemoveCreamWeldJointsForSweets(ActiveSweets* sweets, const char* context);
        void ClearCreamWeldJoints(const char* context);
        void RemoveCementClusterForSweets(ActiveSweets* sweets, const char* context);
        void ClearCementClusters(const char* context);

        void OnStateChanged(ActiveSweets* sweets, SweetsState previousState, SweetsState newState);

        void RemoveCementAdjacencyForEntity(Entities::PhysicalEntity* entity);


    private:
        struct WeakSweetsHash
        {
            std::size_t operator()(const std::weak_ptr<ActiveSweets>& handle) const noexcept
            {
                const auto locked = handle.lock();
                return std::hash<ActiveSweets*>{}(locked.get());
            }
        };

        struct WeakSweetsEqual
        {
            bool operator()(const std::weak_ptr<ActiveSweets>& lhs,
                            const std::weak_ptr<ActiveSweets>& rhs) const noexcept
            {
                return !lhs.owner_before(rhs) && !rhs.owner_before(lhs);
            }
        };

        using WeakSweetsHandle = std::weak_ptr<ActiveSweets>;
        using WeakSweetsSet = std::unordered_set<WeakSweetsHandle, WeakSweetsHash, WeakSweetsEqual>;
        using WeakSweetsMap = std::unordered_map<WeakSweetsHandle, WeakSweetsSet, WeakSweetsHash, WeakSweetsEqual>;

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
        struct PendingCementCluster
        {
            ActiveSweets* cream{ nullptr };
            Entities::PhysicalEntity* other{ nullptr };
            float elapsedSeconds{ 0.0f };
        };

        enum class CreamAdhesionRejectReason
        {
            None,
            ContactLost,
            InvalidEntity,
            TargetBlocked,
            TargetNotAllowed,
            RequireCarried,
            CooldownActive,
            ExistingBond,
            MissingPhysics,
            MissingContactPoint,
            JointCreateFailed
        };

        enum class CreamBondBreakReason
        {
            None,
            ContactHit,
            ContactLost,
            Manual
        };

        struct CreamAdhesionCandidate
        {
            ActiveSweets* cream{ nullptr };
            Entities::PhysicalEntity* target{ nullptr };
            float timeTouching{ 0.0f };
            float graceRemaining{ 0.0f };
            int touchingShapeCount{ 0 };
            CreamAdhesionRejectReason lastRejectReason{ CreamAdhesionRejectReason::None };
        };

        struct CreamAdhesionBond
        {
            ActiveSweets* cream{ nullptr };
            Entities::PhysicalEntity* target{ nullptr };
            b2JointId jointId{};
            float cooldownRemaining{ 0.0f };
            float createdAtSeconds{ 0.0f };
            float lastBreakTimestamp{ 0.0f };
            CreamBondBreakReason lastBreakReason{ CreamBondBreakReason::None };
        };

        struct CreamAdhesionPairKey
        {
            ActiveSweets* cream{ nullptr };
            Entities::PhysicalEntity* target{ nullptr };

            bool operator==(const CreamAdhesionPairKey& other) const noexcept
            {
                return cream == other.cream && target == other.target;
            }
        };

        struct CreamAdhesionPairHash
        {
            std::size_t operator()(const CreamAdhesionPairKey& key) const noexcept
            {
                std::size_t h1 = std::hash<ActiveSweets*>{}(key.cream);
                std::size_t h2 = std::hash<Entities::PhysicalEntity*>{}(key.target);
                return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
            }
        };

        struct CreamAdhesionConfig
        {
            float adhesionDelaySeconds{ 0.3f };
            float adhesionGraceSeconds{ 0.08f };
            float releaseCooldownSeconds{ 0.0f };
            float weldLinearHertz{ 6.0f };
            float weldAngularHertz{ 6.0f };
            float weldLinearDampingRatio{ 0.7f };
            float weldAngularDampingRatio{ 0.7f };
            float breakImpulseThreshold{ 0.0f };
            bool requireSupport{ true };
            bool requireCarried{ true };
            std::unordered_set<std::string> allowedTags;
            std::unordered_set<std::string> blockedTags;
        };

        void UpdateContactStateBegin(ActiveSweets* sweets, Entities::PhysicalEntity* other);
        void UpdateContactStateEnd(ActiveSweets* sweets, Entities::PhysicalEntity* other);
        void UpdateCreamAdhesionFromBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void UpdateCreamAdhesionFromEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void UpdateCreamAdhesionFromLost(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void UpdateCreamAdhesionFromHit(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b, float approachSpeed);
        void UpdateCreamAdhesionState(float dt);
        bool ResolveCreamAdhesionPair(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b,
            ActiveSweets*& cream, Entities::PhysicalEntity*& target) const;
        void LogCreamAdhesionEvent(const char* label, const ActiveSweets* cream, const Entities::PhysicalEntity* target);
        void TickCreamContacts();
        bool IsAdhesionTargetAllowed(const ActiveSweets& cream, const Entities::PhysicalEntity& other) const;
        bool TryCreateCreamWeldJoint(ActiveSweets* cream, Entities::PhysicalEntity* other, float touchingSeconds);
        void BreakCreamWeldJoint(ActiveSweets* cream, Entities::PhysicalEntity* other, const char* context,
            CreamBondBreakReason reason);
        void ClearPendingBreaksForSweets(ActiveSweets* sweets);
        void ClearHitImpulsesForSweets(ActiveSweets* sweets);
        void ClearPendingCementClustersForEntity(Entities::PhysicalEntity* entity);
        void CleanupInvalidContacts();
        void PruneCementGraph();
        const ContactState* FindContactState(const ActiveSweets& sweets) const;
        bool IsCementedOrLocked(const Entities::PhysicalEntity& entity) const;
        bool ShouldLogCreamPairEvent(ActiveSweets* cream, Entities::PhysicalEntity* other, std::string_view eventKey);
        bool UpdateCementAdjacencyBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void UpdateCementAdjacencyEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        std::shared_ptr<CementCluster> BuildCementCluster(ActiveSweets* cream, Entities::PhysicalEntity* other);
        void RequestCementClusterRebuild(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void ProcessPendingCementClusterRebuilds();
        void DeferCementClusterRebuild();
        void RemoveCementCluster(const std::shared_ptr<CementCluster>& cluster, const char* context);
        void CreateCementClusterJoints(CementCluster& cluster, const char* context);
        bool IsCementClusterCandidate(const Entities::PhysicalEntity& entity) const;
        bool IsCakeEntity(const Entities::PhysicalEntity& entity) const;
        WeakSweetsHandle FindSweetsHandle(ActiveSweets* sweets) const;
        void RemoveCementAdjacencyNode(const WeakSweetsHandle& handle);
        void RemoveCakeAdjacencyNode(const WeakSweetsHandle& handle);
        bool IsCreamCarried(const ActiveSweets& cream) const;
        bool TryGetContactPoint(const Entities::PhysicalEntity& a, const Entities::PhysicalEntity& b,
            b2Vec2& outPoint, float* outImpulse) const;

        Systems::PhysicsMachine* m_physicsMachine;
        SweetsRegistry* m_registry;
        bool* m_warnedMissingPhysicsMachine;
        bool m_loggedContactNotification{ false };
        std::size_t m_invalidContactEvents{ 0 };
        float m_creamAdhesionDelaySeconds{ 0.3f };
        float m_creamAdhesionGraceSeconds{ 0.08f };
        float m_creamBreakImpulseThreshold{ 0.0f };
        float m_creamReleaseCooldownSeconds{ 0.0f };
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
        std::unordered_set<std::size_t> m_loggedCreamPairEvents;
        bool m_isResetting{ false };
        CreamAdhesionConfig m_creamAdhesionConfig{};
        std::unordered_map<CreamAdhesionPairKey, CreamAdhesionCandidate, CreamAdhesionPairHash> m_creamCandidates;
        std::unordered_map<CreamAdhesionPairKey, CreamAdhesionBond, CreamAdhesionPairHash> m_creamBonds;
        float m_creamAdhesionClockSeconds{ 0.0f };
        std::size_t m_creamAdhesionEventLogCount{ 0 };

        std::unordered_map<ActiveSweets*, ContactState> m_contactStates;
        std::unordered_set<ActiveSweets*> m_hasSweetsContact;
        std::unordered_map<JointPairKey, b2JointId, JointPairHash> m_creamWeldJoints;
        std::unordered_map<JointPairKey, float, JointPairHash> m_contactHitImpulses;
        // NOTE: Optional cache for cement contacts; cluster builds use adjacency graph data.
        std::unordered_map<ActiveSweets*, WeakSweetsHandle> m_sweetsHandleLookup;
        std::unordered_map<Entities::PhysicalEntity*, std::weak_ptr<Entities::PhysicalEntity>> m_adhesionTargetLookup;
        WeakSweetsMap m_cementAdjacency;
        std::unordered_map<std::string, WeakSweetsSet> m_cementCakeAdjacency;
        Entities::PhysicalEntity* m_cementCakeEntity{ nullptr };
        std::unordered_set<std::shared_ptr<CementCluster>> m_cementClusters;
        std::unordered_map<ActiveSweets*, std::shared_ptr<CementCluster>> m_cementClusterLookup;
        // NOTE: Pending maps store non-owning pointers; clear them whenever a sweets/target entity is deleted.
        std::unordered_map<JointPairKey, PendingAdhesion, JointPairHash> m_pendingAdhesions;
        std::unordered_map<JointPairKey, PendingBreak, JointPairHash> m_pendingBreaks;
        std::unordered_map<JointPairKey, PendingCementCluster, JointPairHash> m_pendingCementClusters;
        std::unordered_set<JointPairKey, JointPairHash> m_pendingCementClusterRebuilds;
        int m_cementClusterRebuildDelayFrames{ 0 };
        bool m_deferCementClusterRebuild{ false };
    };
}
