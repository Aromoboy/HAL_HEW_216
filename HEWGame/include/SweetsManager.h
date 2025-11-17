#pragma once

#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>

#include "Core/UpdateNode.h"
#include "ICamera.h"
#include "ModelRender.h"
#include "Transform.h"

namespace Systems
{
        class DrawSystem;
}

namespace SceneSpace
{
        class PhysicalEntity;
}

namespace SweetsSpace
{
        class BaseSweets;
}

using BaseSweets = SweetsSpace::BaseSweets;

enum class SweetsState
{
        Active,
        Staked,
        Ground
};

class SweetsManager : public Core::UpdateNode
{
public:
        using RegisterNodeCallback = std::function<void(Core::IUpdatable&)>;
        using PhysicsRegisterCallback = std::function<void(SceneSpace::PhysicalEntity&)>;
        using PhysicsUnregisterCallback = std::function<void(SceneSpace::PhysicalEntity*)>;

        SweetsManager();
        ~SweetsManager() override;

        void OnUpdate(Core::UpdateContext& ctx) override;

        void DrawAll(ModelRender& render, const std::shared_ptr<CameraSpace::ICamera>& camera);

        std::list<std::shared_ptr<BaseSweets>> GetByState(SweetsState state);

        std::shared_ptr<BaseSweets> Spawn(const std::string& kind, const Transform& transform);

        void SetDrawSystem(Systems::DrawSystem* drawSystem);
        void SetNodeRegistrationCallbacks(RegisterNodeCallback registerFn, RegisterNodeCallback unregisterFn);
        void SetPhysicsCallbacks(PhysicsRegisterCallback registerFn, PhysicsUnregisterCallback unregisterFn);

private:
        struct SweetsRecipe
        {
                std::string type;
                std::string modelPath;
                float colliderExpand = 1.0f;
                float3 colliderOffset{ 0.0f, 0.0f, 0.0f };
                float weight = 1.0f;
                float damping = 0.98f;
                float3 initialVelocity{ 0.0f, 0.0f, 0.0f };
                float3 initialForce{ 0.0f, 0.0f, 0.0f };
        };

        using Bucket = std::list<std::shared_ptr<BaseSweets>>;
        using Spawner = std::function<std::shared_ptr<BaseSweets>(const Transform&)>;

        void InitializeBuckets();
        void ConfigureRecipes();
        void RegisterDefaultSpawners();
        std::string BuildTag(const std::string& baseKind);
        void RegisterWithSystems(const std::shared_ptr<BaseSweets>& sweet);
        void UnregisterFromSystems(const std::shared_ptr<BaseSweets>& sweet);
        SweetsState EvaluateState(BaseSweets& sweet) const;
        void MoveSweetToState(const std::shared_ptr<BaseSweets>& sweet, SweetsState from, SweetsState to);
        bool EraseUnregistered(Bucket& bucket, SweetsState state);
        const char* StateToString(SweetsState state) const;
        void DebugLogSpawn(const std::string& kind, const std::shared_ptr<BaseSweets>& sweet) const;
        void DebugLogDespawn(const BaseSweets* sweet, SweetsState state, const std::string& reason) const;

        template<typename TSweets>
        void RegisterSpawnerForKind(const std::string& kind);

        template<typename TSweets>
        std::shared_ptr<BaseSweets> InstantiateFromRecipe(const SweetsRecipe& recipe, const Transform& transform);

        std::unordered_map<SweetsState, Bucket> m_byState;
        std::unordered_map<std::string, Spawner> m_spawners;
        std::unordered_map<std::string, SweetsRecipe> m_recipes;
        std::unordered_map<std::string, size_t> m_spawnCounts;

        Systems::DrawSystem* m_drawSystem;
        RegisterNodeCallback m_registerNode;
        RegisterNodeCallback m_unregisterNode;
        PhysicsRegisterCallback m_registerPhysicsBody;
        PhysicsUnregisterCallback m_unregisterPhysicsBody;

        bool m_warnedMissingScheduler;
        bool m_warnedMissingDrawSystem;
        bool m_warnedMissingPhysicsRegister;
        bool m_warnedMissingPhysicsUnregister;
};
