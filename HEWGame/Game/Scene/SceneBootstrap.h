#pragma once

#include <memory>

namespace SceneSpace
{
class IScene;
class SceneManager;
}

std::unique_ptr<SceneSpace::IScene> CreateStartupScene(SceneSpace::SceneManager& manager);
