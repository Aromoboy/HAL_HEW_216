#include "Sweets.h"

namespace SweetsSpace
{
    // コンストラクタ：モデルを受け取り、Transform を初期化
    BaseSweets::BaseSweets(std::shared_ptr<ModelDX> model)
        : m_model(model)
    {
        m_transform.scale = { 1,1,1 };
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
        // m_transform スケールを反映
        // cam.get() で shared_ptr からポインタを取得
        render.Draw(*m_model, m_transform, cam.get());
    }
}
