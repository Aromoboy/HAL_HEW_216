#include "SceneGameContactHandler.h"

#include "Systems/Physics/IPhysicsBinder.h"
#include "SweetsManager.h"

void SceneGameContactHandler::Bind(Systems::IPhysicsBinder& binder, SweetsSpace::SweetsManager* sweetsManager)
{
    m_sweetsManager = sweetsManager;
    binder.AddContactHandler(
        [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b, const Systems::ContactEventInfo& info)
        {
            if (m_sweetsManager)
            {
                m_sweetsManager->OnContactBegin(&a, &b, info);
            }
        },
        [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b, const Systems::ContactEventInfo& info)
        {
            if (m_sweetsManager)
            {
                m_sweetsManager->OnContactEnd(&a, &b, info);
            }
        });
}
