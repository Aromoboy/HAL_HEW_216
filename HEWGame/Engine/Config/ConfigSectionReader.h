// 設定ファイルの指定区画を読み取る
// 各種設定ローダーがこの読み取りを使う

#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <vector>

namespace Config
{
    struct SectionLine
    {
        std::string key;
        std::string value;
        std::size_t lineNumber{ 0 };
    };

    inline std::string Trim(const std::string& value)
    {
        auto begin = value.begin();
        while (begin != value.end() && std::isspace(static_cast<unsigned char>(*begin)))
        {
            ++begin;
        }

        auto end = value.end();
        while (end != begin)
        {
            auto prev = end;
            --prev;
            if (!std::isspace(static_cast<unsigned char>(*prev)))
            {
                break;
            }
            end = prev;
        }

        return std::string(begin, end);
    }

    inline std::string ToLowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    inline std::string NormalizeSectionName(const std::string& value)
    {
        return ToLowerCopy(Trim(value));
    }

    inline void StripInlineComment(std::string& line)
    {
        const auto firstNonSpace = std::find_if(line.begin(), line.end(), [](unsigned char c)
        {
            return !std::isspace(c);
        });
        if (firstNonSpace == line.end())
        {
            return;
        }

        if (*firstNonSpace == ';')
        {
            line = line.substr(0, static_cast<std::size_t>(std::distance(line.begin(), firstNonSpace)));
            return;
        }

        const auto commentPos = line.find('#');
        if (commentPos != std::string::npos)
        {
            line = line.substr(0, commentPos);
        }
    }

    inline bool ReadSectionLines(const std::string& path, const std::string& section, std::vector<SectionLine>& outLines)
    {
        std::ifstream file(path);
        if (!file)
        {
            return false;
        }

        const std::string targetSection = NormalizeSectionName(section);
        std::string currentSection;
        std::string line;
        std::size_t lineNumber = 0;

        while (std::getline(file, line))
        {
            ++lineNumber;
            StripInlineComment(line);
            line = Trim(line);
            if (line.empty())
            {
                continue;
            }

            if (line.front() == '[' && line.back() == ']')
            {
                const std::string sectionName = line.substr(1, line.size() - 2);
                currentSection = NormalizeSectionName(sectionName);
                continue;
            }

            if (!targetSection.empty() && currentSection != targetSection)
            {
                continue;
            }

            const auto eq = line.find('=');
            if (eq == std::string::npos)
            {
                continue;
            }

            SectionLine entry{};
            entry.key = Trim(line.substr(0, eq));
            entry.value = Trim(line.substr(eq + 1));
            entry.lineNumber = lineNumber;
            outLines.push_back(std::move(entry));
        }

        return !outLines.empty();
    }
}
