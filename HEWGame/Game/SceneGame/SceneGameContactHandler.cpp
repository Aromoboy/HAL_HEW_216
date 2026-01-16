#include "SceneGameContactHandler.h"

#include "Systems/Physics/IPhysicsBinder.h"
#include "Entities/PhysicalEntity.h"
#include "MeteorsManager.h"
#include "Systems/Physics/CollisionLayer.h"

void SceneGameContactHandler::Bind(Systems::IPhysicsBinder& binder,
    MeteorsSpace::MeteorsManager* meteorsManager,
    Entities::PhysicalEntity* playerEntity)
{
    m_meteorsManager = meteorsManager;
    m_playerEntity = playerEntity;
    binder.AddContactHandler(
        [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b, const Systems::ContactEventInfo& info)
        {
            if (m_meteorsManager)
            {
                m_meteorsManager->OnContactBegin(&a, &b, info);
            }

            if (!m_playerEntity)
            {
                return;
            }

            const bool playerIsA = (&a == m_playerEntity);
            const bool playerIsB = (&b == m_playerEntity);
            if (!playerIsA && !playerIsB)
            {
                return;
            }

            Entities::PhysicalEntity* other = playerIsA ? &b : &a;
            if (other->GetCollisionLayer() == Collision::CollisionLayer::Meteors)
            {
                m_playerContact = PlayerContactEvent{ other, PlayerContactType::Begin };
            }
        },
        [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b, const Systems::ContactEventInfo& info)
        {
            if (m_meteorsManager)
            {
                m_meteorsManager->OnContactEnd(&a, &b, info);
            }

            if (!m_playerEntity)
            {
                return;
            }

            const bool playerIsA = (&a == m_playerEntity);
            const bool playerIsB = (&b == m_playerEntity);
            if (!playerIsA && !playerIsB)
            {
                return;
            }

            Entities::PhysicalEntity* other = playerIsA ? &b : &a;
            if (other->GetCollisionLayer() == Collision::CollisionLayer::Meteors)
            {
                m_playerContact = PlayerContactEvent{ other, PlayerContactType::End };
            }
        });
}

std::optional<SceneGameContactHandler::PlayerContactEvent> SceneGameContactHandler::ConsumePlayerContact()
{
    if (!m_playerContact)
    {
        return std::nullopt;
    }

    std::optional<PlayerContactEvent> contact = m_playerContact;
    m_playerContact = std::nullopt;
    return contact;
}

void SceneGameContactHandler::Reset()
{
    m_playerContact = std::nullopt;
}
