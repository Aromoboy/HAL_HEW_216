// 描画基盤向けのモデルデータを保持する
// モデル描画やローダーから参照される

#ifndef MODELDX_H
#define MODELDX_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "AABB.h"
#include "MeshDX.h"
#include "MaterialDX.h"
#include "DirectXMath.h"
#include "DirectX.h"







class ModelDX
{
public:
        ModelDX(std::vector<std::shared_ptr<MeshDX>> a_meshes, std::vector<std::shared_ptr<MaterialDX>> a_MaterialDXs, std::optional<AABB> localBounds = std::nullopt);
        ~ModelDX();

        unsigned int GetMeshCount() const;
        std::weak_ptr<MeshDX> GetMesh(int a_meshIndex) const;
        std::weak_ptr<MaterialDX> GetMaterial(int a_materialIndex) const;

        bool HasLocalBounds() const;
        const AABB& GetLocalBounds() const;
        void SetLocalBounds(const AABB& bounds);
        const std::string& GetPath() const noexcept { return m_sourcePath; }

private:

        std::vector<std::shared_ptr<MeshDX>> m_meshes;
        std::vector<std::shared_ptr<MaterialDX>> m_materials;

     
        unsigned int m_meshCount;

        AABB m_localBounds{};
        bool m_hasLocalBounds{ false };
        std::string m_sourcePath{};
};

#endif // MODELDX_H
