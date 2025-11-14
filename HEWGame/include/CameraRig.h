#pragma once
#include "Vector.h"
#include "Core/UpdateNode.h"

class CameraRig : Core::UpdateNode
{
public:


	float3 offset;

	float yaw;
	float pitch;



private:
	void OnUpdate(/*Update*/);
	void Attach(/*Entity‚Ìƒ|ƒCƒ“ƒ^*/);

};