#pragma once

#include "ModelDX.h"
#include "ICamera.h"
#include "Transform.h"

class Animation;

class ModelRender
{
public:
ModelRender();
~ModelRender();

void Draw(Render::Model& model, Transform transform, const CameraSpace::ICamera* camera, Animation* animation);

private:
void MeshDraw(MeshDX&);
};

namespace Render
{
    using ModelRenderer = ::ModelRender;
}
