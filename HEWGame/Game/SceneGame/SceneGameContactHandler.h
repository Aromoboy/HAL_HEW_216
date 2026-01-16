#pragma once

namespace Entities
{
    class PhysicalEntity;
}

namespace Systems
{
    class IPhysicsBinder;
}

namespace MeteorsSpace
{
    class MeteorsManager;
}

class SceneGameContactHandler
{
public:
    void Bind(Systems::IPhysicsBinder& binder,
              MeteorsSpace::MeteorsManager* meteorsManager,
              Entities::PhysicalEntity* playerEntity);
    bool ConsumePlayerHit();
    void Reset();

private:
    MeteorsSpace::MeteorsManager* m_meteorsManager = nullptr;
    Entities::PhysicalEntity* m_playerEntity = nullptr;
    bool m_playerHit = false;
};
