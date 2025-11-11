#include "ModelCache.h"
#include <iostream>

namespace Render
{
    ModelCache& ModelCache::Instance()
    {
        static ModelCache instance;
        return instance;
    }

    void ModelCache::SetLoader(std::shared_ptr<IModelLoader> loader)
    {
        m_loader = loader;
    }

    std::shared_ptr<ModelDX> ModelCache::Get(const std::string& path)
    {
        auto it = m_models.find(path);
        if (it != m_models.end())
        {
            std::cout << "[ModelCache] Found cached model: " << path << std::endl;
            return it->second;
        }

        if (!m_loader)
        {
            std::cerr << "[ModelCache] Error: No loader set!" << std::endl;
            return nullptr;
        }

        std::cout << "[ModelCache] Loading model: " << path << std::endl;
        std::shared_ptr<ModelDX> model = m_loader->Load(path);
        if (model)
        {
            m_models[path] = model;
        }
        else
        {
            std::cerr << "[ModelCache] Failed to load: " << path << std::endl;
        }

        return model;
    }

    void ModelCache::Clear()
    {
        m_models.clear();
    }
}
