#pragma once
#include "ICamera.h"
class CameraDebug : public ICamera
{
public:
	
	CameraDebug(float3 a_fPos, float a_fFovy) :
		bIsMove(true),
		ICamera(a_fPos, a_fFovy)
	{}

	void Update() override;


private:
	bool bIsMove;
};

