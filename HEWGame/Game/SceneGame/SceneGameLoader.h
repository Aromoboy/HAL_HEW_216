#pragma once

#include "Render/Model.h"
#include "SceneGameConfigTypes.h"
#include "Vector.h"
#include "ISceneServices.h"

#include <functional>
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
        SceneGameConfig::PlayerSpawnTransform playerSpawnTransform{};
        SceneGameConfig::PlayerMovementSettings playerMovementSettings{};
        float playerBaseHorizontalSpeed = 0.0f;
        float playerBaseVerticalSpeed = 0.0f;
        float playerSpeedMultiplier = 1.0f;
    };

    struct Result
    {
        std::shared_ptr<Render::Model> playerModel;
        std::unique_ptr<Entities::Player> player;
    };

    SceneGameLoader(LogFn log, const char* logPrefix);

    Result Load(const Config& config) const;

private:
    static std::string NormalizeColliderKey(const std::string& key);
    static std::string ResolvePlayerColliderKey(const std::string& key);

    LogFn m_log;
    const char* m_logPrefix = "";
};
