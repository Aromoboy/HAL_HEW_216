#pragma once

#include "Core/UpdateNode.h"

#include <memory>
#include <unordered_set>
#include <vector>
#include <unordered_map>
#include "AABBCollider.h"
#include "Transform.h"

namespace CameraSpace
{
        class ICamera;
}

class ModelDX;
class ModelRender;

namespace SceneSpace
{
        class Entity;
        class CollisionEntity;
}

namespace Systems
{
        class DrawSystem : public Core::UpdateNode
        {
        public:
                DrawSystem();
                ~DrawSystem() override = default;

                void SetRenderer(ModelRender* renderer);
                void RegisterRenderable(SceneSpace::Entity& entity);
                void RegisterRenderable(SceneSpace::CollisionEntity& entity);
                void UnregisterRenderable(SceneSpace::Entity& entity);
                void ClearRenderables();

                void SetDebugLoggingEnabled(bool enabled);

                void OnUpdate(Core::UpdateContext& ctx) override;
                void RenderQueued(const std::shared_ptr<CameraSpace::ICamera>& camera);

                size_t GetRenderCommandCount() const;

        private:
                struct DrawCommand
                {
                        SceneSpace::Entity* source;
                        ModelDX* model;
                        Transform transform;
                };

                struct DrawColliderCommand
                {
                    SceneSpace::Entity* source;
                    AABB aabb;
                };

                bool ValidateTransform(const Transform& transform, const SceneSpace::Entity* source) const;
                void LogMissingRendererOnce();
                void LogMissingCameraOnce();
                void LogMissingModel(SceneSpace::Entity* entity);
                void RenderEntity(const std::shared_ptr<CameraSpace::ICamera>& camera);
                void RenderCollision(const std::shared_ptr<CameraSpace::ICamera>& camera);


                ModelRender* m_renderer;
                std::vector<SceneSpace::Entity*> m_renderables;
                std::unordered_map<SceneSpace::Entity*, Collision::ICollider*> m_colliderRenderables;
                std::vector<DrawCommand> m_commands;
                std::vector<DrawColliderCommand> m_colliderCommands;
                std::unordered_set<const SceneSpace::Entity*> m_missingModelEntities;
                bool m_debugLoggingEnabled;
                bool m_warnedMissingRenderer;
                bool m_warnedMissingCamera;
        };
}
