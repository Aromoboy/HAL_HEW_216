// ============================================================================
// Tv: ICollider ɊւC^[tF[Xf[^\錾Ainclude ėpł悤ɂB
// ړI: ߂ẴW[ɐGJ҂łAӖƈˑ֌Wfcł悤ɍx̐t^ĂB
// Ag: include z̑t@CVXewƘA邽߁At@CԂ̊֌WǐՂۂ̖ڈƂėpłB
// ============================================================================

// l: ICollider.h Ɋւ{ŕ⑫ĂB

#ifndef __ICOLLIDER_H__
#define __ICOLLIDER_H__


#include "Transform.h"
#include "AABB.h"

namespace Collision
{
    class ICollider
    {
    public:
        virtual ~ICollider() = default;

        virtual AABB GetAABB() const = 0;

        // 衝突判定を行う純粋仮想関数
        virtual bool CheckCollision(const Collision::ICollider& other) = 0;

        // バウンディングボックスを更新する純粋仮想関数
        virtual void UpdateBounds(const Transform& transform) = 0;
    };
}
#endif __ICOLLIDER_H__