#pragma once

#include "PickupConfig.h"
#include "Render/Model.h"
#include "SceneGameConfigTypes.h"
#include "Vector.h"
#include "ISceneServices.h"

#include <functional>
#include <limits>
#include <memory>
#include <string>

struct ID3D11Device;

namespace Entities
{
    class Player;
}

namespace Systems
{
    class DrawSystem;
    class IPhysicsBinder;
}

namespace SweetsSpace
{
    class SweetsManager;
}

class SceneGameUIOverlay;

class SceneGameLoader
{
public:
    using LogFn = std::function<void(const std::string&)>;

    struct Config
    {
        ID3D11Device* device = nullptr;
        SceneSpace::ISceneServices* services = nullptr;
        Systems::IPhysicsBinder* physicsBinder = nullptr;
        Systems::DrawSystem* drawSystem = nullptr;
        SweetsSpace::SweetsManager* sweetsManager = nullptr;
        SceneGameConfig::PlayerSpawnTransform playerSpawnTransform{};
        float playerBaseHorizontalSpeed = 0.0f;
        float playerBaseVerticalSpeed = 0.0f;
        float playerPickupBoxWidth = 0.0f;
        float playerPickupBoxHeight = 0.0f;
        Stage::PickupOffset playerPickupOffset{};
        float playerPickupMaxDistance = 0.0f;
        float playerPickupDepthThreshold = std::numeric_limits<float>::infinity();
        float playerSpeedMultiplier = 1.0f;
    };

    struct Result
    {
        std::unique_ptr<SceneGameUIOverlay> uiOverlay;
        std::shared_ptr<Render::Model> playerModel;
        std::unique_ptr<Entities::Player> player;
        float playerPickupBoxWidth = 0.0f;
        float playerPickupBoxHeight = 0.0f;
        float3 playerPickupOriginOffset{ 0.0f, 0.0f, 0.0f };
    };

    SceneGameLoader(LogFn log, const char* logPrefix);

    Result Load(const Config& config) const;

private:
    static std::string NormalizeColliderKey(const std::string& key);
    static std::string ResolvePlayerColliderKey(const std::string& key);

    LogFn m_log;
    const char* m_logPrefix = "";
};
