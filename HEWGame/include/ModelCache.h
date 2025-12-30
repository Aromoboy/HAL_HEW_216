#ifndef	__MODELCACHE_H__
#define __MODELCACHE_H__

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include "Render/Model.h"
#include "IModelLoader.h"

struct ID3D11Device;
class TextureCache;

namespace Render
{
    class ModelCache
    {
    public:
        static ModelCache& Instance();

        std::shared_ptr<Render::Model> Get(const std::string& path);
        void SetLoader(std::shared_ptr<IModelLoader> loader, ID3D11Device* device, TextureCache& textureCache);
        void ClearModels();
        void Clear();
        void Reset();
        void SetTextureLoadingEnabled(bool enabled) noexcept { m_textureLoadingEnabled = enabled; }
        bool IsTextureLoadingEnabled() const noexcept { return m_textureLoadingEnabled; }

    private:
        ModelCache() = default;
        ~ModelCache() = default;
        ModelCache(const ModelCache&) = delete;
        ModelCache& operator=(const ModelCache&) = delete;

    private:
        std::shared_ptr<IModelLoader> m_loader;
        ID3D11Device* m_device = nullptr;
        TextureCache* m_textureCache = nullptr;
        std::map<std::string, std::shared_ptr<Render::Model>> m_models;
        std::unordered_set<std::string> m_inFlightKeys;
        bool m_textureLoadingEnabled = true;
    };
}

#endif //__MODELCACHE_H__
