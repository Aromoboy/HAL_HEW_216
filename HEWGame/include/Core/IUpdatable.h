#pragma once

#include <cstdint>

class Scene;

namespace Core
{
        enum class UpdatePhase
        {
                Input,
                PrePhysics,
                Physics,
                PostPhysics,
                Gameplay,
                Camera,
                UI,
                Late,
        };

        struct UpdateContext
        {
                float deltaTime = 0.0f;
                std::uint64_t frameIndex = 0;
                Scene* scene = nullptr;
        };

        class IUpdatable
        {
        public:
                virtual ~IUpdatable() = default;

                virtual void OnUpdate(const UpdateContext& ctx) = 0;
                virtual UpdatePhase GetPhase() const = 0;
                virtual int GetPriority() const = 0;
                virtual bool IsEnabled() const = 0;
        };
}

