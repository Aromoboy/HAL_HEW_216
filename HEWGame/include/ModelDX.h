#ifndef MODELDX_H
#define MODELDX_H

#include <vector>
#include <memory>

#include "MeshDX.h"
#include "MaterialDX.h"

class ModelDX
{
public:
	ModelDX(std::vector<std::shared_ptr<MeshDX>> a_meshes, std::vector<std::shared_ptr<MaterialDX>> a_MaterialDXs);
	~ModelDX();

	unsigned int GetMeshCount();

	std::weak_ptr<MeshDX> GetMesh(int a_meshIndex);
	std::weak_ptr<MaterialDX> GetMaterial(int a_materialIndex);

private:
	
	std::vector<std::shared_ptr<MeshDX>> m_meshes;
	std::vector<std::shared_ptr<MaterialDX>> m_materials;

	unsigned int m_meshCount;
};



#endif // MODELDX_H
