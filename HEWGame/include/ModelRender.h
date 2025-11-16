#pragma once

#include "ModelDX.h"
#include "ICamera.h"


class ModelRender
{
public:
ModelRender();
~ModelRender();

void Draw(ModelDX& model, const CameraSpace::ICamera* camera);

private:
void MeshDraw(MeshDX&);
};
