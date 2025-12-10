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
        unsigned int GetMaterialCount() const;

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
// ModelDX provides the concrete implementation, so Render::Model is a thin
// wrapper that derives from ModelDX to keep a single, unified model type
// across entities and systems while still allowing forward declarations.
namespace Render
{
    // Lightweight Render-layer wrapper to keep Render::Model as a distinct type
    // while reusing the ModelDX implementation.
    class Model : public ::ModelDX
    {
    public:
        using ::ModelDX::ModelDX;
        ~Model() = default;
    };
}



#endif // MODELDX_H
