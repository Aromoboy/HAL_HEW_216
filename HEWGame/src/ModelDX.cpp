// ============================================================================
// Tv: ModelDX ̋֘AWbNAsrc ɑ@\̉ĂB
// ړI: ߂ẴW[ɐGJ҂łAӖƈˑ֌Wfcł悤ɍx̐t^ĂB
// Ag: src z̑t@CVXewƘA邽߁At@CԂ̊֌WǐՂۂ̖ڈƂėpłB
// ============================================================================


#include "ModelDX.h"


ModelDX::ModelDX(std::vector<std::shared_ptr<MeshDX>> a_meshes, std::vector<std::shared_ptr<MaterialDX>> a_materials)
	: m_meshes(a_meshes)
	, m_materials(a_materials)
{
	// でかすぎるのでunsigned intに切り詰めて保持
	size_t meshCount = m_meshes.size();
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
