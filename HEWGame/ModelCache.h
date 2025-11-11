#ifndef	__MODELCACHE_H__
#define __MODELCACHE_H__

#include <map>
#include <memory>
#include <string>
#include "ModelDX.h"
#include "MeshDX.h"
#include "MaterialDX.h"

class ModelCache
{
public:
	static ModelCache& Instance();

	std::shared_ptr<ModelDX> Get(const std::string& name);
	void add(const std::string& name, std::shared_ptr<ModelDX> model);
	void Clear();

private:
	ModelCache() = default;
	~ModelCache() = default;
	ModelCache(const ModelCache&) = delete;
	ModelCache& operator=(const ModelCache&) = delete;

private:
	std::map<std::string, std::shared_ptr<ModelDX>> m_models;
};

#endif //__MODELCACHE_H__