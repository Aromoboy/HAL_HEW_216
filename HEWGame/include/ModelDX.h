#ifndef MODELDX_H
#define MODELDX_H

#include <memory>
#include <optional>
#include <vector>
#include <array>

#include "AABB.h"
#include "MeshDX.h"
#include "MaterialDX.h"
#include "DirectXMath.h"
#include "DirectX.h"



// 型定義
using NodeIndexDX = int;	// ボーン(階層)番号
using ChildrenDX = std::vector<NodeIndexDX>;

struct BoneDX
{
    NodeIndexDX			nodeIndex;	// 変形情報は階層情報に格納
    DirectX::XMFLOAT4X4	invOffset;	// 逆行列
};



// ボーン情報
struct NodeDX
{
    std::string			name;		// ボーン名
    NodeIndexDX			parent;		// 親ボーン
    ChildrenDX			children;	// 子ボーン
    DirectX::XMMATRIX	mat;		// 変換行列
};



constexpr float kNodeNone = -1.0f;




class ModelDX
{
public:
        ModelDX(std::vector<std::shared_ptr<MeshDX>> a_meshes, std::vector<std::shared_ptr<MaterialDX>> a_MaterialDXs, const std::vector<BoneDX>& a_bones, std::optional<AABB> localBounds = std::nullopt);
        ~ModelDX();


        using AnimeNo = int;	// アニメーション番号

        




        unsigned int GetMeshCount() const;

        std::weak_ptr<MeshDX> GetMesh(int a_meshIndex) const;
        std::weak_ptr<MaterialDX> GetMaterial(int a_materialIndex) const;

        bool HasLocalBounds() const;
        const AABB& GetLocalBounds() const;
        void SetLocalBounds(const AABB& bounds);

private:

        std::vector<std::shared_ptr<MeshDX>> m_meshes;
        std::vector<std::shared_ptr<MaterialDX>> m_materials;

        // 各メッシュのBone情報だが本当にここで持つべきかは何とも言えない、時間がないため一度ここに書く
        std::vector<BoneDX> m_bones;

        unsigned int m_meshCount;

        AABB m_localBounds{};
        bool m_hasLocalBounds{ false };
};


// Render-layer model abstraction
// The class diagram groups renderable meshes under Render::Model.
// ModelDX provides the concrete implementation, so we alias it here to
// keep a single, unified model type across entities and systems.
namespace Render
{
    using Model = ::ModelDX;
}



#endif // MODELDX_H
