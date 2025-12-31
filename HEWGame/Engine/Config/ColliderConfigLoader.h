#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Vector.h"

namespace Collision
{
        enum class ColliderKind
        {
                Circle,
                Hull,
                Compound,
        };

        struct ColliderDefinition
        {
                ColliderKind type{ ColliderKind::Circle };
                float radius{ 0.5f };
                float thickness{ 0.1f };
                float scale{ 1.0f };
                float margin{ 0.0f };
                std::optional<float2> margin2;
                std::optional<float3> margin3;
                float2 offset{ 0.0f, 0.0f };
                std::vector<float2> points;
                std::vector<ColliderDefinition> children;
        };

        bool LoadColliderDefinitions(
                const std::string& path,
                const std::string& section,
                std::map<std::string, ColliderDefinition>& outDefinitions);
}
