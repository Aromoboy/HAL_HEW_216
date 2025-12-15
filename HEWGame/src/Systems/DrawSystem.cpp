#include "Systems/DrawSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>

#if defined(_WIN32) && defined(_DEBUG)
#    include <windows.h>
#endif

#include "Core/IUpdatable.h"
#include "Entities/Entity.h"
#include "Entities/CollisionEntity.h"
#include "Entities/PhysicalEntity.h"
#include "Collision2D.h"
#include "DirectX.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "Geometory.h"
#include "DebugColliderRenderer.h"

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
        void DebugTrace(const std::string& message)
        {
                ::OutputDebugStringA(message.c_str());
        }
#else
        void DebugTrace(const std::string& message)
        {
                (void)message;
        }
#endif

        bool IsFiniteFloat(float value)
        {
                return std::isfinite(value) != 0;
        }

        bool IsFiniteVector(const float3& vec)
        {
                return IsFiniteFloat(vec.x) && IsFiniteFloat(vec.y) && IsFiniteFloat(vec.z);
        }

        constexpr float kMinRenderableScale = 1e-3f;
        constexpr float kMaxRenderableScale = 1.0e4f;
        constexpr float kMaxDepthMagnitude = 1.0e5f;
        constexpr float kPlaneZTolerance = 5.0f;

        float2 ApplyTransform2D(const float2& local, const Transform& transform)
        {
                const float scaledX = local.x * transform.scale.x;
                const float scaledY = local.y * transform.scale.y;
                const float cosR = std::cos(transform.rotation.z);
                const float sinR = std::sin(transform.rotation.z);

                return float2{
                        transform.position.x + (cosR * scaledX - sinR * scaledY),
                        transform.position.y + (sinR * scaledX + cosR * scaledY),
                };
        }

        bool ReadDebugColliderCvar()
        {
                const char* value = std::getenv("r.DebugColliders");
                if (!value)
                {
                        return true;
                }

                return std::atoi(value) != 0;
        }

        bool ReadDebugColliderBoundsCvar()
        {
                const char* value = std::getenv("r.DebugColliderBounds");
                if (!value)
                {
                        return false;
                }

                return std::atoi(value) != 0;
        }
}

bool g_DrawColliders = ReadDebugColliderCvar();
bool g_DrawColliderBounds = ReadDebugColliderBoundsCvar();

namespace Systems
{
        DrawSystem::DrawSystem()
                : Core::UpdateNode(Core::UpdatePhase::Camera, 0)
                , m_renderer(nullptr)
                , m_renderables()
                , m_commands()
                , m_colliderCommands()
                , m_hasStagePlaneZ(false)
                , m_stagePlaneZ(0.0f)
        {
        }

        void DrawSystem::SetRenderer(ModelRender* renderer)
        {
                if (m_renderer == renderer)
                {
                        return;
                }

                m_renderer = renderer;
        }

        void DrawSystem::RegisterRenderable(Entities::Entity& entity)
        {
                Entities::Entity* entityPtr = &entity;
                if (std::find(m_renderables.begin(), m_renderables.end(), entityPtr) != m_renderables.end())
                {
                        return;
                }

                m_renderables.push_back(entityPtr);
        }

        void DrawSystem::UnregisterRenderable(Entities::Entity& entity)
        {
                Entities::Entity* entityPtr = &entity;
                auto it = std::remove(m_renderables.begin(), m_renderables.end(), entityPtr);
                if (it == m_renderables.end())
                {
                        return;
                }

                m_renderables.erase(it, m_renderables.end());

                std::unordered_map<Entities::Entity*, Collision::ICollider*>::iterator colIt = m_colliderRenderables.find(entityPtr);

                if (colIt != m_colliderRenderables.end())
                {
                    m_colliderRenderables.erase(colIt);
                }

                if (auto* physical = dynamic_cast<Entities::PhysicalEntity*>(entityPtr))
                {
                        m_physicalColliderRenderables.erase(physical);
                }

        }

        void DrawSystem::RegisterRenderable(Entities::CollisionEntity& entity)
        {
            Entities::CollisionEntity* entityPtr = &entity;
            if (std::find(m_renderables.begin(), m_renderables.end(), entityPtr) != m_renderables.end())
            {
                return;
            }

            m_renderables.push_back(entityPtr);

            m_colliderRenderables[entityPtr] = entityPtr->GetCollider();
        }

