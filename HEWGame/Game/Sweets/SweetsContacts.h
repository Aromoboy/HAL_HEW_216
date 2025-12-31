// スイーツ同士の接触に関する処理を扱う
// 物理接触の通知から呼ばれる

#pragma once

#include <unordered_map>
#include <unordered_set>

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

namespace SweetsSpace
{
    class SweetsContacts
    {
    public:
        SweetsContacts(Systems::PhysicsMachine* physicsMachine, bool* warnedMissingPhysicsMachine);

        void OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);

        bool HasSupportingContact(const ActiveSweets& sweets, bool* foundationOnly = nullptr) const;
        bool HasEverCollidedWithSweets(const ActiveSweets& sweets) const;

        void ClearContactState(ActiveSweets* sweets);
        void ClearSweetsContact(ActiveSweets* sweets);
        void Reset();

        void RemoveCreamWeldJointsForSweets(ActiveSweets* sweets, const char* context);
        void ClearCreamWeldJoints(const char* context);

        void OnStateChanged(ActiveSweets* sweets, SweetsState previousState, SweetsState newState);

    private:
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

        Systems::PhysicsMachine* m_physicsMachine;
        bool* m_warnedMissingPhysicsMachine;
        bool m_loggedContactNotification{ false };

        std::unordered_map<ActiveSweets*, ContactState> m_contactStates;
        std::unordered_set<ActiveSweets*> m_hasSweetsContact;
        std::unordered_map<JointPairKey, b2JointId, JointPairHash> m_creamWeldJoints;
    };
}
