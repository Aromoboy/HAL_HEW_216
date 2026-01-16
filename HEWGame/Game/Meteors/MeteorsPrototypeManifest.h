#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
    
namespace MeteorsSpace
{
    enum class MeteorsType : std::uint8_t
    {
        Mercury,
        Meteor,
        Meteorite,
    };

    struct MeteorsPrototypeDescriptor
    {
        std::string key;
        std::string modelPath;
        std::string description;
        bool allowMeshPhysics{ false };
    };

    struct MeteorsPrototypeEntry
    {
        MeteorsType type;
        MeteorsPrototypeDescriptor descriptor;
    };

    struct MeteorsTypeHash
    {
        std::size_t operator()(MeteorsType type) const noexcept
        {
            return static_cast<std::size_t>(type);
        }
    };

    class MeteorsPrototypeManifest
    {
    public:
        static constexpr const char* kDefaultConfigPath = "Assets/Config/gameplay.cfg";

        static MeteorsPrototypeManifest& Instance();

        const MeteorsPrototypeDescriptor* Find(MeteorsType type) const;
        const MeteorsPrototypeDescriptor* Find(const std::string& key) const;
        std::optional<MeteorsType> Resolve(const std::string& key) const;
        std::vector<MeteorsPrototypeEntry> Enumerate() const;

        bool ReloadFromCsv(const std::string& configPath = kDefaultConfigPath);

    private:
        MeteorsPrototypeManifest();

        void ApplyDefaults();
        bool LoadOverridesFromConfig(const std::string& configPath);
        void Register(MeteorsType type, std::string key, std::string relativePath, std::string description,
            bool allowMeshPhysics = false);
        void RegisterAlias(MeteorsType type, const std::string& aliasKey);
        void RegisterExtra(std::string key, std::string relativePath, std::string description,
            bool allowMeshPhysics = false);
        void RegisterExtraAlias(const std::string& key, const std::string& aliasKey);

        std::unordered_map<MeteorsType, MeteorsPrototypeDescriptor, MeteorsTypeHash> m_byType;
        std::unordered_map<std::string, MeteorsType> m_lookup;
        std::unordered_map<std::string, MeteorsPrototypeDescriptor> m_extras;
        std::unordered_map<std::string, std::string> m_extraLookup;
    };

    std::string ToString(MeteorsType type);
    std::optional<MeteorsType> ParseMeteorsType(const std::string& key);
}
