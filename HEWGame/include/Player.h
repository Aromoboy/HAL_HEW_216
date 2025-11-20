#ifndef ___PLAYER_H___
#define ___PLAYER_H___

#include "CollisionEntity.h"
#include "Input.h"
#include "ICamera.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "PhysicalEntity.h"
#include <memory>
#include <vector>

namespace SweetsSpace
{
    class BaseSweets;
    class SweetsManager;
}

void Test();

namespace SceneSpace
{
    class Player : public SceneSpace::PhysicalEntity
    {
    public:
        ModelDX* GetRenderableModel() override;
        const ModelDX* GetRenderableModel() const override;

        Player(std::string a_tag, float3 pos, float3 rot, float3 scl,
            std::shared_ptr<Collision::ICollider> collide,
            float3 velocity = { 0.0f, 0.0f, 0.0f }, float3 force = { 0.0f, 0.0f, 0.0f }, float weight = 1.0f, float damping = 0.98f,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0,
            bool kinematic = false);

        virtual void OnUpdate(Core::UpdateContext& ctx) override;

        virtual void Draw(ModelRender& render, CameraSpace::ICamera& camera);

        void HandleInput(Core::Input input);

        void SetSweetsManager(SweetsSpace::SweetsManager* manager);
        bool IsCarryingSweets() const;
        const std::vector<std::shared_ptr<SweetsSpace::BaseSweets>>& GetCarriedSweets() const;
        void ClearCarriedSweets();
        bool IsKinematic() const;
        void SetMoveSpeed(float speed);
        float GetMoveSpeed() const;
        void SetMinimumY(float minY);
        float GetMinimumY() const;
        bool HasMinimumY() const;

    private:
        std::shared_ptr<ModelDX> model;
        SweetsSpace::SweetsManager* m_sweetsManager;
        std::vector<std::shared_ptr<SweetsSpace::BaseSweets>> m_carrieds;
        int m_maxCarry;
        float m_pickupRadius;
        float3 m_carryOffset;
        float m_dropUpwardBoost;
        bool m_isKinematic;
        float m_moveSpeed;
        bool m_hasMinimumY;
        float m_minimumY;

        void UpdateCarriedSweets();
        void HandleCarryInput(Core::UpdateContext& ctx);
        void TryPickupNearest();
        void DropCarriedSweets();
        float3 ComputeCarryAnchor() const;
        void ClampVerticalPosition();
    };
}

#endif // !___PLAYER_H___
