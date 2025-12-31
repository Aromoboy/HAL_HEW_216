// シーンが使うサービスへの窓口を定義する
// シーン内の各要素がこれを参照する

#pragma once

#include "ModelRender.h"

namespace Core
{
    class Input;
    class StepTimer;
    class UpdateScheduler;
}

namespace Systems
{
    class IPhysicsBinder;
}

namespace SceneSpace
{
    class ISceneServices
    {
    public:
        virtual ~ISceneServices() = default;

        virtual Systems::IPhysicsBinder* GetPhysicsBinder() = 0;
        virtual Render::ModelRenderer* GetRenderer() = 0;
        virtual Core::UpdateScheduler& GetScheduler() = 0;
        virtual Core::Input* GetInput() = 0;
        virtual Core::StepTimer* GetTimer() = 0;
    };
}
