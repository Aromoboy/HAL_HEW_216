#ifndef MODELDX_H
#define MODELDX_H

#include <memory>
#include <optional>
#include <vector>

#include "AABB.h"
#include "MeshDX.h"
#include "MaterialDX.h"

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

private:

        std::vector<std::shared_ptr<MeshDX>> m_meshes;
        std::vector<std::shared_ptr<MaterialDX>> m_materials;

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
