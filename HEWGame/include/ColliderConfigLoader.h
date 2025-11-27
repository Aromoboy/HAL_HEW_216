#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Collision2D.h"

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
                float2 offset{ 0.0f, 0.0f };
                std::vector<float2> points;
                std::vector<ColliderDefinition> children;
        };

        bool LoadColliderDefinitions(const std::string& path, std::map<std::string, ColliderDefinition>& outDefinitions);

        std::shared_ptr<Collision::ICollider> BuildColliderFromDefinition(
                const ColliderDefinition& definition,
                const Transform& transform,
                Collision::CollisionLayer layer);
}

