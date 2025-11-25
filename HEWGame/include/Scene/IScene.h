#pragma once

#include "Core/UpdateNode.h"

namespace Core
{
    struct UpdateContext;
}

namespace SceneSpace
{
class IScene : public Core::UpdateNode
{
public:
    explicit IScene(Core::UpdatePhase phase = Core::UpdatePhase::Gameplay, int priority = 0);
    ~IScene() override = default;

    virtual void Load() = 0;
    virtual void Update(Core::UpdateContext& ctx) = 0;
    virtual void Draw() = 0;

    void OnUpdate(Core::UpdateContext& ctx) override;
};
}

inline SceneSpace::IScene::IScene(Core::UpdatePhase phase, int priority)
    : Core::UpdateNode(phase, priority)
{
}

inline void SceneSpace::IScene::OnUpdate(Core::UpdateContext& ctx)
{
    Update(ctx);
}

