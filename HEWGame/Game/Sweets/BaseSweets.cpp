#include "Sweets/BaseSweets.h"

#include "ModelPivot.h"

#include <cmath>
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

    void BaseSweets::OnUpdate(const Core::UpdateContext& ctx)
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

        renderer.Draw(*m_model, m_renderTransform, &camera, m_animation.get());
    }

    Transform BaseSweets::GetRenderTransform() const
    {
        return m_renderTransform;
    }

    void BaseSweets::ApplyCementedPhysics(float gravityScale, float linearDamping, float angularDamping)
    {
        if (HasBox2DBody())
        {
            const b2BodyId bodyId = GetBox2DBodyId();
            if (!m_cemented && !m_cementedState.hasStored)
            {
                m_cementedState.gravityScale = b2Body_GetGravityScale(bodyId);
                m_cementedState.linearDamping = b2Body_GetLinearDamping(bodyId);
                m_cementedState.angularDamping = b2Body_GetAngularDamping(bodyId);
                m_cementedState.hasStored = true;
            }

            b2Body_SetGravityScale(bodyId, gravityScale);
            b2Body_SetLinearDamping(bodyId, linearDamping);
            b2Body_SetAngularDamping(bodyId, angularDamping);
            SetLinearVelocity(float3{ 0.0f, 0.0f, 0.0f });
            SetAngularVelocity(0.0f);
        }

        m_cemented = true;
    }

    void BaseSweets::RestoreCementedPhysics()
    {
        if (!m_cemented)
        {
            return;
        }

        if (HasBox2DBody() && m_cementedState.hasStored)
        {
            const b2BodyId bodyId = GetBox2DBodyId();
            b2Body_SetGravityScale(bodyId, m_cementedState.gravityScale);
            b2Body_SetLinearDamping(bodyId, m_cementedState.linearDamping);
            b2Body_SetAngularDamping(bodyId, m_cementedState.angularDamping);
        }

        m_cementedState.hasStored = false;
        m_cemented = false;
    }

    void BaseSweets::SyncFromPhysics()
    {
        float3 renderOffset{};
        if (const auto& colliderComponent = GetBoxColliderComponent())
        {
            renderOffset.x = colliderComponent->colliderOffset.x;
            renderOffset.y = colliderComponent->colliderOffset.y;
        }

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

            const float offsetAngle = updatedTransform.rotation.z;
            const float cosAngle = std::cos(offsetAngle);
            const float sinAngle = std::sin(offsetAngle);
            // コライダーオフセットはローカル座標なので回転を反映させる
            const float rotatedX = renderOffset.x * cosAngle - renderOffset.y * sinAngle;
            const float rotatedY = renderOffset.x * sinAngle + renderOffset.y * cosAngle;

            m_visual.position = updatedTransform.position;
            m_visual.position.x += rotatedX;
            m_visual.position.y += rotatedY;
            m_visual.position.z = updatedTransform.position.z;
            m_visual.rotation = updatedTransform.rotation;
            RefreshRenderTransform();
            return;
        }

        Transform updatedTransform = m_transform;
        const float offsetAngle = updatedTransform.rotation.z;
        const float cosAngle = std::cos(offsetAngle);
        const float sinAngle = std::sin(offsetAngle);
        // コライダーオフセットはローカル座標なので回転を反映させる
        const float rotatedX = renderOffset.x * cosAngle - renderOffset.y * sinAngle;
        const float rotatedY = renderOffset.x * sinAngle + renderOffset.y * cosAngle;

        m_visual.position = updatedTransform.position;
        m_visual.position.x += rotatedX;
        m_visual.position.y += rotatedY;
        m_visual.position.z = updatedTransform.position.z;
        m_visual.rotation = updatedTransform.rotation;
        RefreshRenderTransform();
    }

    void BaseSweets::RefreshRenderTransform()
    {
        // 描画のたびに合成しないようキャッシュした変換を頼る
        m_renderTransform = Transform{ m_visual.position, m_visual.rotation, m_visual.scale };
    }
}
