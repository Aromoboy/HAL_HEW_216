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
        Cake,
        Pudding,
        Sherbet,
        Strawberry,
        Fairy,
    };

    struct SweetsPrototypeDescriptor
    {
        std::string key;
        std::string modelPath;
        std::string description;
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
        static constexpr const char* kDefaultCsvPath = "Assets/Config/sweets_models.csv";

        static SweetsPrototypeManifest& Instance();

        const SweetsPrototypeDescriptor* Find(SweetsType type) const;
        const SweetsPrototypeDescriptor* Find(const std::string& key) const;
        std::optional<SweetsType> Resolve(const std::string& key) const;
        std::vector<SweetsPrototypeEntry> Enumerate() const;

        bool ReloadFromCsv(const std::string& csvPath = kDefaultCsvPath);

    private:
        SweetsPrototypeManifest();

        void ApplyDefaults();
        bool LoadOverridesFromCsv(const std::string& csvPath);
        void Register(SweetsType type, std::string key, std::string relativePath, std::string description);
        void RegisterAlias(SweetsType type, const std::string& aliasKey);

        std::unordered_map<SweetsType, SweetsPrototypeDescriptor, SweetsTypeHash> m_byType;
        std::unordered_map<std::string, SweetsType> m_lookup;
    };

    std::string ToString(SweetsType type);
    std::optional<SweetsType> ParseSweetsType(const std::string& key);
}
