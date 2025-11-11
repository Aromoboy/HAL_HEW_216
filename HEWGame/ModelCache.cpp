#include "ModelCache.h"
#include <iostream>

ModelCache& ModelCache::Instance()
{
	static ModelCache instance;
	return instance;
}

std::shared_ptr<ModelDX> ModelCache::Get(const std::string& name)
{
	auto it = m_models.find(name);
	if (it != m_models.end())
	{
		std::cout << "[Cache} Found: " << name << std::endl;
		return it->second;
	}

	std::cout << "[Cache] Not found, creating new model: " << name << std::endl;

	std::vector<std::shared_ptr<MeshDX>> meshes;
	std::vector<std::shared_ptr<MaterialDX>> materials;
	auto model = std::make_shared<ModelDX>(meshes, materials);

	m_models[name] = model;
	return model;
}

void ModelCache::add(const std::string& name, std::shared_ptr<ModelDX> model)
{
	m_models[name] = model;
}

void ModelCache::Clear()
{
	m_models.clear();
}