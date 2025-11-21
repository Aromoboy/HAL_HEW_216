#include "SweetsPrototypeManifest.h"

#include <cctype>
#include <utility>

namespace
{
    constexpr const char* kPrototypeRoot = "Assets/Model/SweetsPrototype/";

    std::vector<float3> MakeTriangleTemplate()
    {
        return {
            float3{ -1.0f, -1.0f, 0.0f },
            float3{ 1.0f, -1.0f, 0.0f },
            float3{ 0.0f, 1.0f, 0.0f },
        };
    }

    std::vector<float3> MakeLozengeTemplate()
    {
        return {
            float3{ 0.0f, 1.0f, 0.0f },
            float3{ 1.0f, 0.0f, 0.0f },
            float3{ 0.0f, -1.0f, 0.0f },
            float3{ -1.0f, 0.0f, 0.0f },
        };
    }

    std::string NormalizeKey(const std::string& key)
    {
        std::string normalized;
        normalized.reserve(key.size());
        for (unsigned char ch : key)
        {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        }
        return normalized;
    }
}

namespace SweetsSpace
{
    const SweetsPrototypeManifest& SweetsPrototypeManifest::Instance()
    {
        static const SweetsPrototypeManifest manifest;
        return manifest;
    }

    SweetsPrototypeManifest::SweetsPrototypeManifest()
    {
        Register(
            SweetsType::Cake,
            "Cake",
            "Cake.obj",
            "Textured sponge cake cube with frosting layers",
            ColliderTemplate{ Collision::ColliderShape::Box, float3{ 0.02f, 0.02f, 0.02f }, {}, 0.0f });
        Register(
            SweetsType::Pudding,
            "Pudding",
            "Pudding.obj",
            "UV-mapped triangular pudding slice",
            ColliderTemplate{ Collision::ColliderShape::Prism2D, float3{ 0.01f, 0.04f, 0.01f }, MakeTriangleTemplate(), 0.0f });
        Register(
            SweetsType::Sherbet,
            "Sherbet",
            "Sherbet.obj",
            "Shaded sherbet scoop with smooth normals",
            ColliderTemplate{ Collision::ColliderShape::Prism2D, float3{ 0.01f, 0.01f, 0.01f }, MakeLozengeTemplate(), 0.0f });
        Register(
            SweetsType::Fairy,
            "Fairy",
            "Fairy.obj",
            "Procedural cross-plane fairy (Fairy.obj kept as fallback)",
            ColliderTemplate{});
    }

    void SweetsPrototypeManifest::Register(SweetsType type, std::string key, std::string relativePath, std::string description, ColliderTemplate collider)
    {
        SweetsPrototypeDescriptor descriptor{};
        descriptor.key = std::move(key);
        descriptor.modelPath = std::string(kPrototypeRoot) + relativePath;
        descriptor.description = std::move(description);
        descriptor.collider = std::move(collider);

        const std::string normalized = NormalizeKey(descriptor.key);
        m_lookup[normalized] = type;
        m_byType[type] = std::move(descriptor);
    }

    const SweetsPrototypeDescriptor* SweetsPrototypeManifest::Find(SweetsType type) const
    {
        auto it = m_byType.find(type);
        if (it == m_byType.end())
        {
            return nullptr;
        }
        return &it->second;
    }

    std::optional<SweetsType> SweetsPrototypeManifest::Resolve(const std::string& key) const
    {
        const std::string normalized = NormalizeKey(key);
        auto it = m_lookup.find(normalized);
        if (it == m_lookup.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    const SweetsPrototypeDescriptor* SweetsPrototypeManifest::Find(const std::string& key) const
    {
        auto resolved = Resolve(key);
        if (!resolved)
        {
            return nullptr;
        }
        return Find(*resolved);
    }

    std::string ToString(SweetsType type)
    {
        switch (type)
        {
        case SweetsType::Cake:
            return "Cake";
        case SweetsType::Pudding:
            return "Pudding";
        case SweetsType::Sherbet:
            return "Sherbet";
        case SweetsType::Fairy:
            return "Fairy";
        default:
            return "Unknown";
        }
    }

    std::optional<SweetsType> ParseSweetsType(const std::string& key)
    {
        return SweetsPrototypeManifest::Instance().Resolve(key);
    }
}
