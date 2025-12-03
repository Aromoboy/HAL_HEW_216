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
        Register(SweetsType::Fairy, "Fairy", "Fairy.obj", "Procedural cross-plane fairy (Fairy.obj kept as fallback)");
    }

    void SweetsPrototypeManifest::Register(SweetsType type, std::string key, std::string relativePath, std::string description)
    {
        SweetsPrototypeDescriptor descriptor{};
        descriptor.key = std::move(key);
        descriptor.modelPath = ResolveModelPath(relativePath);
        descriptor.description = std::move(description);

        const std::string normalized = NormalizeKey(descriptor.key);
        m_lookup[normalized] = type;
        m_byType[type] = std::move(descriptor);
    }

    bool SweetsPrototypeManifest::ReloadFromCsv(const std::string& csvPath)
    {
        m_byType.clear();
        m_lookup.clear();

        ApplyDefaults();

        const bool loaded = LoadOverridesFromCsv(csvPath);

        const std::array<SweetsType, 4> kTypes{ SweetsType::Cake, SweetsType::Pudding, SweetsType::Sherbet, SweetsType::Fairy };
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
                std::ostringstream oss;
                oss << kLogPrefix << "Lookup map restored for type '" << ToString(type) << "' key='"
                    << m_byType[type].key << "'.\n";
                ManifestDebugTrace(oss.str());
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

        std::ostringstream banner;
        banner << kLogPrefix << "Loading sweets manifest overrides from '" << csvPath << "'.\n";
        ManifestDebugTrace(banner.str());

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
            std::string description;
            if (tokens.size() >= 3)
            {
                description = tokens[2];
                for (std::size_t i = 3; i < tokens.size(); ++i)
                {
                    description.append(",");
                    description.append(tokens[i]);
                }
            }

            const std::string normalizedKey = NormalizeKey(key);
            const std::string normalizedModelHeader = NormalizeKey(modelPath);
            if (normalizedKey == "key" && normalizedModelHeader == "modelpath")
            {
                ManifestDebugTrace(std::string(kLogPrefix) + "CSV header detected; skipping line.\n");
                continue;
            }
            std::optional<SweetsType> mappedType;
            if (normalizedKey == NormalizeKey(ToString(SweetsType::Cake))) mappedType = SweetsType::Cake;
            else if (normalizedKey == NormalizeKey(ToString(SweetsType::Pudding))) mappedType = SweetsType::Pudding;
            else if (normalizedKey == NormalizeKey(ToString(SweetsType::Sherbet))) mappedType = SweetsType::Sherbet;
            else if (normalizedKey == NormalizeKey(ToString(SweetsType::Fairy)) || normalizedKey == "player") mappedType = SweetsType::Fairy;

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

            Register(*mappedType, key, resolvedPath, description);
            touched.insert(*mappedType);

            std::ostringstream info;
            info << kLogPrefix << "Registered/overrode type '" << ToString(*mappedType)
                << "' with key='" << key << "' path='" << resolvedPath << "'.\n";
            ManifestDebugTrace(info.str());
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

        for (SweetsType type : { SweetsType::Cake, SweetsType::Pudding, SweetsType::Sherbet, SweetsType::Fairy })
        {
            if (touched.find(type) == touched.end())
            {
                std::ostringstream warn;
                warn << kLogPrefix << "No CSV override for type '" << ToString(type)
                    << "'; using built-in descriptor path='" << m_byType[type].modelPath << "'.\n";
                ManifestDebugTrace(warn.str());
            }
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
