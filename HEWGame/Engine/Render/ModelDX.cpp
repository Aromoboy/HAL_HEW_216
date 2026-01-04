#include <utility>

#include "ModelDX.h"


// �^��`

ModelDX::ModelDX(std::vector<std::shared_ptr<MeshDX>> a_meshes, std::vector<std::shared_ptr<MaterialDX>> a_materials, std::optional<AABB> localBounds)
{
        m_meshes = std::move(a_meshes);
        m_materials = std::move(a_materials);
        m_meshCount = static_cast<unsigned int>(m_meshes.size());
       
        if (localBounds)
        {
                m_localBounds = *localBounds;
                m_hasLocalBounds = true;
        }
}

ModelDX::~ModelDX()
{	
}

//	各種ゲッタ
unsigned int ModelDX::GetMeshCount() const
{
	return m_meshCount;
}

std::weak_ptr<MeshDX> ModelDX::GetMesh(int a_meshIndex) const
{
        return m_meshes[a_meshIndex];
}

std::weak_ptr<MaterialDX> ModelDX::GetMaterial(int a_materialIndex) const
{
        return m_materials[a_materialIndex];
}

void ModelDX::SetMaterialKey(const std::string& key)
{
        const MaterialTuningSettings& tuningSettings = GetDefaultMaterialTuningSettings();
        for (const auto& material : m_materials)
        {
                if (material)
                {
                        material->SetMaterialKey(key);
                        ApplyMaterialTuning(*material, key, tuningSettings);
                }
        }
}

bool ModelDX::HasLocalBounds() const
{
        return m_hasLocalBounds;
}

const AABB& ModelDX::GetLocalBounds() const
{
        return m_localBounds;
}

void ModelDX::SetLocalBounds(const AABB& bounds)
{
        m_localBounds = bounds;
        m_hasLocalBounds = true;
}
