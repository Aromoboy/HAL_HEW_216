#pragma once

#include <functional>
#include <memory>
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
    class ActiveMeteors;
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
        std::weak_ptr<MeteorsSpace::ActiveMeteors> meteor{};
        PlayerContactType type = PlayerContactType::Begin;
    };

    struct MissileContactEvent
    {
        std::weak_ptr<Entities::PhysicalEntity> missile{};
        std::weak_ptr<MeteorsSpace::ActiveMeteors> meteor{};
    };

    using MissileResolver = std::function<std::shared_ptr<Entities::PhysicalEntity>(Entities::PhysicalEntity*)>;

    void Bind(Systems::IPhysicsBinder& binder,
              MeteorsSpace::MeteorsManager* meteorsManager,
              Entities::PhysicalEntity* playerEntity,
              MissileResolver missileResolver);
    std::optional<PlayerContactEvent> ConsumePlayerContact();
    std::optional<MissileContactEvent> ConsumeMissileContact();
    void Reset();

private:
    MeteorsSpace::MeteorsManager* m_meteorsManager = nullptr;
    Entities::PhysicalEntity* m_playerEntity = nullptr;
    MissileResolver m_missileResolver;
    std::optional<PlayerContactEvent> m_playerContact = std::nullopt;
    std::optional<MissileContactEvent> m_missileContact = std::nullopt;
};
