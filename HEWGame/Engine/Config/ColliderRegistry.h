#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <box2d/box2d.h>

#include "BoxColliderComponent.h"

namespace Physics
{
    struct ColliderRegistryDefinition
    {
        std::vector<ColliderShapeDescriptor> shapes;
        b2ShapeDef shapeDef{ b2DefaultShapeDef() };
        float linearDamping{ 0.0f };
    };

    class ColliderRegistry
    {
    public:
        static ColliderRegistry& Instance();

        bool LoadFromFile(const std::string& path);
        std::optional<ColliderRegistryDefinition> Find(const std::string& id) const;

    private:
        std::map<std::string, ColliderRegistryDefinition> m_definitions;

        static std::string NormalizeKey(const std::string& id);
    };
}
