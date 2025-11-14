#ifndef	__MODELCACHE_H__
#define __MODELCACHE_H__

#include <map>
#include <memory>
#include <string>
#include "ModelDX.h"      
#include "IModelLoader.h" 

namespace Render
{
    class ModelCache
    {
    public:
        static ModelCache& Instance();

        std::shared_ptr<ModelDX> Get(const std::string& path);
        void SetLoader(std::shared_ptr<IModelLoader> loader);
        void Clear();

    private:
        ModelCache() = default;
        ~ModelCache() = default;
        ModelCache(const ModelCache&) = delete;
        ModelCache& operator=(const ModelCache&) = delete;

    private:
        std::shared_ptr<IModelLoader> m_loader;
        std::map<std::string, std::shared_ptr<ModelDX>> m_models;
    };
}

#endif //__MODELCACHE_H__