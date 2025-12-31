// スイーツ種別のキー変換をまとめている
// 設定読み込みや生成処理から参照される

#pragma once

#include <string>
#include <cctype>

namespace SweetsSpace
{
    inline std::string NormalizeKey(const std::string& key)
    {
        std::string normalized;
        normalized.reserve(key.size());
        for (unsigned char ch : key)
        {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        }
        return normalized;
    }
}