        void DrawSystem::RegisterCollider(Entities::PhysicalEntity& entity)
        {
                m_physicalColliderRenderables.insert(&entity);
        }

        void DrawSystem::ClearRenderables()
        {
                m_renderables.clear();
                m_colliderRenderables.clear();
                m_physicalColliderRenderables.clear();
                m_commands.clear();
                m_colliderCommands.clear();
                m_missingModelLogged.clear();
                m_invalidTransformLogged.clear();
        }

        void DrawSystem::SetStagePlaneZ(float planeZ)
        {
                if (!std::isfinite(planeZ))
                {
                        return;
                }

                m_stagePlaneZ = planeZ;
                m_hasStagePlaneZ = true;
        }

        void DrawSystem::OnUpdate(Core::UpdateContext& ctx)
        {
                (void)ctx;

                m_commands.clear();
                m_colliderCommands.clear();
                if (m_renderables.empty())
                {
                        return;
                }

                for (Entities::Entity* entity : m_renderables)
                {
                        if (!entity)
                        {
                                continue;
                        }

                        Render::Model* model = entity->GetRenderableModel();
                        if (!model)
                        {
                                if (m_missingModelLogged.insert(entity).second)
                                {
                                        std::ostringstream oss;
                                        oss << "[DrawSystem] Entity '" << entity->GetTag() << "' 描画モデル欠落のため描画コマンドをスキップ\n";
                                        DebugTrace(oss.str());
                                }
                                continue;
                        }

                        const Transform transform = entity->GetRenderTransform();
                        if (!ValidateTransform(transform, entity))
                        {
                                continue;
                        }

                        m_commands.push_back(DrawCommand{ entity, model, transform });
                }

                for (auto it : m_colliderRenderables)
                {
                    auto* collisionEntity = dynamic_cast<Entities::CollisionEntity*>(it.first);
                    if (!collisionEntity || !it.second)
                    {
                        continue;
                    }

                    m_colliderCommands.push_back(DrawColliderCommand{ collisionEntity, nullptr });

                }

                for (auto* physical : m_physicalColliderRenderables)
                {
                        if (!physical)
                        {
                                continue;
                        }

                        const auto& boxCollider = physical->GetBoxColliderComponent();
                        const bool hasCollider = boxCollider && (!boxCollider->Empty() || boxCollider->createdFixtureCount > 0);
                        if (!hasCollider)
                        {
                                continue;
                        }

                        m_colliderCommands.push_back(DrawColliderCommand{ nullptr, physical });
                }
        }

        void DrawSystem::RenderQueued(const std::shared_ptr<CameraSpace::ICamera>& camera)
        {
                if (!m_renderer || !camera)
                {
                        return;
                }

                RenderEntity(camera);
        }

        void DrawSystem::DebugDrawQueued(const std::shared_ptr<CameraSpace::ICamera>& camera)
        {
                if (m_colliderCommands.empty())
                {
                        return;
                }

                if (!g_DrawColliders)
                {
                        m_colliderCommands.clear();
                        return;
                }

                if (!camera)
                {
                        m_colliderCommands.clear();
                        return;
                }

                RenderCollision(camera);
        }

        size_t DrawSystem::GetRenderCommandCount() const
        {
                return m_commands.size();
        }

