
#include "ModelDX.h"


ModelDX::ModelDX(std::vector<std::shared_ptr<MeshDX>> a_meshes, std::vector<std::shared_ptr<MaterialDX>> a_materials)
	: m_meshes(a_meshes)
	, m_materials(a_materials)
{
	if (a_meshIndex < 0)
	{
		return std::weak_ptr<MeshDX>();
	}

	const size_t index = static_cast<size_t>(a_meshIndex);
	if (index >= m_meshes.size())
	{
		return std::weak_ptr<MeshDX>();
	}

	return m_meshes[index];
	if (a_materialIndex < 0)
	{
		return std::weak_ptr<MaterialDX>();
	}

	const size_t index = static_cast<size_t>(a_materialIndex);
	if (index >= m_materials.size())
	{
		return std::weak_ptr<MaterialDX>();
	}

	return m_materials[index];
	m_meshCount = static_cast<unsigned int>(meshCount);
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
