#pragma once


#include "CollisionEntity.h"
#include "ModelDX.h"
#include <memory>


class Player : public Entity::CollisionEntity
{
public:
    // OnUpdateは各派生クラスで実際の処理を書く場所だ
// Contextを受け取り、必要なシステム情報にアクセスできる


    Player(std::string a_tag, float3 pos, float3 rot, float3 scl,
        std::shared_ptr<Collision::ICollider> collide,
        Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
        int priority = 0);


    virtual void OnUpdate(Core::UpdateContext& ctx) override;

    virtual void Draw(ModelRender render, ICamera& camera);

private:
    std::shared_ptr<ModelDX> model;

};