#include "SceneGameContactHandler.h"

#include "Systems/Physics/IPhysicsBinder.h"
#include "SweetsManager.h"

void SceneGameContactHandler::Bind(Systems::IPhysicsBinder& binder, SweetsSpace::SweetsManager* sweetsManager)
{
    m_sweetsManager = sweetsManager;
    binder.AddContactHandler(
        [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b, const std::optional<b2Vec2>& contactPoint)
        {
            if (m_sweetsManager)
            {
                m_sweetsManager->OnContactBegin(&a, &b, contactPoint);
            }
        },
        [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b)
        {
            if (m_sweetsManager)
            {
                m_sweetsManager->OnContactEnd(&a, &b);
            }
        });
}
