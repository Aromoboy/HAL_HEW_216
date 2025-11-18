#pragma once

#include "Core/UpdateNode.h"

#include <vector>

#include "PhysicalEntity.h"

namespace Systems
{
    class ContactLiteSystem : public Core::UpdateNode
    {
    public:
        ContactLiteSystem();
        ~ContactLiteSystem() override = default;

        void SetBodySource(const std::vector<SceneSpace::PhysicalEntity*>* bodies);
        void SetPenetrationTolerance(float tolerance);
        void SetRestitution(float restitution);

        float GetPenetrationTolerance() const { return m_penetrationTolerance; }
        float GetRestitution() const { return m_restitution; }

        void OnUpdate(Core::UpdateContext& ctx) override;

    private:
        const std::vector<SceneSpace::PhysicalEntity*>* m_bodies;
        float m_penetrationTolerance;
        float m_restitution;
        bool m_loggedMissingBodyList;
        bool m_loggedPlayerPresence;
        bool m_loggedPlayerAbsence;
        bool m_loggedPlayerPlatformContact;
        bool m_loggedPlayerSweetsContact;
    };
}
