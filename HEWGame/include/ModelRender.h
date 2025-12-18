#pragma once

#include "ModelDX.h"
#include "ICamera.h"
#include "Transform.h"
#include "Animation.h"

class ModelRender
{
public:
ModelRender();
~ModelRender();

void Draw(Render::Model& model, Transform transform, const CameraSpace::ICamera* camera);

void Draw(Render::Model& model, Animation animation, Transform transform, const CameraSpace::ICamera* camera);

private:
void MeshDraw(MeshDX&);
};

namespace Render
{
    using ModelRenderer = ::ModelRender;
}
