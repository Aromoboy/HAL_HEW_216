#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include "Core/UpdateNode.h"
#include "Entities/CollisionEntity.h"
#include "Entities/PhysicalEntity.h"
#include "Transform.h"
#include "ModelDX.h"

namespace CameraSpace
{
        class ICamera;
}

class ModelRender;

extern bool g_DrawColliders;
extern bool g_DrawColliderBounds;

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
                void RegisterCollider(Entities::PhysicalEntity& entity);
                void UnregisterRenderable(Entities::Entity& entity);
                void ClearRenderables();

                void SetStagePlaneZ(float planeZ);

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
                    Entities::CollisionEntity* collisionEntity;
                    Entities::PhysicalEntity* physicalEntity;
                };

                bool ValidateTransform(const Transform& transform, const Entities::Entity* source) const;
                void RenderEntity(const std::shared_ptr<CameraSpace::ICamera>& camera);
                void RenderCollision(const std::shared_ptr<CameraSpace::ICamera>& camera);
                void RenderColliderComponent(const Entities::PhysicalEntity& entity) const;


                ModelRender* m_renderer;
                std::vector<Entities::Entity*> m_renderables;
                std::unordered_map<Entities::Entity*, Collision::ICollider*> m_colliderRenderables;
                std::unordered_set<Entities::PhysicalEntity*> m_physicalColliderRenderables;
                std::vector<DrawCommand> m_commands;
                std::vector<DrawColliderCommand> m_colliderCommands;
                bool m_hasStagePlaneZ;
                float m_stagePlaneZ;
                mutable std::unordered_set<const Entities::Entity*> m_missingModelLogged;
                mutable std::unordered_set<const Entities::Entity*> m_invalidTransformLogged;
        };
}
