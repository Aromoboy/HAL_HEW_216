#pragma once

#include <memory>

#include "ICamera.h"
#include "ModelRender.h"
#include "Sweets/SweetsRegistry.h"

namespace SweetsSpace
{
    class SweetsRenderer
    {
    public:
        explicit SweetsRenderer(const SweetsRegistry& registry);

        void DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<::CameraSpace::ICamera>& camera) const;

    private:
        const SweetsRegistry* m_registry;
    };
}
