// PhysicalEntity.h

#pragma once

#include <memory>

#include "CollisionEntity.h"
#include "Physical.h"

namespace SceneSpace
{
    class PhysicalEntity : public CollisionEntity
    {
    public:
        PhysicalEntity(const std::string& tag,
            const float3& pos,
            const float3& rot,
            const float3& scl,
            std::shared_ptr<Collision::ICollider> collider,
            Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
            const float3& velocity = float3{ 0.0f, 0.0f, 0.0f },
            const float3& force = float3{ 0.0f, 0.0f, 0.0f },
            float weight = 1.0f,
            float damping = 0.98f,
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0);

        ~PhysicalEntity() override;

        Physics::Physical& GetPhysical();

    protected:
        Physics::Physical m_physical;
    };

    // PhysicalEntity は Collision に加えて Physical（速度・力・質量など）を持つ
    // Entity → CollisionEntity → PhysicalEntity の階層に統一
    // Transform と一緒に初期化できるようにした
    // PhysicsMachine がこの Physical を直接操作する
    // そのため参照で返している

    void TranslatePhysicalEntity(PhysicalEntity& entity, const float3& delta);
} // namespace SceneSpace
