#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "CollisionLayer.h"
#include "Core/UpdateNode.h"
#include "ColliderConfigLoader.h"
#include "ColliderRegistry.h"
#include "Render/Model.h"
#include "Systems/Physics/ShapeConstants.h"
#include "Meteors/MeteorsKeyUtils.h"
#include "Meteors/MeteorsTypes.h"
#include "MeteorsPrototypeManifest.h"
#include "Transform.h"

namespace Systems
{
    class PhysicsMachine;
}

namespace MeteorsSpace
{
    class MeteorsFactory
    {
    public:
        using SpawnerMap = std::unordered_map<std::string, std::function<std::shared_ptr<ActiveMeteors>(const Transform&)>>;

        explicit MeteorsFactory(Systems::PhysicsMachine& physicsMachine);

        std::shared_ptr<ActiveMeteors> Create(const std::string& kind, const Transform& transform);
        std::vector<std::pair<std::string, FactoryPreset>> EnumerateFactoryPresets() const;

        const std::map<std::string, FactoryPreset>& GetFactoryPresets() const;
        std::optional<FactoryPreset> FindFactoryPreset(const std::string& normalizedKind) const;
        const SpawnerMap& GetSpawners() const;

        std::string ResolveKindKey(const std::string& key) const;

        void SetGeneratedColliderThicknessPadding(float factor);
        void SetGeneratedColliderMinThickness(float thickness);
        void SetForceGeneratedColliders(bool force);
        void SetForceConfigColliders(bool force, const std::vector<std::string>& kinds);
        void SetColliderConfigOverridesEnabled(bool enabled);

    private:
        struct GeneratedCollider
        {
            std::vector<float2> hull;
            float2 centroidOffset{ 0.0f, 0.0f };
            float thickness = 1.0f;
        };

        static std::optional<FactoryPreset> BuildDefaultFactoryPreset(MeteorsType type);
        static std::map<std::string, FactoryPreset> LoadFactoryPresets(bool* parseError);

        std::map<std::string, Collision::ColliderDefinition>::const_iterator FindColliderDefinition(const std::string& key) const;
        ColliderBuildResult BuildColliderForMeteors(const std::string& kind,
            MeteorsType type,
            const Transform& transform,
            const std::string& modelPath,
            const std::shared_ptr<Render::Model>& model,
            const float2& colliderCenter,
            bool allowMeshPhysics);

        template<typename T>
        std::shared_ptr<ActiveMeteors> MakeMeteors(
            const std::string& tag,
            const std::shared_ptr<Render::Model>& model,
            const Transform& transform,
            const std::optional<Physics::Box2DShapeConfig>& shapeConfig,
            Collision::CollisionLayer layer,
            Core::UpdatePhase phase,
            int priority,
            bool allowMeshPhysics)
        {
            if (!model)
                return nullptr;

            auto meteors = std::make_shared<T>(
                tag,
                transform,
                model,
                shapeConfig,
                layer,
                m_physicsMachine,
                phase,
                priority,
                allowMeshPhysics
            );

            if (!meteors)
                return nullptr;

            return meteors;
        }

        Systems::PhysicsMachine* m_physicsMachine;
        SpawnerMap m_spawner;
        std::map<std::string, FactoryPreset> m_factoryPresets;
        std::map<std::string, Collision::ColliderDefinition> m_colliderDefinitions;
        std::unordered_set<std::string> m_warnedMissingColliderDefinitionKinds;
        std::unordered_set<std::string> m_warnedMissingFactoryPresetKinds;
        bool m_warnedMissingGeneratedHull{ false };
        float m_generatedColliderThicknessPadding{ 1.0f };
        float m_generatedColliderMinThickness{ Physics::kMinimumShapeDimensionMeters };
        bool m_forceGeneratedColliders{ false };
        bool m_forceConfigColliders{ false };
        std::unordered_set<std::string> m_forceConfigColliderKinds;
        bool m_colliderConfigOverridesEnabled{ true };
    };
}
