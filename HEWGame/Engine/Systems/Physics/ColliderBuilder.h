#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>

#include <box2d/box2d.h>

#include "BoxColliderComponent.h"
#include "ColliderConfigLoader.h"
#include "Entities/PhysicalEntity.h"
#include "SceneGameConfigTypes.h"
#include "Transform.h"

namespace Physics
{
    class ColliderBuilder
    {
    public:
        using LogFn = std::function<void(const std::string&)>;

        struct BuildResult
        {
            std::optional<BoxColliderComponent> collider;
            std::optional<Box2DShapeConfig> shapeConfig;
            float3 centerOffset{ 0.0f, 0.0f, 0.0f };
            float3 halfExtents{ 0.0f, 0.0f, 0.0f };
            std::string source{};
            std::string shapeSource{};
            bool registryHit{ false };
            bool registryHasShapes{ false };
            bool attemptedConfigFallback{ false };
            bool attemptedRegistryFallback{ false };
            bool attemptedFallback{ false };
        };

        struct RegistryBuildRequest
        {
            std::string key{};
            Transform transform{};
            b2BodyType bodyType{ b2_dynamicBody };
            bool fixedRotation{ false };
            bool allowRotation{ true };
            float angularDamping{ 0.0f };
            float linearDamping{ 0.0f };
            bool useRegistryLinearDamping{ false };
            float3 offset{ 0.0f, 0.0f, 0.0f };
            bool scaleOffset{ true };
            const SceneGameConfig::PlayerColliderTuning* tuning{ nullptr };
            const std::map<std::string, Collision::ColliderDefinition>* configDefinitions{ nullptr };
            bool forceConfig{ false };
            bool allowFallback{ false };
        };

        ColliderBuilder(LogFn log, const char* logPrefix = nullptr);

        BuildResult BuildFromRegistry(const RegistryBuildRequest& request) const;

    private:
        LogFn m_log{};
        std::string m_logPrefix{};
    };
}
