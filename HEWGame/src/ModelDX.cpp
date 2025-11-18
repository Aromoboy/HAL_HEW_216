#include <utility>
#include "ModelDX.h"


ModelDX::ModelDX(std::vector<std::shared_ptr<MeshDX>> a_meshes, std::vector<std::shared_ptr<MaterialDX>> a_materials)
{
	m_meshes = std::move(a_meshes);
	m_materials = std::move(a_materials);
	m_meshCount = static_cast<unsigned int>(m_meshes.size());
}

ModelDX::~ModelDX()
{	
}

//	各種ゲッタ
unsigned int ModelDX::GetMeshCount()
{
	return m_meshCount;
}

std::weak_ptr<MeshDX> ModelDX::GetMesh(int a_meshIndex)
{
	return m_meshes[a_meshIndex];
}

std::weak_ptr<MaterialDX> ModelDX::GetMaterial(int a_materialIndex)
{
	return m_materials[a_materialIndex];
}
