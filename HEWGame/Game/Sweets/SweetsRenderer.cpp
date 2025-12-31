// スイーツの描画を行う役目を持つ
// スイーツ管理やシーン描画から呼ばれる

#include "Sweets/SweetsRenderer.h"

#include "Sweets/BaseSweets.h"

namespace SweetsSpace
{
    SweetsRenderer::SweetsRenderer(const SweetsRegistry& registry)
        : m_registry(&registry)
    {
    }

    void SweetsRenderer::DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<::CameraSpace::ICamera>& camera) const
    {
        if (!camera || !m_registry)
        {
            return;
        }

        for (const auto& entry : m_registry->StateMap())
        {
            const auto& sweetsList = entry.second;
            for (const auto& sweets : sweetsList)
            {
                if (sweets && !sweets->IsPendingDestroy())
                {
                    sweets->Draw(renderer, *camera);
                }
            }
        }
    }
}
