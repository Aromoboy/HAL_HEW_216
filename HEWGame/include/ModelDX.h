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


// �^��`
using NodeIndexDX = int;	// �{�[��(�K�w)�ԍ�
using ChildrenDX = std::vector<NodeIndexDX>;

struct BoneDX
{
    NodeIndexDX			nodeIndex;	// �ό`���͊K�w���Ɋi�[
    DirectX::XMFLOAT4X4	invOffset;	// �t�s��
};



// �{�[�����
struct NodeDX
{
    std::string			name;		// �{�[����
    NodeIndexDX			parent;		// �e�{�[��
    ChildrenDX			children;	// �q�{�[��
    DirectX::XMMATRIX	mat;		// �ϊ��s��
};



constexpr float kNodeNone = -1.0f;




class ModelDX
{
public:
        ModelDX(std::vector<std::shared_ptr<MeshDX>> a_meshes, std::vector<std::shared_ptr<MaterialDX>> a_MaterialDXs, const std::vector<BoneDX>& a_bones, std::optional<AABB> localBounds = std::nullopt);
        ~ModelDX();

        unsigned int GetMeshCount() const;
        unsigned int GetBoneCount() const;

        DirectX::XMMATRIX GetBoneMatrix(unsigned int index);
        NodeIndexDX GetBoneNodeIndex(unsigned int index) const;
        std::weak_ptr<MeshDX> GetMesh(int a_meshIndex) const;
        std::weak_ptr<MaterialDX> GetMaterial(int a_materialIndex) const;

        bool HasLocalBounds() const;
        const AABB& GetLocalBounds() const;
        void SetLocalBounds(const AABB& bounds);
        const std::string& GetPath() const noexcept { return m_sourcePath; }

private:

        std::vector<std::shared_ptr<MeshDX>> m_meshes;
        std::vector<std::shared_ptr<MaterialDX>> m_materials;

        // �e���b�V����Bone��񂾂��{���ɂ����Ŏ��ׂ����͉��Ƃ������Ȃ��A���Ԃ��Ȃ����߈�x�����ɏ���
        std::vector<BoneDX> m_bones;

        unsigned int m_meshCount;

        AABB m_localBounds{};
        bool m_hasLocalBounds{ false };
        std::string m_sourcePath{};
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
