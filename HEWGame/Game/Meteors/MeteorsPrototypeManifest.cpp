#include "MeteorsPrototypeManifest.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>
#include <unordered_set>
#include <vector>
#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

#include "ConfigSectionReader.h"
#include "GameplayConfig.h"

namespace
{
    constexpr const char* kPrototypeRoot = "Assets/Model/MeteorsPrototype/";
    constexpr const char* kLogPrefix = "[MeteorsManifest] ";

#if defined(_WIN32) && defined(_DEBUG)
    inline void ManifestDebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    inline void ManifestDebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

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

    std::string Trim(const std::string& value)
    {
        auto begin = value.begin();
        auto end = value.end();
        while (begin != end && std::isspace(static_cast<unsigned char>(*begin)))
        {
            ++begin;
        }
        while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1))))
        {
            --end;
        }
        return std::string(begin, end);
    }

    bool IsAbsolutePath(const std::string& path)
    {
        if (path.empty())
        {
            return false;
        }
        if (path[0] == '/' || path[0] == '\\')
        {
            return true;
        }
        if (path.size() > 1 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':')
        {
            return true;
        }
        return false;
    }

    std::string ResolveModelPath(const std::string& modelPath)
    {
        const std::string trimmed = Trim(modelPath);
        if (trimmed.empty())
        {
            return std::string{};
        }

        if (IsAbsolutePath(trimmed) || trimmed.rfind("Assets/", 0) == 0)
        {
            return trimmed;
        }

        return std::string(kPrototypeRoot) + trimmed;
    }

}

namespace MeteorsSpace
{
    constexpr const char* MeteorsPrototypeManifest::kDefaultConfigPath;

    MeteorsPrototypeManifest& MeteorsPrototypeManifest::Instance()
    {
        static MeteorsPrototypeManifest manifest;
        return manifest;
    }

    MeteorsPrototypeManifest::MeteorsPrototypeManifest()
    {
        ReloadFromCsv();
    }

    void MeteorsPrototypeManifest::ApplyDefaults()
    {
        Register(MeteorsType::Mercury, "Mercury", "Assets/Model/Mercury.fbx",
            "Mercury-inspired meteor sphere with cratered surface");
        Register(MeteorsType::Meteor, "Meteor", "Assets/Model/meteor.fbx",
            "Meteor model with jagged rocky silhouette");
        Register(MeteorsType::Meteorite, "Meteorite", "Assets/Model/Meteorite.fbx",
            "Meteorite chunk with uneven, fractured surface");

        RegisterExtra("Fairy", "Assets/Model/base.fbx", "Fighter jet prototype");
        RegisterExtraAlias("Fairy", "Player");
        RegisterExtra("Missile", "Assets/Model/Missile.fbx", "Air-to-air heat seeker missile model");
    }

    void MeteorsPrototypeManifest::Register(MeteorsType type, std::string key, std::string relativePath, std::string description,
        bool allowMeshPhysics)
    {
        MeteorsPrototypeDescriptor descriptor{};
        descriptor.key = std::move(key);
        descriptor.modelPath = ResolveModelPath(relativePath);
        descriptor.description = std::move(description);
        descriptor.allowMeshPhysics = allowMeshPhysics;

        const std::string normalized = NormalizeKey(descriptor.key);
        m_lookup[normalized] = type;
        m_byType[type] = std::move(descriptor);
    }

    void MeteorsPrototypeManifest::RegisterAlias(MeteorsType type, const std::string& aliasKey)
    {
        m_lookup[NormalizeKey(aliasKey)] = type;
    }

    void MeteorsPrototypeManifest::RegisterExtra(std::string key, std::string relativePath, std::string description,
        bool allowMeshPhysics)
    {
        MeteorsPrototypeDescriptor descriptor{};
        descriptor.key = std::move(key);
        descriptor.modelPath = ResolveModelPath(relativePath);
        descriptor.description = std::move(description);
        descriptor.allowMeshPhysics = allowMeshPhysics;

        const std::string normalized = NormalizeKey(descriptor.key);
        m_extraLookup[normalized] = normalized;
        m_extras[normalized] = std::move(descriptor);
    }

