#pragma once

#include "SceneGameConfigTypes.h"
#include "Entities/PhysicalEntity.h"
#include "ColliderConfigLoader.h"
#include "Transform.h"
#include "Vector.h"

#include <functional>
#include <map>
#include <optional>
#include <string>

class SceneGamePlayerColliderBuilder
{
public:
    using LogFn = std::function<void(const std::string&)>;

    struct BuildResult
    {
        std::optional<Physics::BoxColliderComponent> collider;
        std::optional<Physics::Box2DShapeConfig> shapeConfig;
        float3 halfExtents{ 0.0f, 0.0f, 0.0f };
        float3 centerOffset{ 0.0f, 0.0f, 0.0f };
        std::string source{ "Unknown" };
        std::string shapeSource{ "None" };
        bool registryHit{ false };
        bool registryHasShapes{ false };
    };

    SceneGamePlayerColliderBuilder(LogFn log, const char* logPrefix);

    BuildResult BuildFromRegistry(const std::string& colliderId,
                                  const Transform& transform,
                                  const SceneGameConfig::PlayerColliderTuning& tuning,
                                  b2BodyType bodyType);
    BuildResult BuildFromConfig(const std::string& resolvedKey,
                                const Transform& transform,
                                const SceneGameConfig::PlayerColliderTuning& tuning,
                                const std::map<std::string, Collision::ColliderDefinition>& definitions,
                                b2BodyType bodyType);
    BuildResult BuildFallback(const std::string& resolvedKey,
                              const Transform& transform,
                              const SceneGameConfig::PlayerColliderTuning& tuning,
                              b2BodyType bodyType);

private:
    LogFn m_log;
    const char* m_logPrefix = "";
};
