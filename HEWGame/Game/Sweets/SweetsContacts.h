#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <box2d/box2d.h>

#include "Sweets/SweetsTypes.h"
#include "Systems/Physics/ContactEventInfo.h"

namespace Entities
{
    class PhysicalEntity;
}

namespace Systems
{
    class PhysicsMachine;
}

namespace SweetsSpace
{
    class SweetsContacts
    {
    public:
        struct GlueDebugSnapshot
        {
            struct GlueLink
            {
                b2Vec2 anchorWorld{};
                ActiveSweets* cream{ nullptr };
                ActiveSweets* fruit{ nullptr };
                bool fruitFrozen{ false };
            };

            std::vector<b2Vec2> creamAnchors;
            std::vector<GlueLink> fruitLinks;
            int activeCreamAnchors{ 0 };
            int activeFruitGlue{ 0 };
            int frozenFruit{ 0 };
            float createPerSec{ 0.0f };
            float rejectPerSec{ 0.0f };
            float cleanupPerSec{ 0.0f };
        };

        SweetsContacts(Systems::PhysicsMachine* physicsMachine, bool* warnedMissingPhysicsMachine);

        void OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b, const Systems::ContactEventInfo& info);
        void OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b, const Systems::ContactEventInfo& info);

        bool HasSupportingContact(const ActiveSweets& sweets, bool* foundationOnly = nullptr) const;
        bool HasEverCollidedWithSweets(const ActiveSweets& sweets) const;
        bool IsAnchoredToCake(const ActiveSweets& sweets) const;

        void ClearContactState(ActiveSweets* sweets);
        void ClearSweetsContact(ActiveSweets* sweets);
        void Reset();

        void RemoveCreamWeldJointsForSweets(ActiveSweets* sweets, const char* context);
        void ClearCreamWeldJoints(const char* context);
        void ProcessPendingGlue();

        void OnStateChanged(ActiveSweets* sweets, SweetsState previousState, SweetsState newState);
        void TickDebugStats(float dt);
        GlueDebugSnapshot BuildDebugSnapshot() const;

    private:
        struct GlueTuning
        {
            float linearHertz{ 0.0f };
            float linearDampingRatio{ 0.7f };
            float angularHertz{ 0.0f };
            float angularDampingRatio{ 0.7f };
            bool collideConnected{ false };
            float pushOutEpsilon{ 0.03f };
            float maxPushOut{ 0.25f };
            float maxSpeed{ 0.0f };
            float breakForce{ 0.0f };
            float breakImpulse{ 0.0f };
        };

        struct ContactAnchorData
        {
            b2Vec2 point{};
            b2Vec2 normal{};
            float separation{ 0.0f };
            b2ShapeId shapeIdA{ b2_nullShapeId };
            b2ShapeId shapeIdB{ b2_nullShapeId };
        };

        struct PendingGlue
        {
            ActiveSweets* cream{ nullptr };
            ActiveSweets* fruit{ nullptr };
            b2ContactId contactId{ b2_nullContactId };
            b2ShapeId shapeIdA{ b2_nullShapeId };
            b2ShapeId shapeIdB{ b2_nullShapeId };
        };

        struct CreamWeldEntry
        {
            ActiveSweets* cream{ nullptr };
            ActiveSweets* fruit{ nullptr };
            b2JointId jointId{ b2_nullJointId };
            b2Vec2 anchorWorld{};
        };

        struct FrozenBodyState
        {
            b2BodyType bodyType{ b2_dynamicBody };
            float gravityScale{ 1.0f };
        };

        struct GlueStats
        {
            int createCount{ 0 };
            int rejectCount{ 0 };
            int cleanupCount{ 0 };
            float elapsedSeconds{ 0.0f };
            float createPerSec{ 0.0f };
            float rejectPerSec{ 0.0f };
            float cleanupPerSec{ 0.0f };
        };

        struct ContactState
        {
            int foundationContacts{ 0 };
            int sweetsContacts{ 0 };
        };

        struct ContactIdHash
        {
            std::size_t operator()(const b2ContactId& id) const noexcept
            {
                const std::size_t h1 = std::hash<int32_t>{}(id.index1);
                const std::size_t h2 = std::hash<uint16_t>{}(id.world0);
                const std::size_t h3 = std::hash<uint32_t>{}(id.generation);
                std::size_t seed = h1;
                seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                return seed;
            }
        };

        struct ContactIdEqual
        {
            bool operator()(const b2ContactId& lhs, const b2ContactId& rhs) const noexcept
            {
                return B2_ID_EQUALS(lhs, rhs);
            }
        };

        void UpdateContactStateBegin(ActiveSweets* sweets, Entities::PhysicalEntity* other);
        void UpdateContactStateEnd(ActiveSweets* sweets, Entities::PhysicalEntity* other);
        bool TryGetContactAnchorData(const Systems::ContactEventInfo& info, ContactAnchorData& outAnchor) const;
        bool TryGetContactAnchorData(const b2ContactId& contactId, b2ShapeId shapeIdA, b2ShapeId shapeIdB,
            ContactAnchorData& outAnchor) const;
        bool ApplyGluePushOut(Entities::PhysicalEntity& movingBody, bool movingIsShapeA,
            const ContactAnchorData& anchorData, b2Vec2& outAnchor) const;
        bool DoesShapeBelongToEntity(const Entities::PhysicalEntity& entity, b2ShapeId shapeId) const;
        Entities::PhysicalEntity* GetEntityForShape(b2ShapeId shapeId) const;
        void FreezeFruitBody(ActiveSweets& fruit);
        void UnfreezeFruitBody(ActiveSweets& fruit);
        bool IsGlueSpeedTooFast(const Entities::PhysicalEntity& entity) const;
        bool IsGlueBreakForceExceeded(b2JointId jointId) const;
        void UpdateBreakableJoints();
        void LogGlueReject(const char* reason, const ActiveSweets* cream, const Entities::PhysicalEntity* target) const;
        void LogGlueDestroy(const char* reason, const ActiveSweets* cream, const Entities::PhysicalEntity* target) const;
        void LogGlueCreateCreamToCake(const ActiveSweets* cream, const Entities::PhysicalEntity* cake, const b2Vec2& anchorWorld) const;
        void LogGlueCreateFruitToCream(const ActiveSweets* cream, const ActiveSweets* fruit, const b2Vec2& anchorWorld) const;
        void CleanupInvalidJoint(const b2ContactId& contactId, CreamWeldEntry& entry);
        void CleanupInvalidAnchor(ActiveSweets* cream);
        void RemoveFruitCreamLink(ActiveSweets* fruit, ActiveSweets* cream);
        void RemovePendingGlueForSweets(ActiveSweets* sweets);
        void TickGlueCooldowns(float dt);
        bool IsGlueCooldownActive(const ActiveSweets& fruit) const;

        Systems::PhysicsMachine* m_physicsMachine;
        bool* m_warnedMissingPhysicsMachine;
        GlueTuning m_glueTuning{};
        mutable GlueStats m_glueStats{};
        std::unordered_map<ActiveSweets*, ContactState> m_contactStates;
        std::unordered_set<ActiveSweets*> m_hasSweetsContact;
        std::unordered_map<b2ContactId, PendingGlue, ContactIdHash, ContactIdEqual> m_pendingGlue;
        std::unordered_map<b2ContactId, CreamWeldEntry, ContactIdHash, ContactIdEqual> m_creamWeldJoints;
        std::unordered_map<ActiveSweets*, b2JointId> m_creamAnchorJoints;
        std::unordered_map<ActiveSweets*, b2Vec2> m_creamAnchorPoints;
        std::unordered_map<ActiveSweets*, std::unordered_set<ActiveSweets*>> m_fruitToCreams;
        std::unordered_map<ActiveSweets*, FrozenBodyState> m_frozenFruitStates;
        std::unordered_map<ActiveSweets*, float> m_glueCooldown;
    };
}
