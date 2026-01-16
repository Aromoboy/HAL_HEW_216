#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
    
namespace SweetsSpace
{
    enum class SweetsType : std::uint8_t
    {
        Strawberry,
        Blueberry,
        Pineapple,
    };

    struct SweetsPrototypeDescriptor
    {
        std::string key;
        std::string modelPath;
        std::string description;
        bool allowMeshPhysics{ false };
    };

    struct SweetsPrototypeEntry
    {
        SweetsType type;
        SweetsPrototypeDescriptor descriptor;
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
        static constexpr const char* kDefaultConfigPath = "Assets/Config/gameplay.cfg";

        static SweetsPrototypeManifest& Instance();

        const SweetsPrototypeDescriptor* Find(SweetsType type) const;
        const SweetsPrototypeDescriptor* Find(const std::string& key) const;
        std::optional<SweetsType> Resolve(const std::string& key) const;
        std::vector<SweetsPrototypeEntry> Enumerate() const;

        bool ReloadFromCsv(const std::string& configPath = kDefaultConfigPath);

    private:
        SweetsPrototypeManifest();

        void ApplyDefaults();
        bool LoadOverridesFromConfig(const std::string& configPath);
        void Register(SweetsType type, std::string key, std::string relativePath, std::string description,
            bool allowMeshPhysics = false);
        void RegisterAlias(SweetsType type, const std::string& aliasKey);
        void RegisterExtra(std::string key, std::string relativePath, std::string description,
            bool allowMeshPhysics = false);
        void RegisterExtraAlias(const std::string& key, const std::string& aliasKey);

        std::unordered_map<SweetsType, SweetsPrototypeDescriptor, SweetsTypeHash> m_byType;
        std::unordered_map<std::string, SweetsType> m_lookup;
        std::unordered_map<std::string, SweetsPrototypeDescriptor> m_extras;
        std::unordered_map<std::string, std::string> m_extraLookup;
    };

    std::string ToString(SweetsType type);
    std::optional<SweetsType> ParseSweetsType(const std::string& key);
}
