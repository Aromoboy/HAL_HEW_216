#include "SceneBootstrap.h"

#include "SceneSlopeTest.h"

std::unique_ptr<SceneSpace::IScene> CreateStartupScene(SceneSpace::SceneManager& manager)
{
    return std::make_unique<SceneSlopeTest>(manager);
}
