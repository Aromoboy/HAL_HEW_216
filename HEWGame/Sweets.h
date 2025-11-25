#pragma once
#include <memory>
#include "ModelDX.h"
#include "Transform.h"
#include "ModelRender.h"
#include "PhysicalEntity.h"
#include <Scene.h>

namespace SweetsSpace
{
    // Prototype-era implementation retained for the existing gameplay pipeline.
    class LegacyBaseSweets : public SceneSpace::PhysicalEntity
    {
    public:
        ModelDX* GetRenderableModel() override;
        const ModelDX* GetRenderableModel() const override;

        // コンストラクタ：モデルを受け取り保持
        LegacyBaseSweets(const std::string& tag,
            std::shared_ptr<ModelDX> model,
            const float3& pos,
            const float3& rot,
            const float3& scl,
            std::shared_ptr<Collision::ICollider> collider,
            Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
            const float3& velocity = float3{ 0.0f, 0.0f, 0.0f },
            const float3& force = float3{ 0.0f, 0.0f, 0.0f },
            float weight  = 1.0f,
            float damping = 0.98f,
            Core::UpdatePhase phase = Core::UpdatePhase::Physics,
            int priority = 0);

        ~LegacyBaseSweets() = default;

        // 更新処理
        void OnUpdate(const Core::UpdateContext& ctx);
        // 描画処理
        void Draw(ModelRender& render, const std::shared_ptr<CameraSpace::ICamera>& cam);

    protected:
        std::shared_ptr<ModelDX> m_model;  // 描画対象のモデルを保持
    };
}
