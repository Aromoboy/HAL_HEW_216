#pragma once

namespace Entities
{
    class PhysicalEntity;
}

namespace Systems
{
    class IPhysicsBinder;
}

namespace SweetsSpace
{
    class SweetsManager;
}

class SceneGameContactForwarder
{
public:
    void Bind(Systems::IPhysicsBinder& binder, SweetsSpace::SweetsManager* sweetsManager);

private:
    SweetsSpace::SweetsManager* m_sweetsManager = nullptr;
};
