#include "SweetsPrototypeManifest.h"

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
    constexpr const char* kPrototypeRoot = "Assets/Model/SweetsPrototype/";
    constexpr const char* kLogPrefix = "[SweetsManifest] ";

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

namespace SweetsSpace
{
    constexpr const char* SweetsPrototypeManifest::kDefaultConfigPath;

    SweetsPrototypeManifest& SweetsPrototypeManifest::Instance()
    {
        static SweetsPrototypeManifest manifest;
        return manifest;
    }

    SweetsPrototypeManifest::SweetsPrototypeManifest()
    {
        ReloadFromCsv();
    }

    void SweetsPrototypeManifest::ApplyDefaults()
    {
        Register(SweetsType::Cream, "Cream", "Assets/Model/cream.fbx", "Soft-serve inspired cream scoop used for sherbet prototype");
        Register(SweetsType::Strawberry, "Strawberry", "Assets/Model/strawberry.fbx", "Textured strawberry fruit with visible seeds");
        Register(SweetsType::Blueberry, "Blueberry", "Assets/Model/blueberry.fbx", "Stylized blueberry fruit with smooth shading");
        Register(SweetsType::Pineapple, "Pineapple", "Assets/Model/pineapple.fbx", "High-detail pineapple with leafy crown");
        Register(SweetsType::Banana, "Banana", "Assets/Model/banana.fbx", "Low-poly curved banana fruit");

        RegisterExtra("Fairy", "Assets/Model/yousei.fbx", "Cross-plane fairy using embedded material data");
        RegisterExtraAlias("Fairy", "Player");
    }

    void SweetsPrototypeManifest::Register(SweetsType type, std::string key, std::string relativePath, std::string description,
        std::string materialKey, bool allowMeshPhysics)
    {
        SweetsPrototypeDescriptor descriptor{};
        descriptor.key = std::move(key);
        descriptor.modelPath = ResolveModelPath(relativePath);
        descriptor.description = std::move(description);
        descriptor.materialKey = std::move(materialKey);
        descriptor.allowMeshPhysics = allowMeshPhysics;

        const std::string normalized = NormalizeKey(descriptor.key);
        m_lookup[normalized] = type;
        m_byType[type] = std::move(descriptor);
    }

    void SweetsPrototypeManifest::RegisterAlias(SweetsType type, const std::string& aliasKey)
    {
        m_lookup[NormalizeKey(aliasKey)] = type;
    }

    void SweetsPrototypeManifest::RegisterExtra(std::string key, std::string relativePath, std::string description,
        std::string materialKey, bool allowMeshPhysics)
    {
        SweetsPrototypeDescriptor descriptor{};
        descriptor.key = std::move(key);
        descriptor.modelPath = ResolveModelPath(relativePath);
        descriptor.description = std::move(description);
        descriptor.materialKey = std::move(materialKey);
        descriptor.allowMeshPhysics = allowMeshPhysics;

        const std::string normalized = NormalizeKey(descriptor.key);
        m_extraLookup[normalized] = normalized;
        m_extras[normalized] = std::move(descriptor);
    }

    void SweetsPrototypeManifest::RegisterExtraAlias(const std::string& key, const std::string& aliasKey)
    {
        const std::string normalizedKey = NormalizeKey(key);
        const std::string normalizedAlias = NormalizeKey(aliasKey);
        if (m_extras.find(normalizedKey) != m_extras.end())
        {
            m_extraLookup[normalizedAlias] = normalizedKey;
        }
    }

