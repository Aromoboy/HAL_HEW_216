#include "Meteors/MeteorsRenderer.h"

#include "Meteors/BaseMeteors.h"

namespace MeteorsSpace
{
    MeteorsRenderer::MeteorsRenderer(const MeteorsRegistry& registry)
        : m_registry(&registry)
    {
    }

    void MeteorsRenderer::DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<::CameraSpace::ICamera>& camera) const
    {
        if (!camera || !m_registry)
        {
            return;
        }

        for (const auto& entry : m_registry->StateMap())
        {
            const auto& meteorsList = entry.second;
            for (const auto& meteors : meteorsList)
            {
                if (meteors && !meteors->IsPendingDestroy())
                {
                    meteors->Draw(renderer, *camera);
                }
            }
        }
    }
}
