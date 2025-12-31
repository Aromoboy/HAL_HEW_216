#pragma once
#include <memory>
#include <string>

#include "Render/Model.h"
#include "Animation.h"

struct ID3D11Device;
class TextureCache;


class IModelLoader
{
public:
IModelLoader() {};
virtual ~IModelLoader() = default;

        virtual std::shared_ptr<Render::Model> Load(const std::string& path, ID3D11Device* device, TextureCache& textureCache) = 0;
        virtual std::shared_ptr<Animation> LoadAnimation(const std::string& path) = 0;
};
