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

    void Bind(Systems::IPhysicsBinder& binder,
              MeteorsSpace::MeteorsManager* meteorsManager,
              Entities::PhysicalEntity* playerEntity);
    std::optional<PlayerContactEvent> ConsumePlayerContact();
    void Reset();

private:
    MeteorsSpace::MeteorsManager* m_meteorsManager = nullptr;
    Entities::PhysicalEntity* m_playerEntity = nullptr;
    std::optional<PlayerContactEvent> m_playerContact = std::nullopt;
};
