#pragma once
#include "ICamera.h"
#include "Core/UpdateNode.h"
class CameraDebug : public ICamera, public Core::UpdateNode
{
public:

        CameraDebug(float3 a_fPos, float a_fFovy) :
                ICamera(a_fPos, a_fFovy),
                Core::UpdateNode(Core::UpdatePhase::Camera),
                bIsMove(true)
        {}

        void Update() override;
        void OnUpdate(Core::UpdateContext& ctx) override;

private:
        bool bIsMove;
};

