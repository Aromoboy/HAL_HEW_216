#pragma once

#include "ModelDX.h"
#include "ICamera.h"
#include "Transform.h"

class ModelRender
{
public:
ModelRender();
~ModelRender();

void Draw(ModelDX& model, Transform transform, const CameraSpace::ICamera* camera);

private:
void MeshDraw(MeshDX&);
};
