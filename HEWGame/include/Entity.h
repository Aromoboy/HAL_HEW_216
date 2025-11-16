// Entity.h
#ifndef __ENTITY_H__
#define __ENTITY_H__

#include <string>
#include "Core/UpdateNode.h"
#include "Transform.h"

#include "ModelRender.h"

namespace Entity
{
    class CEntity : public Core::UpdateNode
    {
    public:
        CEntity(std::string a_tag, float3 pos, float3 rot, float3 scl,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay, int priority = 0);
        virtual ~CEntity();

        // UpdateNodeの関数をオーバーライド
        virtual void OnUpdate(Core::UpdateContext& ctx) override;

        // 描画処理（UpdateNodeとは別に独自に定義）
        virtual void Draw(ModelRender render, ICamera& camera);

        Transform& GetTransform();

    protected:
        Transform m_transform;
        std::string tag;
    };
}
#endif