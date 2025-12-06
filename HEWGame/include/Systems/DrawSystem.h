#pragma once

#include "Core/UpdateNode.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include "AABBCollider.h"
#include "Entities/CollisionEntity.h"
#include "Transform.h"
#include "ModelDX.h"

namespace CameraSpace
{
        class ICamera;
}

class ModelRender;

extern bool g_DrawColliders;

namespace Systems
{
        class DrawSystem : public Core::UpdateNode
        {
        public:
                DrawSystem();
                ~DrawSystem() override = default;

                void SetRenderer(ModelRender* renderer);
                void RegisterRenderable(Entities::Entity& entity);
                void RegisterRenderable(Entities::CollisionEntity& entity);
                void UnregisterRenderable(Entities::Entity& entity);
                void ClearRenderables();

                void OnUpdate(Core::UpdateContext& ctx) override;
                void RenderQueued(const std::shared_ptr<CameraSpace::ICamera>& camera);
                void DebugDrawQueued(const std::shared_ptr<CameraSpace::ICamera>& camera);

                size_t GetRenderCommandCount() const;

        private:
                struct DrawCommand
                {
                        Entities::Entity* source;
                        Render::Model* model;
                        Transform transform;
                };

                struct DrawColliderCommand
                {
                    Entities::Entity* source;
                    Collision::ICollider* collider;
                    AABB aabb;
                };

                bool ValidateTransform(const Transform& transform, const Entities::Entity* source) const;
                void RenderEntity(const std::shared_ptr<CameraSpace::ICamera>& camera);
                void RenderCollision(const std::shared_ptr<CameraSpace::ICamera>& camera);


                ModelRender* m_renderer;
                std::vector<Entities::Entity*> m_renderables;
                std::unordered_map<Entities::Entity*, Collision::ICollider*> m_colliderRenderables;
                std::vector<DrawCommand> m_commands;
                std::vector<DrawColliderCommand> m_colliderCommands;
        };
}
