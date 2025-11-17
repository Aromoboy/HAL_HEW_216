#pragma once

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>

#include "Core/UpdateNode.h"
#include "ICamera.h"
#include "ModelRender.h"
#include "Transform.h"

namespace Core
{
    class UpdateScheduler;
}

namespace Systems
{
    class DrawSystem;
    class PhysicsMachine;
}

namespace Collision
{
    class ICollider;
}

class ModelDX;

namespace SweetsSpace
{
    class BaseSweets;

    enum class SweetsState : std::uint8_t
    {
        Active,  // 現在制御中または落下中の生成されたお菓子
        Stacked, // タワーの一部であるお菓子を積み重ねることに成功した
        Ground   // 地面に置かれたお菓子が、拾われるのを待っている
    };

    class SweetsManager : public Core::UpdateNode
    {
    public:
        using SweetsList = std::list<std::shared_ptr<BaseSweets>>;

        SweetsManager();
        ~SweetsManager();

        void OnUpdate(Core::UpdateContext& ctx) override;

        void DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<CameraSpace::ICamera>& camera);

        SweetsList& GetByState(SweetsState state);
        const SweetsList& GetByState(SweetsState state) const;

        float GetHighestStackedHeight() const;

        void SetState(const std::shared_ptr<BaseSweets>& sweets, SweetsState state);
        void Erase(const std::shared_ptr<BaseSweets>& sweets);

        void SetDrawSystem(Systems::DrawSystem* drawSystem);
        void SetPhysicsMachine(Systems::PhysicsMachine* physicsMachine);
        void SetUpdateScheduler(Core::UpdateScheduler* scheduler);

        std::shared_ptr<BaseSweets> Spawn(const std::string& kind, const Transform& transform);

    private:
        struct SweetsStateHash
        {
            std::size_t operator()(SweetsState state) const noexcept
            {
                return static_cast<std::size_t>(state);
            }
        };

        std::unordered_map<SweetsState, SweetsList, SweetsStateHash> m_byState;
        std::unordered_map<std::shared_ptr<BaseSweets>, SweetsState> m_lookup;

        std::unordered_map<std::string, std::function<std::shared_ptr<BaseSweets>(const Transform&)>> m_spawner;

        Systems::DrawSystem* m_drawSystem;
        Systems::PhysicsMachine* m_physicsMachine;
        Core::UpdateScheduler* m_scheduler;

        bool m_warnedMissingDrawSystem;
        bool m_warnedMissingPhysicsMachine;
        bool m_warnedMissingScheduler;

        void RegisterWithSystems(const std::shared_ptr<BaseSweets>& sweets);

        template<typename T>
        std::shared_ptr<BaseSweets> MakeSweets(const std::string& tag,
            const std::shared_ptr<ModelDX>& model,
            const Transform& transform,
            const std::shared_ptr<Collision::ICollider>& collider,
            const float3& velocity,
            const float3& force,
            float weight,
            float damping,
            Core::UpdatePhase phase,
            int priority)
        {
            if (!model || !collider)
            {
                return nullptr;
            }

            return std::make_shared<T>(tag,
                model,
                transform.position,
                transform.rotation,
                transform.scale,
                collider,
                velocity,
                force,
                weight,
                damping,
                phase,
                priority);
        }
    };
}
