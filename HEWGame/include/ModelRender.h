#pragma once

#include "ModelDX.h"
#include "ICamera.h"


class ModelRender
{
public:
	ModelRender();
	~ModelRender();


void Draw(ModelDX&, CameraSpace::ICamera&);


private:
	void MeshDraw(MeshDX&);
};
