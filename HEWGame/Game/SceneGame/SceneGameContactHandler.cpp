#include "SceneGameContactHandler.h"

#include "Systems/Physics/IPhysicsBinder.h"
#include "MeteorsManager.h"

void SceneGameContactHandler::Bind(Systems::IPhysicsBinder& binder, MeteorsSpace::MeteorsManager* meteorsManager)
{
    m_meteorsManager = meteorsManager;
    binder.AddContactHandler(
        [this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b, const Systems::ContactEventInfo& info)
        {
            if (m_meteorsManager)
            {
                m_meteorsManager->OnContactBegin(&a, &b, info);
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
