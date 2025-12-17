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
        int priority,
        bool allowMeshPhysics)
        : Entities::PhysicalEntity(transform,
            std::move(shapeConfig),
            layer,
            std::move(model),
            physicsMachine,
            phase,
            priority,
            tag,
            allowMeshPhysics)
        , m_visual{ transform.position, transform.rotation, transform.scale }
    {
        RefreshRenderTransform();
    }

    void BaseSweets::OnUpdate(Core::UpdateContext& ctx)
    {
        SyncFromPhysics();
        Entities::PhysicalEntity::OnUpdate(ctx);
    }

    void BaseSweets::Draw(Render::ModelRenderer& renderer, const ::CameraSpace::ICamera& camera)
    {
        if (!m_model)
        {
            if (!m_missingModelWarned)
            {
                DebugTrace(std::string(kLogPrefix) + "描画スキップ: スイーツモデルがない\n");
                m_missingModelWarned = true;
            }
            return;
        }

        renderer.Draw(*m_model, m_renderTransform, &camera);
    }

    Transform BaseSweets::GetRenderTransform() const
    {
        return m_renderTransform;
    }

    void BaseSweets::SyncFromPhysics()
    {
        if (HasBox2DBody())
        {
            const b2Vec2 position = b2Body_GetPosition(GetBox2DBodyId());
            const float angle = b2Rot_GetAngle(b2Body_GetRotation(GetBox2DBodyId()));
            const float3 worldPosition = ToWorldPosition(position);
            const float worldAngle = ToWorldAngle(angle);

            Transform updatedTransform = m_transform;
            updatedTransform.position = worldPosition;
            updatedTransform.rotation.z = worldAngle;

            SetTransform(updatedTransform);

            m_visual.position = updatedTransform.position;
            m_visual.rotation = updatedTransform.rotation;
            RefreshRenderTransform();
            return;
        }

        m_visual.position = m_transform.position;
        m_visual.rotation = m_transform.rotation;
        RefreshRenderTransform();
    }

    void BaseSweets::RefreshRenderTransform()
    {
        // 描画のたびに合成しないようキャッシュした変換を頼る
        m_renderTransform = Transform{ m_visual.position, m_visual.rotation, m_visual.scale };
    }
}