    void MeteorsPrototypeManifest::RegisterExtraAlias(const std::string& key, const std::string& aliasKey)
    {
        const std::string normalizedKey = NormalizeKey(key);
        const std::string normalizedAlias = NormalizeKey(aliasKey);
        if (m_extras.find(normalizedKey) != m_extras.end())
        {
            m_extraLookup[normalizedAlias] = normalizedKey;
        }
    }

    bool MeteorsPrototypeManifest::ReloadFromCsv(const std::string& configPath)
    {
        m_byType.clear();
        m_lookup.clear();
        m_extras.clear();
        m_extraLookup.clear();

        ApplyDefaults();

        const bool loaded = LoadOverridesFromConfig(configPath);

        const std::array<MeteorsType, 3> kTypes{
            MeteorsType::Mercury,
            MeteorsType::Meteor,
            MeteorsType::Meteorite,
        };
        for (const auto type : kTypes)
        {
            if (m_byType.find(type) == m_byType.end())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Missing descriptor for type '" << ToString(type)
                    << "' even after defaults. This should not happen.\n";
                ManifestDebugTrace(oss.str());
                continue;
            }

            const std::string normalized = NormalizeKey(m_byType[type].key);
            if (m_lookup.find(normalized) == m_lookup.end())
            {
                m_lookup[normalized] = type;
            }
        }

