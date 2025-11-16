// Entity.cpp
#include "Entity.h"

using namespace Entity;

CEntity::CEntity(std::string a_tag, float3 pos, float3 rot, float3 scl,
    Core::UpdatePhase phase, int priority)
    : Core::UpdateNode(phase, priority)
    , m_transform()
    , tag(a_tag)
{
    m_transform.position = pos;
    m_transform.rotation = rot;
    m_transform.scale = scl;
}

CEntity::~CEntity()
{
}

void CEntity::OnUpdate(Core::UpdateContext& ctx)
{
}


void Entity::CEntity::Draw(ModelRender render, ICamera& camera)
{
}

Transform& CEntity::GetTransform()
{
    return m_transform;
}