// Entity.cpp
#include "Entity.h"

using namespace Entity;

CEntity::CEntity(std::string a_tag, float3 pos, float3 rot, float3 scl,
    Core::UpdatePhase phase, int priority)
    : Core::UpdateNode(phase, priority)
    , m_transform{ pos, rot, scl }  // Transformをコンストラクタで初期化
    , tag(a_tag)
{
}

CEntity::~CEntity()
{
}

void CEntity::OnUpdate(Core::UpdateContext& ctx)
{
}

void CEntity::Draw()
{
}

Transform& CEntity::GetTransform()
{
    return m_transform;
}