#include "Systems/DrawSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "Core/IUpdatable.h"
#include "Entities/Entity.h"
#include "Entities/CollisionEntity.h"
#include "Collision2D.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "Geometory.h"

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

        bool ReadDebugColliderCvar()
        {
                const char* value = std::getenv("r.DebugColliders");
                if (!value)
                {
                        return true;
                }

                return std::atoi(value) != 0;
        }
}

bool g_DrawColliders = ReadDebugColliderCvar();

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
                    auto* collisionEntity = dynamic_cast<Entities::CollisionEntity*>(it.first);
                    if (!collisionEntity || !it.second)
                    {
                        continue;
                    }

                    m_colliderCommands.push_back(DrawColliderCommand{ collisionEntity });

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

                command.source->DebugDraw();
            }

            m_colliderCommands.clear();
            Geometory::DrawLines();
        }
}
