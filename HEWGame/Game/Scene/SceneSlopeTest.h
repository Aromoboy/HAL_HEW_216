#pragma once

#include "Ball.h"
#include "Block.h"
#include "Scene.h"
#include "SceneManager.h"

#include <memory>

namespace CameraSpace
{
    class PerspectiveCamera;
    class ICamera;
}

class SceneSlopeTest : public SceneSpace::Scene, public SceneSpace::IScene
{
public:
    explicit SceneSlopeTest(SceneSpace::SceneManager& manager);

    bool Load(Core::Input& input) override;
    void Update(Core::StepTimer& timer, Core::Input& input) override;
    void Draw() override;
    void OnEnter() override;
    void OnExit() override;

protected:
    void ConfigureUpdates() override;
    void Update(const Core::UpdateContext& ctx) override;
    void Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera) override;

private:
    void DrawWorld(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera) override;

private:
    SceneSpace::SceneManager* m_manager = nullptr;
    std::shared_ptr<CameraSpace::PerspectiveCamera> m_camera;
    bool m_inputRegistered = false;

    Ball m_ball;
    std::unique_ptr<Block> m_pSlope;
    std::unique_ptr<Block> m_pFloor;
};
