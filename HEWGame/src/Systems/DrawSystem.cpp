#include "Systems/DrawSystem.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

#if defined(_WIN32)
#       ifndef NOMINMAX
#               define NOMINMAX
#       endif
#       include <windows.h>
#endif

#include "Core/IUpdatable.h"
#include "Entities/Entity.h"
#include "Entities/CollisionEntity.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "Geometory.h"

namespace
{
        void DebugTrace(const std::string& message)
        {
#if defined(_WIN32) && defined(_DEBUG)
                ::OutputDebugStringA(message.c_str());
#else
                (void)message;
#endif
        }

        constexpr const char* kLogPrefix = "[DrawSystem] ";

        bool IsFiniteFloat(float value)
        {
                return std::isfinite(value) != 0;
        }

        bool IsFiniteVector(const float3& vec)
        {
                return IsFiniteFloat(vec.x) && IsFiniteFloat(vec.y) && IsFiniteFloat(vec.z);
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
                , m_missingModelEntities()
                , m_debugLoggingEnabled(false)
                , m_warnedMissingRenderer(false)
                , m_warnedMissingCamera(false)
        {
        }

        void DrawSystem::SetRenderer(ModelRender* renderer)
        {
                if (m_renderer == renderer)
                {
                        return;
                }

                m_renderer = renderer;
                m_warnedMissingRenderer = false;
        }

        void DrawSystem::RegisterRenderable(SceneSpace::Entity& entity)
        {
                SceneSpace::Entity* entityPtr = &entity;
                if (std::find(m_renderables.begin(), m_renderables.end(), entityPtr) != m_renderables.end())
                {
#if defined(_DEBUG)
                        std::ostringstream oss;
                        oss << kLogPrefix << "Duplicate renderable registration ignored entity=" << entityPtr << "\n";
                        DebugTrace(oss.str());
#endif
                        return;
                }

                m_renderables.push_back(entityPtr);
        }

        void DrawSystem::UnregisterRenderable(SceneSpace::Entity& entity)
        {
                SceneSpace::Entity* entityPtr = &entity;
                auto it = std::remove(m_renderables.begin(), m_renderables.end(), entityPtr);
                if (it == m_renderables.end())
                {
#if defined(_DEBUG)
                        std::ostringstream oss;
                        oss << kLogPrefix << "Unregister requested for unknown renderable entity=" << entityPtr << "\n";
                        DebugTrace(oss.str());
#endif
                        return;
                }

                m_renderables.erase(it, m_renderables.end());

                std::unordered_map<SceneSpace::Entity*, Collision::ICollider*>::iterator colIt = m_colliderRenderables.find(entityPtr);

                if (colIt != m_colliderRenderables.end())
                {
                    m_colliderRenderables.erase(colIt);
                }

                m_missingModelEntities.erase(entityPtr);
        }

        void DrawSystem::RegisterRenderable(SceneSpace::CollisionEntity& entity)
        {
            SceneSpace::CollisionEntity* entityPtr = &entity;
            if (std::find(m_renderables.begin(), m_renderables.end(), entityPtr) != m_renderables.end())
            {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << kLogPrefix << "Duplicate renderable registration ignored entity=" << entityPtr << "\n";
                DebugTrace(oss.str());
#endif
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
                m_missingModelEntities.clear();
        }

        void DrawSystem::SetDebugLoggingEnabled(bool enabled)
        {
                m_debugLoggingEnabled = enabled;
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

                for (SceneSpace::Entity* entity : m_renderables)
                {
                        if (!entity)
                        {
                                continue;
                        }

                        ModelDX* model = entity->GetRenderableModel();
                        if (!model)
                        {
                                LogMissingModel(entity);
                                continue;
                        }

                        const Transform& transform = entity->GetTransform();
                        if (!ValidateTransform(transform, entity))
                        {
                                continue;
                        }

                        m_commands.push_back(DrawCommand{ entity, model, transform });

                        if (m_debugLoggingEnabled)
                        {
                                std::ostringstream oss;
                                oss << kLogPrefix << "Queued render for entity=" << entity
                                    << " pos=(" << transform.position.x << ',' << transform.position.y << ',' << transform.position.z << ')'
                                    << " rot=(" << transform.rotation.x << ',' << transform.rotation.y << ',' << transform.rotation.z << ')'
                                    << " scale=(" << transform.scale.x << ',' << transform.scale.y << ',' << transform.scale.z << ")\n";
                                DebugTrace(oss.str());
                        }
                }

                for (auto it : m_colliderRenderables)
                {
                    m_colliderCommands.push_back(DrawColliderCommand{it.first, it.second->GetAABB()});

                }
        }

        void DrawSystem::RenderQueued(const std::shared_ptr<CameraSpace::ICamera>& camera)
        {
                if (!m_renderer)
                {
                        LogMissingRendererOnce();
                        return;
                }

                if (!camera)
                {
                        LogMissingCameraOnce();
                        return;
                }

                m_warnedMissingCamera = false;

                RenderEntity(camera);

                RenderCollision(camera);

        }

        size_t DrawSystem::GetRenderCommandCount() const
        {
                return m_commands.size();
        }

        bool DrawSystem::ValidateTransform(const Transform& transform, const SceneSpace::Entity* source) const
        {
                if (IsFiniteVector(transform.position) && IsFiniteVector(transform.rotation) && IsFiniteVector(transform.scale))
                {
                        return true;
                }

                std::ostringstream oss;
                oss << kLogPrefix << "Invalid transform detected for entity=" << source
                    << " pos=(" << transform.position.x << ',' << transform.position.y << ',' << transform.position.z << ')'
                    << " rot=(" << transform.rotation.x << ',' << transform.rotation.y << ',' << transform.rotation.z << ')'
                    << " scale=(" << transform.scale.x << ',' << transform.scale.y << ',' << transform.scale.z << ")\n";
                DebugTrace(oss.str());
                return false;
        }

        void DrawSystem::LogMissingRendererOnce()
        {
                if (m_warnedMissingRenderer)
                {
                        return;
                }

                DebugTrace(std::string(kLogPrefix) + "RenderQueued skipped: renderer not configured.\n");
                m_warnedMissingRenderer = true;
        }

        void DrawSystem::LogMissingCameraOnce()
        {
                if (m_warnedMissingCamera)
                {
                        return;
                }

                DebugTrace(std::string(kLogPrefix) + "RenderQueued skipped: active camera missing.\n");
                m_warnedMissingCamera = true;
        }

        void DrawSystem::LogMissingModel(SceneSpace::Entity* entity)
        {
                if (!entity)
                {
                        return;
                }

                if (m_missingModelEntities.insert(entity).second)
                {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Renderable entity=" << entity << " has no model assigned.\n";
                        DebugTrace(oss.str());
                }
        }
        void DrawSystem::RenderEntity(const std::shared_ptr<CameraSpace::ICamera>& camera)
        {
            if (m_commands.empty())
            {
                if (m_debugLoggingEnabled)
                {
                    DebugTrace(std::string(kLogPrefix) + "RenderQueued called with no commands.\n");
                }
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

            if (m_debugLoggingEnabled)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Submitted " << m_commands.size() << " draw commands.\n";
                DebugTrace(oss.str());
            }




            m_commands.clear();
        }

        void DrawSystem::RenderCollision(const std::shared_ptr<CameraSpace::ICamera>& camera)
        {
            if (m_colliderCommands.empty())
            {
                if (m_debugLoggingEnabled)
                {
                    DebugTrace(std::string(kLogPrefix) + "RenderQueued called with no commands.\n");
                }
                return;
            }

            for (const DrawColliderCommand& command : m_colliderCommands)
            {
                // `æ

                DirectX::XMFLOAT3 leftup, rightup, leftdown, rightdown;

                Transform trandform = command.source->GetTransform();
                AABB itAABB = command.aabb;

                leftup.x = itAABB.min.x;
                leftup.y = itAABB.max.y;
                leftup.z = trandform.position.z;

                rightup.x = itAABB.max.x;
                rightup.y = itAABB.max.y;
                rightup.z = trandform.position.z;

                leftdown.x = itAABB.min.x;
                leftdown.y = itAABB.min.y;
                leftdown.z = trandform.position.z;

                rightdown.x = itAABB.max.x;
                rightdown.y = itAABB.min.y;
                rightdown.z = trandform.position.z;

                Geometory::AddLine(leftup, leftdown);

                Geometory::AddLine(leftup, rightup);

                Geometory::AddLine(rightdown, leftdown);

                Geometory::AddLine(rightdown, rightup);
            }

            if (m_debugLoggingEnabled)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Submitted " << m_colliderCommands.size() << " draw commands.\n";
                DebugTrace(oss.str());
            }




            m_colliderCommands.clear();
        }
}
