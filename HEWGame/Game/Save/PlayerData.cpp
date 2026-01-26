#include "PlayerData.h"

#include <cerrno>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string_view>

#include "Debug.h"

namespace
{
	constexpr const char* kPlayerDataPath = "Assets/Data/PlayerData.cfg";
	constexpr const char* kLogPrefix = "[PlayerData] ";
	constexpr const char* kSectionHeader = "[PlayerData]";

	std::string_view TrimView(std::string_view value)
	{
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
		{
			value.remove_prefix(1);
		}
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
		{
			value.remove_suffix(1);
		}
		return value;
	}

	std::vector<std::string_view> SplitCsv(std::string_view value)
	{
		std::vector<std::string_view> result;
		std::size_t start = 0;
		while (start <= value.size())
		{
			const std::size_t comma = value.find(',', start);
			if (comma == std::string_view::npos)
			{
				result.push_back(TrimView(value.substr(start)));
				break;
			}
			result.push_back(TrimView(value.substr(start, comma - start)));
			start = comma + 1;
		}
		return result;
	}

	bool TryParseFloat(std::string_view token, float& outValue)
	{
		const std::string trimmed{ TrimView(token) };
		if (trimmed.empty())
		{
			return false;
		}

		char* end = nullptr;
		errno = 0;
		const float parsed = std::strtof(trimmed.c_str(), &end);
		if (end == trimmed.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite(parsed))
		{
			return false;
		}
		outValue = parsed;
		return true;
	}

	bool TryParseInt(std::string_view token, int& outValue)
	{
		const std::string trimmed{ TrimView(token) };
		if (trimmed.empty())
		{
			return false;
		}

		char* end = nullptr;
		errno = 0;
		const long parsed = std::strtol(trimmed.c_str(), &end, 10);
		if (end == trimmed.c_str() || *end != '\0' || errno == ERANGE)
		{
			return false;
		}
		outValue = static_cast<int>(parsed);
		return true;
	}

	bool TryParseBool(std::string_view token, bool& outValue)
	{
		std::string trimmed{ TrimView(token) };
		if (trimmed.empty())
		{
			return false;
		}
		for (char& ch : trimmed)
		{
			ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		}
		if (trimmed == "1" || trimmed == "true")
		{
			outValue = true;
			return true;
		}
		if (trimmed == "0" || trimmed == "false")
		{
			outValue = false;
			return true;
		}
		return false;
	}

	void LogParseError(std::size_t lineNumber, const std::string& message)
	{
		DebugLog::DebugMessage(std::string(kLogPrefix) + "Line " + std::to_string(lineNumber) + ": " + message + "\n");
	}
}

bool PlayerData::LoadData()
{
	m_entries.clear();

	std::ifstream file(kPlayerDataPath);
	if (!file.is_open())
	{
		DebugLog::DebugMessage(std::string(kLogPrefix) + "Failed to open '" + kPlayerDataPath + "'.\n");
		return false;
	}

	bool inSection = false;
	bool hadError = false;
	bool anyEntry = false;
	std::string line;
	std::size_t lineNumber = 0;

	while (std::getline(file, line))
	{
		++lineNumber;
		const std::string_view trimmed = TrimView(line);
		if (trimmed.empty() || trimmed.starts_with('#') || trimmed.starts_with(';'))
		{
			continue;
		}

		if (trimmed.front() == '[')
		{
			inSection = (trimmed == kSectionHeader);
			continue;
		}

		if (!inSection)
		{
			continue;
		}

		if (!trimmed.starts_with("entry"))
		{
			continue;
		}

		const std::size_t equalsPos = trimmed.find('=');
		if (equalsPos == std::string_view::npos)
		{
			LogParseError(lineNumber, "Missing '=' for entry.");
			hadError = true;
			continue;
		}

		const std::string_view payload = TrimView(trimmed.substr(equalsPos + 1));
		if (payload.empty())
		{
			LogParseError(lineNumber, "Entry payload is empty.");
			hadError = true;
			continue;
		}

		const std::vector<std::string_view> fields = SplitCsv(payload);
		if (fields.size() != 7)
		{
			LogParseError(lineNumber, "Entry must have 7 fields.");
			hadError = true;
			continue;
		}

		Data entry{};
		if (!TryParseFloat(fields[0], entry.fSpeed)
			|| !TryParseBool(fields[1], entry.bCream)
			|| !TryParseFloat(fields[2], entry.fCakeSize)
			|| !TryParseFloat(fields[3], entry.fReplenishSpeed)
			|| !TryParseInt(fields[4], entry.nReplenishCount)
			|| !TryParseFloat(fields[5], entry.fPtMultipl)
			|| !TryParseInt(fields[6], entry.nPoints))
		{
			LogParseError(lineNumber, "Failed to parse entry values.");
			hadError = true;
			continue;
		}

		m_entries.push_back(entry);
		anyEntry = true;
	}

	if (!anyEntry)
	{
		DebugLog::DebugMessage(std::string(kLogPrefix) + "No player data entries found.\n");
	}

	return !hadError;
}

bool PlayerData::SaveData() const
{
	std::ofstream out(kPlayerDataPath, std::ios::trunc);
	if (!out.is_open())
	{
		DebugLog::DebugMessage(std::string(kLogPrefix) + "Failed to write '" + kPlayerDataPath + "'.\n");
		return false;
	}

	out << kSectionHeader << "\n";
	for (const auto& entry : m_entries)
	{
		out << "entry="
			<< entry.fSpeed << ","
			<< (entry.bCream ? 1 : 0) << ","
			<< entry.fCakeSize << ","
			<< entry.fReplenishSpeed << ","
			<< entry.nReplenishCount << ","
			<< entry.fPtMultipl << ","
			<< entry.nPoints << "\n";
	}

	return true;
}

const std::vector<PlayerData::Data>& PlayerData::GetEntries() const
{
	return m_entries;
}

void PlayerData::SetEntries(const std::vector<Data>& entries)
{
	m_entries = entries;
}

void PlayerData::SetEntries(std::vector<Data>&& entries)
{
	m_entries = std::move(entries);
}
