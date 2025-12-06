#include "Systems/DrawSystem.h"

#include <algorithm>
#include <cmath>
#include <DirectXMath.h>

#include "Core/IUpdatable.h"
#include "Entities/Entity.h"
#include "Entities/CollisionEntity.h"
#include "Collision2D.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "Geometory.h"

bool g_DrawColliders = false;

namespace
{
        bool IsFiniteFloat(float value)
        {
                return std::isfinite(value) != 0;
        }

        bool IsFiniteVector(const float3& vec)
        {
                return IsFiniteFloat(vec.x) && IsFiniteFloat(vec.y) && IsFiniteFloat(vec.z);
        }

        DirectX::XMFLOAT3 ToFloat3(const float2& point, float z)
        {
                return DirectX::XMFLOAT3{ point.x, point.y, z };
        }

        void DrawAABB2D(const AABB& bounds, float z, const DirectX::XMFLOAT4& color)
        {
                DirectX::XMFLOAT3 leftUp{ bounds.min.x, bounds.max.y, z };
                DirectX::XMFLOAT3 rightUp{ bounds.max.x, bounds.max.y, z };
                DirectX::XMFLOAT3 leftDown{ bounds.min.x, bounds.min.y, z };
                DirectX::XMFLOAT3 rightDown{ bounds.max.x, bounds.min.y, z };

                Geometory::AddLine(leftUp, leftDown, color);
                Geometory::AddLine(leftUp, rightUp, color);
                Geometory::AddLine(rightDown, leftDown, color);
                Geometory::AddLine(rightDown, rightUp, color);
        }

        void DrawCircleOutline(const Collision::CircleCollider2D& circle, float z, const DirectX::XMFLOAT4& color)
        {
                constexpr int kSegments = 24;
                const float2 center = circle.GetCenter();
                const float radius = circle.GetRadius();
                const float step = DirectX::XM_2PI / static_cast<float>(kSegments);

                for (int i = 0; i < kSegments; ++i)
                {
                        const float startAngle = step * static_cast<float>(i);
                        const float endAngle = step * static_cast<float>(i + 1);
                        const DirectX::XMFLOAT3 start{
                                center.x + std::cos(startAngle) * radius,
                                center.y + std::sin(startAngle) * radius,
                                z };
                        const DirectX::XMFLOAT3 end{
                                center.x + std::cos(endAngle) * radius,
                                center.y + std::sin(endAngle) * radius,
                                z };
                        Geometory::AddLine(start, end, color);
                }
        }

        void DrawHullOutline(const Collision::ConvexHullCollider2D& hull, float z, const DirectX::XMFLOAT4& color)
        {
                const auto& points = hull.GetWorldPoints();
                if (points.size() < 2)
                {
                        return;
                }

                for (size_t i = 0; i < points.size(); ++i)
                {
                        const size_t next = (i + 1) % points.size();
                        Geometory::AddLine(ToFloat3(points[i], z), ToFloat3(points[next], z), color);
                }
        }

        DirectX::XMFLOAT4 GetLayerColor(const Collision::ICollider* collider)
        {
                if (!collider)
                {
                        return DirectX::XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f };
                }

                switch (collider->GetLayer())
                {
                case Collision::CollisionLayer::Player:
                        return DirectX::XMFLOAT4{ 0.0f, 1.0f, 1.0f, 1.0f }; // Cyan
                case Collision::CollisionLayer::Floor:
                        return DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f }; // White
                case Collision::CollisionLayer::Sweets:
                        return DirectX::XMFLOAT4{ 1.0f, 1.0f, 0.0f, 1.0f }; // Yellow
                case Collision::CollisionLayer::Default:
                default:
                        return DirectX::XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f };
                }
        }
}

namespace Systems
{
        DrawSystem::DrawSystem()
                : Core::UpdateNode(Core::UpdatePhase::Camera, 0)
                , m_renderer(nullptr)
                , m_renderables()
                , m_commands()
                , m_colliderCommands()
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

        void DrawSystem::ClearRenderables()
        {
                m_renderables.clear();
                m_colliderRenderables.clear();
                m_commands.clear();
                m_colliderCommands.clear();
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
                                continue;
                        }

                        const Transform& transform = entity->GetTransform();
                        if (!ValidateTransform(transform, entity))
                        {
                                continue;
                        }

                        m_commands.push_back(DrawCommand{ entity, model, transform });
                }

                for (auto it : m_colliderRenderables)
                {
                    m_colliderCommands.push_back(DrawColliderCommand{ it.first, it.second, it.second->GetAABB() });

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
                (void)source;
                if (IsFiniteVector(transform.position) && IsFiniteVector(transform.rotation) && IsFiniteVector(transform.scale))
                {
                        return true;
                }
                return false;
        }
        void DrawSystem::RenderEntity(const std::shared_ptr<CameraSpace::ICamera>& camera)
        {
            if (m_commands.empty())
            {
                return;
            }

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
                if (!command.source)
                {
                    continue;
                }

                const Transform transform = command.source->GetTransform();
                const float z = transform.position.z;
                const auto* collider2D = dynamic_cast<const Collision::ICollider2D*>(command.collider);
                const DirectX::XMFLOAT4 layerColor = GetLayerColor(command.collider);

                if (const auto* compound = dynamic_cast<const Collision::CompoundCollider2D*>(collider2D))
                {
                    size_t childIndex = 0;
                    for (const auto& child : compound->GetChildren())
                    {
                        const auto* child2D = dynamic_cast<const Collision::ICollider2D*>(child.get());
                        if (!child2D)
                        {
                            ++childIndex;
                            continue;
                        }

                        const DirectX::XMFLOAT4 color = layerColor;
                        if (const auto* circle = dynamic_cast<const Collision::CircleCollider2D*>(child2D))
                        {
                            DrawCircleOutline(*circle, z, color);
                        }
                        else if (const auto* hull = dynamic_cast<const Collision::ConvexHullCollider2D*>(child2D))
                        {
                            DrawHullOutline(*hull, z, color);
                        }
                        else
                        {
                            DrawAABB2D(child2D->GetAABB(), z, color);
                        }

                        ++childIndex;
                    }
                }

                if (const auto* circle = dynamic_cast<const Collision::CircleCollider2D*>(collider2D))
                {
                    DrawCircleOutline(*circle, z, layerColor);
                }
                else if (const auto* hull = dynamic_cast<const Collision::ConvexHullCollider2D*>(collider2D))
                {
                    DrawHullOutline(*hull, z, layerColor);
                }
                else if (collider2D)
                {
                    DrawAABB2D(collider2D->GetAABB(), z, layerColor);
                }
                else
                {
                    DrawAABB2D(command.aabb, z, layerColor);
                }
            }

            m_colliderCommands.clear();
            Geometory::DrawLines();
        }
}
