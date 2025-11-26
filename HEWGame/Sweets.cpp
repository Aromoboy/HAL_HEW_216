#include "Sweets.h"

#include <utility>

namespace SweetsSpace
{
    // コンストラクタ：モデルを受け取り、Transform を初期化
    LegacyBaseSweets::LegacyBaseSweets(const std::string& tag,
        std::shared_ptr<ModelDX> model,
        const float3& pos,
        const float3& rot,
        const float3& scl,
        std::shared_ptr<Collision::ICollider> collider,
        Collision::CollisionLayer layer,
        const float3& velocity,
        const float3& force,
        float weight,
        float damping,
        Core::UpdatePhase phase,
        int priority)
        : BaseSweets(tag,
            Transform{ pos, rot, scl },
            std::move(model),
            std::move(collider),
            layer,
            Physics::Physical{ velocity, force, weight, damping },
            nullptr,
            nullptr,
            phase,
            priority)
    {
    }


    ModelDX* LegacyBaseSweets::GetRenderableModel()
    {
        return m_model.get();
    }

    const ModelDX* LegacyBaseSweets::GetRenderableModel() const
    {
        return m_model.get();
    }
    // 更新処理
    void LegacyBaseSweets::OnUpdate(const Core::UpdateContext& ctx)
    {
        (void)ctx;
    }

    // 描画処理
    void LegacyBaseSweets::Draw(ModelRender& render, const std::shared_ptr<CameraSpace::ICamera>& cam)
    {
        if (!m_model)
        {
            return;
        }

        // ModelRender に渡して描画
        // cam.get() で shared_ptr からポインタを取得
        render.Draw(*m_model, GetTransform(), cam.get());
    }
}
