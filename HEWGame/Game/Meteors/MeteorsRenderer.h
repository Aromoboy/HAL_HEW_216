#pragma once

#include <memory>

#include "ICamera.h"
#include "ModelRender.h"
#include "Meteors/MeteorsRegistry.h"

namespace MeteorsSpace
{
    class MeteorsRenderer
    {
    public:
        explicit MeteorsRenderer(const MeteorsRegistry& registry);

        void DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<::CameraSpace::ICamera>& camera) const;

    private:
        const MeteorsRegistry* m_registry;
    };
}
