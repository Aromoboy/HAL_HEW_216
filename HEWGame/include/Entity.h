#pragma once

#include <string>

#include "Core/UpdateNode.h"
#include "Transform.h"

#include "ModelRender.h"
#include "ICamera.h"

namespace SceneSpace
{
    class Entity : public Core::UpdateNode
// Entity はゲーム内オブジェクトの最小単位
// Transform / UpdatePhase / Tag をまとめて管理する基底クラス
// 各サブクラス（CollisionEntity / PhysicalEntity / Camera anchors 等）は
// この共通インターフェースを前提に動く

    {
    public:
        Entity(const std::string& tag,
            const float3& pos = float3{ 0.0f, 0.0f, 0.0f },
            const float3& rot = float3{ 0.0f, 0.0f, 0.0f },
            const float3& scl = float3{ 1.0f, 1.0f, 1.0f },
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0);
        ~Entity() override;

        void OnUpdate(Core::UpdateContext& ctx) override;

        virtual void Draw(ModelRender render, CameraSpace::ICamera& Camera);
 // Transform のデフォルト値を許可して、
// どの翻訳単位でも軽く使えるようにした
// （pos / rot / scale を毎回指定しなくてOK）
 // Transform への mutable / const アクセス
// CameraRig・PhysicsMachine などが直接参照する前提

        Transform& GetTransform();
        const Transform& GetTransform() const;

        const std::string& GetTag() const;

    protected:
        Transform m_transform;
        std::string m_tag;
    };
}