        return loaded;
    }

    bool MeteorsPrototypeManifest::LoadOverridesFromConfig(const std::string& configPath)
    {
        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(configPath, GameplayConfig::kSectionMeteorsManifest, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Config section '" << GameplayConfig::kSectionMeteorsManifest
                << "' not found at '" << configPath << "'; using built-in defaults.\n";
            ManifestDebugTrace(oss.str());
            return false;
        }

        std::unordered_set<MeteorsType, MeteorsTypeHash> touched;
        for (const auto& entry : lines)
        {
            if (entry.key != "entry")
            {
                continue;
            }

            const std::string trimmed = Trim(entry.value);
            if (trimmed.empty())
            {
                continue;
            }

            std::vector<std::string> tokens;
            std::stringstream ss(trimmed);
            std::string column;
            while (std::getline(ss, column, ','))
            {
                tokens.push_back(Trim(column));
            }

            if (tokens.size() < 2)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "Line " << entry.lineNumber
                    << " malformed (needs key,modelPath[,description[,allowMeshPhysics]]).\n";
                ManifestDebugTrace(warn.str());
                continue;
            }

            const std::string& key = tokens[0];
            const std::string& modelPath = tokens[1];
            bool allowMeshPhysics = false;
            std::size_t descriptionEnd = tokens.size();
            if (tokens.size() >= 3)
            {
                const std::string& maybeFlag = tokens.back();
                const std::string normalizedFlag = NormalizeKey(maybeFlag);
                if (normalizedFlag == "true" || normalizedFlag == "1")
                {
                    allowMeshPhysics = true;
                    descriptionEnd = tokens.size() - 1;
                }
                else if (normalizedFlag == "false" || normalizedFlag == "0")
                {
                    allowMeshPhysics = false;
                    descriptionEnd = tokens.size() - 1;
                }
            }

            std::string description;
            if (tokens.size() >= 3)
            {
                const std::size_t startIndex = 2;
                if (descriptionEnd > startIndex)
                {
                    description = tokens[startIndex];
                    for (std::size_t i = startIndex + 1; i < descriptionEnd; ++i)
                    {
                        description.append(",");
                        description.append(tokens[i]);
                    }
                }
            }

            const std::string normalizedKey = NormalizeKey(key);
            const std::string normalizedModelHeader = NormalizeKey(modelPath);
            if (normalizedKey == "key" && normalizedModelHeader == "modelpath")
            {
                continue;
            }
            auto mapToMeteorsType = [](const std::string& normalized) -> std::optional<MeteorsType>
            {
                if (normalized == NormalizeKey(ToString(MeteorsType::Mercury))) return MeteorsType::Mercury;
                if (normalized == NormalizeKey(ToString(MeteorsType::Meteor))) return MeteorsType::Meteor;
                if (normalized == NormalizeKey(ToString(MeteorsType::Meteorite))) return MeteorsType::Meteorite;
                return std::nullopt;
            };

            std::optional<MeteorsType> mappedType = mapToMeteorsType(normalizedKey);
            std::optional<std::string> mappedExtraKey;
            const auto extraIt = m_extraLookup.find(normalizedKey);
            if (!mappedType && extraIt != m_extraLookup.end())
            {
                mappedExtraKey = extraIt->second;
            }
            const bool isExtraAlias = mappedExtraKey && normalizedKey != *mappedExtraKey;

            if (!mappedType && !mappedExtraKey)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "Unknown meteors key '" << key << "' on line " << entry.lineNumber
                    << "; entry ignored.\n";
                ManifestDebugTrace(warn.str());
                continue;
            }

            const std::string resolvedPath = ResolveModelPath(modelPath);
            if (resolvedPath.empty())
            {
                std::ostringstream warn;
                warn << kLogPrefix << "Empty model path for key '" << key << "' on line " << entry.lineNumber
                    << "; keeping previous entry.\n";
                ManifestDebugTrace(warn.str());
                continue;
            }

            if (mappedType)
            {
                if (description.empty())
                {
                    const auto* existing = Find(*mappedType);
                    description = existing ? existing->description : std::string{};
                }

                Register(*mappedType, key, resolvedPath, description, allowMeshPhysics);
                touched.insert(*mappedType);
            }
            else if (mappedExtraKey)
            {
                if (description.empty())
                {
                    const auto descriptorIt = m_extras.find(*mappedExtraKey);
                    if (descriptorIt != m_extras.end())
                    {
                        description = descriptorIt->second.description;
                    }
                }

                std::string canonicalExtraKey = key;
                const auto descriptorIt = m_extras.find(*mappedExtraKey);
                if (descriptorIt != m_extras.end())
                {
                    canonicalExtraKey = descriptorIt->second.key;
                }
                RegisterExtra(canonicalExtraKey, resolvedPath, description, allowMeshPhysics);
                if (isExtraAlias)
                {
                    RegisterExtraAlias(canonicalExtraKey, key);
                }
            }

        }

        for (auto type : touched)
        {
            const auto* descriptor = Find(type);
            if (!descriptor)
            {
                continue;
            }
            const std::string normalized = NormalizeKey(descriptor->key);
            m_lookup[normalized] = type;
        }

        return true;
    }

    const MeteorsPrototypeDescriptor* MeteorsPrototypeManifest::Find(MeteorsType type) const
    {
        auto it = m_byType.find(type);
        if (it == m_byType.end())
        {
            return nullptr;
        }
        return &it->second;
    }

    std::optional<MeteorsType> MeteorsPrototypeManifest::Resolve(const std::string& key) const
    {
        const std::string normalized = NormalizeKey(key);
        auto it = m_lookup.find(normalized);
        if (it == m_lookup.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    std::vector<MeteorsPrototypeEntry> MeteorsPrototypeManifest::Enumerate() const
    {
        std::vector<MeteorsPrototypeEntry> entries;
        entries.reserve(m_byType.size());
        for (const auto& pair : m_byType)
        {
            entries.push_back({ pair.first, pair.second });
        }
        return entries;
    }

    const MeteorsPrototypeDescriptor* MeteorsPrototypeManifest::Find(const std::string& key) const
    {
        auto resolved = Resolve(key);
        if (resolved)
        {
            return Find(*resolved);
        }

        const std::string normalized = NormalizeKey(key);
        const auto aliasIt = m_extraLookup.find(normalized);
        if (aliasIt == m_extraLookup.end())
        {
            return nullptr;
        }

        const auto descriptorIt = m_extras.find(aliasIt->second);
        if (descriptorIt == m_extras.end())
        {
            return nullptr;
        }
        return &descriptorIt->second;
    }

    std::string ToString(MeteorsType type)
    {
        switch (type)
        {
        case MeteorsType::Mercury:
            return "Mercury";
        case MeteorsType::Meteor:
            return "Meteor";
        case MeteorsType::Meteorite:
            return "Meteorite";
        default:
            return "Unknown";
        }
    }

    std::optional<MeteorsType> ParseMeteorsType(const std::string& key)
    {
        return MeteorsPrototypeManifest::Instance().Resolve(key);
    }
}
