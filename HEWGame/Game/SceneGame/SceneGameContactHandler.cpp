#include "SceneGameContactHandler.h"

#include "Systems/Physics/IPhysicsBinder.h"
#include "Entities/PhysicalEntity.h"
#include "MeteorsManager.h"
#include "Systems/Physics/CollisionLayer.h"

namespace
{
    std::shared_ptr<MeteorsSpace::ActiveMeteors> FindMeteorsByEntity(
        MeteorsSpace::MeteorsManager* manager,
        Entities::PhysicalEntity* entity)
    {
        if (!manager || !entity)
        {
            return nullptr;
        }

        const MeteorsSpace::MeteorsState states[] = {
            MeteorsSpace::MeteorsState::Active,
            MeteorsSpace::MeteorsState::Stacked,
            MeteorsSpace::MeteorsState::Ground,
            MeteorsSpace::MeteorsState::Carried
        };

        for (MeteorsSpace::MeteorsState state : states)
        {
            auto& list = manager->GetByState(state);
            for (const auto& meteors : list)
            {
                if (meteors && meteors.get() == entity)
                {
                    return meteors;
                }
            }
        }

        return nullptr;
    }
}

void SceneGameContactHandler::Bind(Systems::IPhysicsBinder& binder,
    MeteorsSpace::MeteorsManager* meteorsManager,
    Entities::PhysicalEntity* playerEntity,
    MissileResolver missileResolver)
{
    m_meteorsManager = meteorsManager;
    m_playerEntity = playerEntity;
    m_missileResolver = std::move(missileResolver);
    binder.AddContactHandler(
        [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b, const Systems::ContactEventInfo& info)
        {
            if (m_meteorsManager)
            {
                m_meteorsManager->OnContactBegin(&a, &b, info);
            }

            const bool aIsMissile = (a.GetTag() == "projectile_missile");
            const bool bIsMissile = (b.GetTag() == "projectile_missile");
            if (aIsMissile && b.GetCollisionLayer() == Collision::CollisionLayer::Meteors)
            {
                auto missile = m_missileResolver ? m_missileResolver(&a) : nullptr;
                auto meteors = FindMeteorsByEntity(m_meteorsManager, &b);
                if (missile && meteors)
                {
                    m_missileContact = MissileContactEvent{ missile, meteors };
                }
            }
            else if (bIsMissile && a.GetCollisionLayer() == Collision::CollisionLayer::Meteors)
            {
                auto missile = m_missileResolver ? m_missileResolver(&b) : nullptr;
                auto meteors = FindMeteorsByEntity(m_meteorsManager, &a);
                if (missile && meteors)
                {
                    m_missileContact = MissileContactEvent{ missile, meteors };
                }
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
                auto meteors = FindMeteorsByEntity(m_meteorsManager, other);
                if (meteors)
                {
                    m_playerContact = PlayerContactEvent{ meteors, PlayerContactType::Begin };
                }
                m_playerContact = PlayerContactEvent{ other, PlayerContactType::Begin, PlayerContactSource::Meteor };
            }
            else if (other->GetTag() == "projectile_missile")
            {
                m_playerContact = PlayerContactEvent{ other, PlayerContactType::Begin, PlayerContactSource::Missile };
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
                auto meteors = FindMeteorsByEntity(m_meteorsManager, other);
                if (meteors)
                {
                    m_playerContact = PlayerContactEvent{ meteors, PlayerContactType::End };
                }
                m_playerContact = PlayerContactEvent{ other, PlayerContactType::End, PlayerContactSource::Meteor };
            }
            else if (other->GetTag() == "projectile_missile")
            {
                m_playerContact = PlayerContactEvent{ other, PlayerContactType::End, PlayerContactSource::Missile };
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

std::optional<SceneGameContactHandler::MissileContactEvent> SceneGameContactHandler::ConsumeMissileContact()
{
    if (!m_missileContact)
    {
        return std::nullopt;
    }

    std::optional<MissileContactEvent> contact = m_missileContact;
    m_missileContact = std::nullopt;
    return contact;
}

void SceneGameContactHandler::Reset()
{
    m_playerContact = std::nullopt;
    m_missileContact = std::nullopt;
}
