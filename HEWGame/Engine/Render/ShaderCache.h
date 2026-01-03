#ifndef __SHADERCACHE_H__
#define __SHADERCACHE_H__

#include <d3d11.h>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include "ShaderProgram.h"

class ShaderCache {
public:
    static ShaderCache& Instance()
    {
        static ShaderCache instance;
        return instance;
    }

    // コピー・ムーブを禁止
    ShaderCache(const ShaderCache&) = delete;
    ShaderCache& operator=(const ShaderCache&) = delete;
    ShaderCache(ShaderCache&&) = delete;
    ShaderCache& operator=(ShaderCache&&) = delete;

    std::shared_ptr<ShaderProgram> Load(const std::string& name,
        ID3D11Device* device,
        const std::filesystem::path& vsPath,
        const std::filesystem::path& psPath);
    std::shared_ptr<ShaderProgram> Get(const std::string& name);

private:
    // コンストラクタをprivateに
    ShaderCache() = default;
    ~ShaderCache() = default;

    std::map<std::string, std::shared_ptr<ShaderProgram>> programs;
};
#endif //__SHADERCACHE_H__
