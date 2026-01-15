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

        void ClearContactState(ActiveSweets* sweets);
        void ClearSweetsContact(ActiveSweets* sweets);
        void Reset();

        void RemoveCreamWeldJointsForSweets(ActiveSweets* sweets, const char* context);
        void ClearCreamWeldJoints(const char* context);

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
        };

        struct ContactAnchorData
        {
            b2Vec2 point{};
            b2Vec2 normal{};
            float separation{ 0.0f };
            b2ShapeId shapeIdA{ b2_nullShapeId };
            b2ShapeId shapeIdB{ b2_nullShapeId };
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

        struct JointPairKey
        {
            ActiveSweets* first{ nullptr };
            ActiveSweets* second{ nullptr };

            static JointPairKey Make(ActiveSweets* a, ActiveSweets* b)
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
                std::size_t h1 = std::hash<ActiveSweets*>{}(key.first);
                std::size_t h2 = std::hash<ActiveSweets*>{}(key.second);
                return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
            }
        };

        void UpdateContactStateBegin(ActiveSweets* sweets, Entities::PhysicalEntity* other);
        void UpdateContactStateEnd(ActiveSweets* sweets, Entities::PhysicalEntity* other);
        bool TryGetContactAnchorData(const Systems::ContactEventInfo& info, ContactAnchorData& outAnchor) const;
        bool ApplyGluePushOut(Entities::PhysicalEntity& movingBody, bool movingIsShapeA,
            const ContactAnchorData& anchorData, b2Vec2& outAnchor) const;
        bool DoesShapeBelongToEntity(const Entities::PhysicalEntity& entity, b2ShapeId shapeId) const;
        void FreezeFruitBody(ActiveSweets& fruit);
        void UnfreezeFruitBody(ActiveSweets& fruit);
        bool IsGlueSpeedTooFast(const Entities::PhysicalEntity& entity) const;
        bool IsGlueBreakForceExceeded(b2JointId jointId) const;
        void UpdateBreakableJoints();
        void LogGlueReject(const char* reason, const ActiveSweets* cream, const Entities::PhysicalEntity* target) const;
        void LogGlueDestroy(const char* reason, const ActiveSweets* cream, const Entities::PhysicalEntity* target) const;
        void LogGlueCreateCreamToCake(const ActiveSweets* cream, const Entities::PhysicalEntity* cake, const b2Vec2& anchorWorld) const;
        void LogGlueCreateFruitToCream(const ActiveSweets* cream, const ActiveSweets* fruit, const b2Vec2& anchorWorld) const;
        void CleanupInvalidJoint(const JointPairKey& key, b2JointId& jointId);
        void CleanupInvalidAnchor(ActiveSweets* cream);
        void RemoveFruitCreamLink(ActiveSweets* fruit, ActiveSweets* cream);

        Systems::PhysicsMachine* m_physicsMachine;
        bool* m_warnedMissingPhysicsMachine;
        GlueTuning m_glueTuning{};
        mutable GlueStats m_glueStats{};
        std::unordered_map<ActiveSweets*, ContactState> m_contactStates;
        std::unordered_set<ActiveSweets*> m_hasSweetsContact;
        std::unordered_map<JointPairKey, b2JointId, JointPairHash> m_creamWeldJoints;
        std::unordered_map<JointPairKey, b2Vec2, JointPairHash> m_creamWeldAnchors;
        std::unordered_map<ActiveSweets*, b2JointId> m_creamAnchorJoints;
        std::unordered_map<ActiveSweets*, b2Vec2> m_creamAnchorPoints;
        std::unordered_map<ActiveSweets*, std::unordered_set<ActiveSweets*>> m_fruitToCreams;
        std::unordered_map<ActiveSweets*, FrozenBodyState> m_frozenFruitStates;
    };
}
