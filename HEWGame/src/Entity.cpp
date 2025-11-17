// Entity.cpp
#include "Entity.h"


#include "ModelCache.h"

namespace SceneSpace
{
    Entity::Entity(const std::string& tag,
        const float3& pos,
        const float3& rot,
        const float3& scl,
        Core::UpdatePhase phase,
        int priority)
        : Core::UpdateNode(phase, priority)
        , m_transform{}
        , m_tag(tag)
    {
        m_transform.position = pos;
        m_transform.rotation = rot;
        m_transform.scale = scl;
    }

    Entity::~Entity() = default;

    void Entity::OnUpdate(Core::UpdateContext& ctx)
    {
        (void)ctx;
    }

    void Entity::Draw(ModelRender render, CameraSpace::ICamera& Camera)
    {

        Render::ModelCache& instance = Render::ModelCache::Instance();

       std::shared_ptr<ModelDX> model = instance.Get("Assets/Model/LowPolyNature/BoxsLam.fbx");
         
       render.Draw(*model, m_transform, &Camera);
    }

    

    Transform& Entity::GetTransform()
    {
        return m_transform;
    }

    const Transform& Entity::GetTransform() const
    {
        return m_transform;
    }

    const std::string& Entity::GetTag() const
    {
        return m_tag;
    }
}