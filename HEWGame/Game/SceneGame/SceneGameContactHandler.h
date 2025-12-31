// シーンゲームの接触イベントを受け取る
// 物理システムから通知されて処理される

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

class SceneGameContactHandler
{
public:
    void Bind(Systems::IPhysicsBinder& binder, SweetsSpace::SweetsManager* sweetsManager);

private:
    SweetsSpace::SweetsManager* m_sweetsManager = nullptr;
};
