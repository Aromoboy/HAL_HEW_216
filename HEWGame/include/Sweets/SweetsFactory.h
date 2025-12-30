#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "CollisionLayer.h"
#include "Core/UpdateNode.h"
#include "ColliderRegistry.h"
#include "Physics/ShapeConstants.h"
#include "Sweets/SweetsKeyUtils.h"
#include "Sweets/SweetsTypes.h"
#include "SweetsPrototypeManifest.h"
#include "Transform.h"

namespace Systems
{
    class PhysicsMachine;
}

namespace Render
{
    class Model;
}

namespace SweetsSpace
{
    class SweetsFactory
    {
    public:
        using SpawnerMap = std::unordered_map<std::string, std::function<std::shared_ptr<ActiveSweets>(const Transform&)>>;

        explicit SweetsFactory(Systems::PhysicsMachine& physicsMachine);

        std::shared_ptr<ActiveSweets> Create(const std::string& kind, const Transform& transform);
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

        static std::optional<FactoryPreset> BuildDefaultFactoryPreset(SweetsType type);
        static std::map<std::string, FactoryPreset> LoadFactoryPresets(bool* parseError);

        std::map<std::string, Collision::ColliderDefinition>::const_iterator FindColliderDefinition(const std::string& key) const;
        ColliderBuildResult BuildColliderForSweets(const std::string& kind,
            SweetsType type,
            const Transform& transform,
            const std::string& modelPath,
            const std::shared_ptr<Render::Model>& model,
            const float2& colliderCenter,
            bool allowMeshPhysics);

        template<typename T>
        std::shared_ptr<ActiveSweets> MakeSweets(
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

            auto sweets = std::make_shared<T>(
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

            if (!sweets)
                return nullptr;

            return sweets;
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
