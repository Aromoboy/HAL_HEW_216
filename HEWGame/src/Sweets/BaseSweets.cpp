#include "Sweets/BaseSweets.h"

#include <memory>
#include <utility>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    inline void DebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    inline void DebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr const char* kLogPrefix = "[BaseSweets] ";
}

namespace SweetsSpace
{
        BaseSweets::BaseSweets(const std::string& tag,
        const Transform& transform,
        std::shared_ptr<Render::Model> model,
        std::optional<Physics::Box2DShapeConfig> shapeConfig,
        Collision::CollisionLayer layer,
        Systems::PhysicsMachine* physicsMachine,
        Core::UpdatePhase phase,
        int priority)
        : Entities::PhysicalEntity(transform,
            std::move(shapeConfig),
            layer,
            std::move(model),
            physicsMachine,
            phase,
            priority,
            tag)
    {
    }

    void BaseSweets::OnUpdate(Core::UpdateContext& ctx)
    {
        (void)ctx;
    }

    void BaseSweets::Draw(Render::ModelRenderer& renderer, const ::CameraSpace::ICamera& camera)
    {
        if (!m_model)
        {
            if (!m_missingModelWarned)
            {
                DebugTrace(std::string(kLogPrefix) + "Draw skipped: sweets model missing.\n");
                m_missingModelWarned = true;
            }
            return;
        }

        SyncTransformFromBox2DBody();
        renderer.Draw(*m_model, m_transform, &camera);
    }
}

