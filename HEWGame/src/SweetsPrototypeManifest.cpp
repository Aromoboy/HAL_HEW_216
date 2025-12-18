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
    constexpr const char* SweetsPrototypeManifest::kDefaultCsvPath;

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
        Register(SweetsType::Cake, "Cake", "Cake.obj", "Textured sponge cake cube with frosting layers");
        Register(SweetsType::Pudding, "Pudding", "Pudding.obj", "UV-mapped triangular pudding slice");
        Register(SweetsType::Sherbet, "Sherbet", "Sherbet.obj", "Shaded sherbet scoop with smooth normals");
        Register(SweetsType::Strawberry, "Strawberry", "Assets/Model/strawberry_low01.fbx", "Textured strawberry fruit with visible seeds");
        Register(SweetsType::Blueberry, "Blueberry", "Assets/Model/blueberry.fbx", "Stylized blueberry fruit with smooth shading");
        Register(SweetsType::Pineapple, "Pineapple", "Assets/Model/pineapple_high_fin.fbx", "High-detail pineapple with leafy crown");
        Register(SweetsType::Banana, "Banana", "Assets/Model/Banana_low01.fbx", "Low-poly curved banana fruit");
        Register(SweetsType::Fairy, "Fairy", "Fairy.obj", "Cross-plane fairy using embedded material data");
        RegisterAlias(SweetsType::Fairy, "Player");
    }

    void SweetsPrototypeManifest::Register(SweetsType type, std::string key, std::string relativePath, std::string description, bool allowMeshPhysics)
    {
        SweetsPrototypeDescriptor descriptor{};
        descriptor.key = std::move(key);
        descriptor.modelPath = ResolveModelPath(relativePath);
        descriptor.description = std::move(description);
        descriptor.allowMeshPhysics = allowMeshPhysics;

        const std::string normalized = NormalizeKey(descriptor.key);
        m_lookup[normalized] = type;
        m_byType[type] = std::move(descriptor);
    }

    void SweetsPrototypeManifest::RegisterAlias(SweetsType type, const std::string& aliasKey)
    {
        m_lookup[NormalizeKey(aliasKey)] = type;
    }

    bool SweetsPrototypeManifest::ReloadFromCsv(const std::string& csvPath)
    {
        m_byType.clear();
        m_lookup.clear();

        ApplyDefaults();

        const bool loaded = LoadOverridesFromCsv(csvPath);

        const std::array<SweetsType, 8> kTypes{
            SweetsType::Cake,
            SweetsType::Pudding,
            SweetsType::Sherbet,
            SweetsType::Strawberry,
            SweetsType::Blueberry,
            SweetsType::Pineapple,
            SweetsType::Banana,
            SweetsType::Fairy,
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

    bool SweetsPrototypeManifest::LoadOverridesFromCsv(const std::string& csvPath)
    {
        std::ifstream stream(csvPath);
        if (!stream.is_open())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Config not found at '" << csvPath << "'; using built-in defaults.\n";
            ManifestDebugTrace(oss.str());
            return false;
        }

        std::unordered_set<SweetsType, SweetsTypeHash> touched;
        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(stream, line))
        {
            ++lineNumber;
            const std::string trimmed = Trim(line);
            if (trimmed.empty() || trimmed[0] == '#')
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
                warn << kLogPrefix << "Line " << lineNumber << " malformed (needs key,modelPath[,description]).\n";
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
            std::optional<SweetsType> mappedType;
            const bool isPlayerAlias = normalizedKey == "player";
            if (normalizedKey == NormalizeKey(ToString(SweetsType::Cake))) mappedType = SweetsType::Cake;
            else if (normalizedKey == NormalizeKey(ToString(SweetsType::Pudding))) mappedType = SweetsType::Pudding;
            else if (normalizedKey == NormalizeKey(ToString(SweetsType::Sherbet))) mappedType = SweetsType::Sherbet;
            else if (normalizedKey == NormalizeKey(ToString(SweetsType::Strawberry))) mappedType = SweetsType::Strawberry;
            else if (normalizedKey == NormalizeKey(ToString(SweetsType::Blueberry))) mappedType = SweetsType::Blueberry;
            else if (normalizedKey == NormalizeKey(ToString(SweetsType::Pineapple))) mappedType = SweetsType::Pineapple;
            else if (normalizedKey == NormalizeKey(ToString(SweetsType::Banana))) mappedType = SweetsType::Banana;
            else if (normalizedKey == NormalizeKey(ToString(SweetsType::Fairy)) || isPlayerAlias) mappedType = SweetsType::Fairy;

            if (!mappedType)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "Unknown sweets key '" << key << "' on line " << lineNumber
                    << "; entry ignored.\n";
                ManifestDebugTrace(warn.str());
                continue;
            }

            const std::string resolvedPath = ResolveModelPath(modelPath);
            if (resolvedPath.empty())
            {
                std::ostringstream warn;
                warn << kLogPrefix << "Empty model path for key '" << key << "' on line " << lineNumber
                    << "; keeping previous entry.\n";
                ManifestDebugTrace(warn.str());
                continue;
            }

            if (description.empty())
            {
                const auto* existing = Find(*mappedType);
                description = existing ? existing->description : std::string{};
            }

            const std::string canonicalKey = ToString(*mappedType);
            const std::string registerKey = isPlayerAlias ? canonicalKey : key;

            Register(*mappedType, registerKey, resolvedPath, description, allowMeshPhysics);
            if (isPlayerAlias)
            {
                RegisterAlias(*mappedType, key);
            }
            touched.insert(*mappedType);

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
        case SweetsType::Strawberry:
            return "Strawberry";
        case SweetsType::Blueberry:
            return "Blueberry";
        case SweetsType::Pineapple:
            return "Pineapple";
        case SweetsType::Banana:
            return "Banana";
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
