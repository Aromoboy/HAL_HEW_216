
#include "ModelDX.h"


ModelDX::ModelDX(std::vector<std::shared_ptr<MeshDX>> a_meshes, std::vector<std::shared_ptr<MaterialDX>> a_materials)
	: m_meshes(a_meshes)
	, m_materials(a_materials)
{
	// ‚Å‚©‚·‚¬‚é‚Ì‚Åunsigned int‚ÉØ‚è‹l‚ß‚Ä•Û
	size_t meshCount = m_meshes.size();
	m_meshCount = static_cast<unsigned int>(meshCount);
}

ModelDX::~ModelDX()
{	
}

//	ŠeíƒQƒbƒ^
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
