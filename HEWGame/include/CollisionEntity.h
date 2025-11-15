// CollisionEntity.h
#ifndef __COLLISIONENTITY_H__
#define __COLLISIONENTITY_H__

#include <memory>
#include "Entity.h"
#include "ICollider.h"

namespace Entity
{
    class CollisionEntity : public CEntity
    {
    public:
        CollisionEntity(std::string a_tag, float3 pos, float3 rot, float3 scl,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay, int priority = 0);
        virtual ~CollisionEntity();

        // Collider‚ÌQÆ‚ğ•Ô‚·
        Collision::ICollider* GetCollider();

        // Collider‚ğİ’è
        void SetCollider(std::shared_ptr<Collision::ICollider> collider);

    protected:
        std::shared_ptr<Collision::ICollider> m_collider;
    };
}
#endif