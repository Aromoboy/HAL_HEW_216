#include "SceneGameContactHandler.h"

#include "Systems/Physics/IPhysicsBinder.h"
#include "SweetsManager.h"

void SceneGameContactHandler::Bind(Systems::IPhysicsBinder& binder, SweetsSpace::SweetsManager* sweetsManager)
{
    m_sweetsManager = sweetsManager;
    binder.AddContactHandler(
        [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b)
        {
            if (m_sweetsManager)
            {
                m_sweetsManager->OnContactBegin(&a, &b);
            }
        },
        [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b)
        {
            if (m_sweetsManager)
            {
                m_sweetsManager->OnContactEnd(&a, &b);
            }
        },
        [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b, float normalImpulse, float tangentImpulse)
        {
            if (m_sweetsManager)
            {
                m_sweetsManager->OnContactHit(&a, &b, normalImpulse, tangentImpulse);
            }
        });
}