        bool DrawSystem::ValidateTransform(const Transform& transform, const Entities::Entity* source) const
        {
                if (!IsFiniteVector(transform.position) || !IsFiniteVector(transform.rotation) || !IsFiniteVector(transform.scale))
                {
                        if (m_invalidTransformLogged.insert(source).second)
                        {
                                std::ostringstream oss;
                                oss << "[DrawSystem] 変換値が非有限のためエンティティ '" << (source ? source->GetTag() : "<null>")
                                        << "' を却下\n";
                                DebugTrace(oss.str());
                        }
                        return false;
                }

                const auto scaleInvalid = [](float component)
                {
                        return std::fabs(component) < kMinRenderableScale || std::fabs(component) > kMaxRenderableScale;
                };

                if (scaleInvalid(transform.scale.x) || scaleInvalid(transform.scale.y) || scaleInvalid(transform.scale.z))
                {
                        if (m_invalidTransformLogged.insert(source).second)
                        {
                                std::ostringstream oss;
                                oss << "[DrawSystem] スケール範囲外のためエンティティ '" << (source ? source->GetTag() : "<null>")
                                        << "' を却下 (" << transform.scale.x << ','
                                        << transform.scale.y << ',' << transform.scale.z << ")\n";
                                DebugTrace(oss.str());
                        }
                        return false;
                }

                if (std::fabs(transform.position.z) > kMaxDepthMagnitude)
                {
                        if (m_invalidTransformLogged.insert(source).second)
                        {
                                std::ostringstream oss;
                                oss << "[DrawSystem] Z位置が極端なためエンティティ '" << (source ? source->GetTag() : "<null>")
                                        << "' を却下 z=" << transform.position.z << "\n";
                                DebugTrace(oss.str());
                        }
                        return false;
                }

                if (m_hasStagePlaneZ && transform.position.z < (m_stagePlaneZ - kPlaneZTolerance))
                {
                        if (m_invalidTransformLogged.insert(source).second)
                        {
                                std::ostringstream oss;
                                oss << "[DrawSystem] ステージ平面より背面のためエンティティ '" << (source ? source->GetTag() : "<null>")
                                        << "' を却下 (z=" << transform.position.z
                                        << " planeZ=" << m_stagePlaneZ << ")\n";
                                DebugTrace(oss.str());
                        }
                        return false;
                }

                return true;
        }
        void DrawSystem::RenderEntity(const std::shared_ptr<CameraSpace::ICamera>& camera)
        {
            if (m_commands.empty())
            {
                return;
            }

            SetBlendMode(BLEND_ALPHA);
            SetCullingMode(D3D11_CULL_NONE);
            SetDepthTest(true);

            for (const DrawCommand& command : m_commands)
            {
                if (!command.model)
                {
                    continue;
                }

                m_renderer->Draw(*command.model, command.transform, camera.get());
            }


            m_commands.clear();
        }

        void DrawSystem::RenderCollision(const std::shared_ptr<CameraSpace::ICamera>& camera)
        {
            (void)camera;
            if (m_colliderCommands.empty())
            {
                return;
            }

            for (const DrawColliderCommand& command : m_colliderCommands)
            {
                if (command.collisionEntity)
                {
                        command.collisionEntity->DebugDraw();
                        continue;
                }

                if (command.physicalEntity)
                {
                        RenderColliderComponent(*command.physicalEntity);
                }
            }

            m_colliderCommands.clear();
            Geometory::DrawLines();
        }

        void DrawSystem::RenderColliderComponent(const Entities::PhysicalEntity& entity) const
        {
                const auto& colliderComponent = entity.GetBoxColliderComponent();
                if (!colliderComponent || colliderComponent->createdFixtureCount <= 0)
                {
                        return;
                }

                constexpr Color kColliderColor{ 0.9f, 0.3f, 0.3f, 1.0f };
                constexpr Color kBoundsColor{ 1.0f, 1.0f, 0.0f, 1.0f };

                const Transform& transform = entity.GetTransform();
                const float z = transform.position.z;

                for (const auto& shape : colliderComponent->shapes)
                {
                        switch (shape.type)
                        {
                        case Physics::ColliderShapeType::Circle:
                        {
                                const float2 center = ApplyTransform2D(float2{ 0.0f, 0.0f }, transform);
                                const float scaledRadius = shape.radius * std::max(transform.scale.x, transform.scale.y);
                                DebugColliderRenderer::DrawCircle(center, scaledRadius, z, kColliderColor);
                                break;
                        }
                        case Physics::ColliderShapeType::Box:
                        case Physics::ColliderShapeType::Polygon:
                        default:
                        {
                                if (shape.vertices.empty())
                                {
                                        continue;
                                }

                                std::vector<float2> hull;
                                hull.reserve(shape.vertices.size());
                                for (const auto& vertex : shape.vertices)
                                {
                                        hull.push_back(ApplyTransform2D(float2{ vertex.x, vertex.y }, transform));
                                }

                                DebugColliderRenderer::DrawHull(hull, z, kColliderColor);
                                break;
                        }
                        }
                }

                if (!colliderComponent->shapes.empty())
                {
                        const AABB bounds = entity.GetAABB();
                        DebugColliderRenderer::DrawHull(
                                {
                                        float2{ bounds.min.x, bounds.min.y },
                                        float2{ bounds.max.x, bounds.min.y },
                                        float2{ bounds.max.x, bounds.max.y },
                                        float2{ bounds.min.x, bounds.max.y },
                                },
                                transform.position.z,
                                kBoundsColor);
                }
        }
}
