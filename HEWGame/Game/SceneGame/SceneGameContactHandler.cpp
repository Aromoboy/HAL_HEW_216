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

            if (!m_playerEntity || m_playerHit)
            {
                return;
            }

            const bool playerIsA = (&a == m_playerEntity);
            const bool playerIsB = (&b == m_playerEntity);
            if (!playerIsA && !playerIsB)
            {
                return;
            }

            const Entities::PhysicalEntity& other = playerIsA ? b : a;
            if (other.GetCollisionLayer() == Collision::CollisionLayer::Meteors)
            {
                m_playerHit = true;
            }
        },
        [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b, const Systems::ContactEventInfo& info)
        {
            if (m_meteorsManager)
            {
                m_meteorsManager->OnContactEnd(&a, &b, info);
            }
        });
}

bool SceneGameContactHandler::ConsumePlayerHit()
{
    if (!m_playerHit)
    {
        return false;
    }
    m_playerHit = false;
    return true;
}

void SceneGameContactHandler::Reset()
{
    m_playerHit = false;
}
