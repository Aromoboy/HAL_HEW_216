// ============================================================================
// 概要: Sweets に関するインターフェースやデータ構造を宣言し、ゲーム全体 から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: ゲーム全体 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once
#include <memory>
#include "ModelDX.h"
#include "Transform.h"
#include "ModelRender.h"
#include "PhysicalEntity.h"
#include <Scene.h>

namespace SweetsSpace
{
    class BaseSweets : public SceneSpace::PhysicalEntity
    {
    public:
        ModelDX* GetRenderableModel() override;
        const ModelDX* GetRenderableModel() const override;

        // コンストラクタ：モデルを受け取り保持
        BaseSweets(const std::string& tag,
            std::shared_ptr<ModelDX> model,
            const float3& pos,
            const float3& rot,
            const float3& scl,
            std::shared_ptr<Collision::ICollider> collider,
            const float3& velocity,
            const float3& force,
            float weight  = 1.0f,
            float damping = 0.98f,
            Core::UpdatePhase phase = Core::UpdatePhase::Physics,
            int priority = 0);

        ~BaseSweets() = default;

        // 更新処理
        void OnUpdate(const Core::UpdateContext& ctx);
        // 描画処理
        void Draw(ModelRender& render, const std::shared_ptr<CameraSpace::ICamera>& cam);

    protected:
        std::shared_ptr<ModelDX> m_model;  // 描画対象のモデルを保持
    };
}
