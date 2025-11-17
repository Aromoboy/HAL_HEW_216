#pragma once
#include <memory>
#include "ModelDX.h"
#include "Transform.h"
#include "ModelRender.h"
#include <Scene.h>

namespace SweetsSpace
{
    class BaseSweets
    {
    public:
        // コンストラクタ：モデルを受け取り保持
        BaseSweets(std::shared_ptr<ModelDX> model);
        ~BaseSweets() = default;

        // 更新処理
        void OnUpdate(const Core::UpdateContext& ctx);
        // 描画処理
        void Draw(ModelRender& render, const std::shared_ptr<CameraSpace::ICamera>& cam);

    protected:
        std::shared_ptr<ModelDX> m_model;  // 描画対象のモデルを保持
        Transform m_transform;             // スケール情報
    };
}
