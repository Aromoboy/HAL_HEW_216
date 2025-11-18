// ============================================================================
// 概要: Sweets の挙動や関連ロジックを実装し、ゲーム全体 に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: ゲーム全体 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#include "Sweets.h"

namespace SweetsSpace
{
    // コンストラクタ：モデルを受け取り、Transform を初期化
    BaseSweets::BaseSweets(const std::string& tag,
                           std::shared_ptr<ModelDX> model,
                           const float3& pos,
                           const float3& rot,
                           const float3& scl,
                           std::shared_ptr<Collision::ICollider> collider,
                           const float3& velocity,
                           const float3& force,
                           float weight,
                           float damping,
                           Core::UpdatePhase phase,
                           int priority)
        :PhysicalEntity(tag, pos, rot, scl, collider, velocity, force, weight, damping, phase, priority)
        ,m_model(model)
    { }


    ModelDX* BaseSweets::GetRenderableModel()
    {
        return m_model.get();
    }

    const ModelDX* BaseSweets::GetRenderableModel() const
    {
        return m_model.get();
    }
    // 更新処理
    void BaseSweets::OnUpdate(const Core::UpdateContext& ctx)
    {
        (void)ctx;
    }

    // 描画処理
    void BaseSweets::Draw(ModelRender& render, const std::shared_ptr<CameraSpace::ICamera>& cam)
    {
        if (!m_model) return;

        // ModelRender に渡して描画
        // cam.get() で shared_ptr からポインタを取得
        render.Draw(*m_model, GetTransform(), cam.get());
    }
}
