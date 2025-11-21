#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ICollider.h"

namespace SweetsSpace
{
    enum class SweetsType : std::uint8_t
    {
        Cake,
        Pudding,
        Sherbet,
        Fairy,
    };

    struct ColliderTemplate
    {
        Collision::ColliderShape shape{ Collision::ColliderShape::Box };
        float3 padding{ 0.0f, 0.0f, 0.0f };
        std::vector<float3> prismVertices; // 正規化されたXYレイアウト、境界の半分の範囲でスケーリング済み
        float depthOverride{ 0.0f }; // 0は境界から導出されることを意味する
    };

    struct SweetsPrototypeDescriptor
    {
        std::string key;
        std::string modelPath;
        std::string description;
        ColliderTemplate collider;
    };

    struct SweetsTypeHash
    {
        std::size_t operator()(SweetsType type) const noexcept
        {
            return static_cast<std::size_t>(type);
        }
    };

    class SweetsPrototypeManifest
    {
    public:
        static const SweetsPrototypeManifest& Instance();

        const SweetsPrototypeDescriptor* Find(SweetsType type) const;
        const SweetsPrototypeDescriptor* Find(const std::string& key) const;
        std::optional<SweetsType> Resolve(const std::string& key) const;

    private:
        SweetsPrototypeManifest();

        void Register(SweetsType type, std::string key, std::string relativePath, std::string description, ColliderTemplate collider);

        std::unordered_map<SweetsType, SweetsPrototypeDescriptor, SweetsTypeHash> m_byType;
        std::unordered_map<std::string, SweetsType> m_lookup;
    };

    std::string ToString(SweetsType type);
    std::optional<SweetsType> ParseSweetsType(const std::string& key);
}
