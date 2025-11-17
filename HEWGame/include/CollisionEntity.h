// CollisionEntity.h
#pragma once

#include <memory>
#include "Entity.h"
#include "ICollider.h"

namespace SceneSpace
        // 同期用のヘルパー。Transform を変更したら必ず呼び出し、
        // コライダーの AABB を最新状態に更新する
        void SyncColliderBounds();

// CollisionEntity は Entity を継承するように統一
// これで Transform や UpdatePhase が共通化され、
// カメラ・物理・描画など下位システムが一貫した扱いをできる

{
    class CollisionEntity : public Entity
    {
    public:
 //クラス図に合わせてタグ・Transform・Collider を
// ひとまとめに扱う設計に変更した
        CollisionEntity(const std::string& tag,
            const float3& pos,
            const float3& rot,
            const float3& scl,
            std::shared_ptr<Collision::ICollider> collider,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0);
        ~CollisionEntity() override;
 // Collider は shared_ptr で管理
// Physics や CollisionSystem が生ポインタ要求なので get() を返している
        Collision::ICollider* GetCollider();

    protected:
        std::shared_ptr<Collision::ICollider> m_collider;
    };
}