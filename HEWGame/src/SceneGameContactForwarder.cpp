#include "SceneGameContactForwarder.h"

#include "Systems/IPhysicsBinder.h"
#include "SweetsManager.h"

void SceneGameContactForwarder::Bind(Systems::IPhysicsBinder& binder, SweetsSpace::SweetsManager* sweetsManager)
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
        });
}
