#pragma once
#include "BaseCamera.h"
#include "Core/UpdateNode.h"
class CameraDebug : public CameraSpace::BaseCamera, public Core::UpdateNode
{
public:

CameraDebug(float3 a_fPos, float a_fFovy);

void OnUpdate(Core::UpdateContext& ctx) override;

private:
bool bIsMove;
};

