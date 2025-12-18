#pragma once
#include <memory>
#include <string>

#include "ModelDX.h"

struct ID3D11Device;
class TextureCache;

// [_[̃C^[tF[X
class IModelLoader
{
public:
IModelLoader() {};
virtual ~IModelLoader() = default;

        virtual std::shared_ptr<Render::Model> Load(const std::string& path, ID3D11Device* device, TextureCache& textureCache) = 0;
};