    bool SweetsPrototypeManifest::ReloadFromCsv(const std::string& configPath)
    {
        m_byType.clear();
        m_lookup.clear();
        m_extras.clear();
        m_extraLookup.clear();

        ApplyDefaults();

        const bool loaded = LoadOverridesFromConfig(configPath);

        const std::array<SweetsType, 5> kTypes{
            SweetsType::Cream,
            SweetsType::Strawberry,
            SweetsType::Blueberry,
            SweetsType::Pineapple,
            SweetsType::Banana,
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

    bool SweetsPrototypeManifest::LoadOverridesFromConfig(const std::string& configPath)
    {
        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(configPath, GameplayConfig::kSectionSweetsManifest, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Config section '" << GameplayConfig::kSectionSweetsManifest
                << "' not found at '" << configPath << "'; using built-in defaults.\n";
            ManifestDebugTrace(oss.str());
            return false;
        }

        std::unordered_set<SweetsType, SweetsTypeHash> touched;
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
                    << " malformed (needs key,modelPath[,description[,materialKey=...][,allowMeshPhysics]]).\n";
                ManifestDebugTrace(warn.str());
                continue;
            }

            const std::string& key = tokens[0];
            const std::string& modelPath = tokens[1];
            bool allowMeshPhysics = false;
            std::size_t descriptionEnd = tokens.size();
            std::optional<std::string> materialKey;
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

            if (descriptionEnd > 2)
            {
                const std::string& maybeMaterialToken = tokens[descriptionEnd - 1];
                const std::string normalizedToken = NormalizeKey(maybeMaterialToken);
                const std::string prefix = "materialkey=";
                const std::string shortPrefix = "material=";
                std::string value;
                if (normalizedToken.rfind(prefix, 0) == 0)
                {
                    value = maybeMaterialToken.substr(prefix.size());
                }
                else if (normalizedToken.rfind(shortPrefix, 0) == 0)
                {
                    value = maybeMaterialToken.substr(shortPrefix.size());
                }

                if (!value.empty())
                {
                    std::string trimmedValue = Trim(value);
                    if (!trimmedValue.empty())
                    {
                        materialKey = std::move(trimmedValue);
                        descriptionEnd -= 1;
                    }
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
            auto mapToSweetsType = [](const std::string& normalized) -> std::optional<SweetsType>
            {
                if (normalized == NormalizeKey(ToString(SweetsType::Cream)) || normalized == "sherbet") return SweetsType::Cream;
                if (normalized == NormalizeKey(ToString(SweetsType::Strawberry))) return SweetsType::Strawberry;
                if (normalized == NormalizeKey(ToString(SweetsType::Blueberry))) return SweetsType::Blueberry;
                if (normalized == NormalizeKey(ToString(SweetsType::Pineapple))) return SweetsType::Pineapple;
                if (normalized == NormalizeKey(ToString(SweetsType::Banana))) return SweetsType::Banana;
                return std::nullopt;
            };

            std::optional<SweetsType> mappedType = mapToSweetsType(normalizedKey);
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
                warn << kLogPrefix << "Unknown sweets key '" << key << "' on line " << entry.lineNumber
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

                const std::string canonicalKey = ToString(*mappedType);
                const bool isAliasForCream = (*mappedType == SweetsType::Cream) && normalizedKey == "sherbet";
                const std::string registerKey = isAliasForCream ? canonicalKey : key;

                Register(*mappedType, registerKey, resolvedPath, description, materialKey.value_or(std::string{}), allowMeshPhysics);
                if (isAliasForCream || NormalizeKey(registerKey) != normalizedKey)
                {
                    RegisterAlias(*mappedType, key);
                }
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
                RegisterExtra(canonicalExtraKey, resolvedPath, description, materialKey.value_or(std::string{}), allowMeshPhysics);
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

    std::vector<SweetsPrototypeEntry> SweetsPrototypeManifest::Enumerate() const
    {
        std::vector<SweetsPrototypeEntry> entries;
        entries.reserve(m_byType.size());
        for (const auto& pair : m_byType)
        {
            entries.push_back({ pair.first, pair.second });
        }
        return entries;
    }

    const SweetsPrototypeDescriptor* SweetsPrototypeManifest::Find(const std::string& key) const
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

    std::string ToString(SweetsType type)
    {
        switch (type)
        {
        case SweetsType::Cream:
            return "Cream";
        case SweetsType::Strawberry:
            return "Strawberry";
        case SweetsType::Blueberry:
            return "Blueberry";
        case SweetsType::Pineapple:
            return "Pineapple";
        case SweetsType::Banana:
            return "Banana";
        default:
            return "Unknown";
        }
    }

    std::optional<SweetsType> ParseSweetsType(const std::string& key)
    {
        return SweetsPrototypeManifest::Instance().Resolve(key);
    }
}
