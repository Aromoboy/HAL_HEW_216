// PhysicalEntity.h

#pragma once

#include <memory>

#include "CollisionEntity.h"
#include "Physical.h"

namespace SceneSpace
{
    class PhysicalEntity : public CollisionEntity
    {
 // PhysicalEntity は Collision に加えて Physical（速度・力・質量など）を持つ
// Entity → CollisionEntity → PhysicalEntity の階層に統一

    public:
        PhysicalEntity(const std::string& tag,
            const float3& pos,
            const float3& rot,
            const float3& scl,
            std::shared_ptr<Collision::ICollider> collider,
            const float3& velocity,
            const float3& force,
            float weight = 1.0f,
            float damping = 0.98f,
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0);
// 物理情報（velocity / force / weight / damping）も
// Transform と一緒に初期化できるようにした

        ~PhysicalEntity() override;
 // PhysicsMachine がこの Physical を直接操作する
// そのため参照で返している

        Physics::Physical& GetPhysical();

    protected:
        Physics::Physical m_physical;
    };
}