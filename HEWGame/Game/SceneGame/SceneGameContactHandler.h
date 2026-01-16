#pragma once

#include <optional>

namespace Entities
{
    class PhysicalEntity;
}

namespace Systems
{
    class IPhysicsBinder;
}

namespace MeteorsSpace
{
    class MeteorsManager;
}

class SceneGameContactHandler
{
public:
    enum class PlayerContactType
    {
        Begin,
        End,
    };

    struct PlayerContactEvent
    {
        Entities::PhysicalEntity* meteor = nullptr;
        PlayerContactType type = PlayerContactType::Begin;
    };

    struct MissileContactEvent
    {
        Entities::PhysicalEntity* missile = nullptr;
        Entities::PhysicalEntity* meteor = nullptr;
    };

    void Bind(Systems::IPhysicsBinder& binder,
              MeteorsSpace::MeteorsManager* meteorsManager,
              Entities::PhysicalEntity* playerEntity);
    std::optional<PlayerContactEvent> ConsumePlayerContact();
    std::optional<MissileContactEvent> ConsumeMissileContact();
    void Reset();

private:
    MeteorsSpace::MeteorsManager* m_meteorsManager = nullptr;
    Entities::PhysicalEntity* m_playerEntity = nullptr;
    std::optional<PlayerContactEvent> m_playerContact = std::nullopt;
    std::optional<MissileContactEvent> m_missileContact = std::nullopt;
};
