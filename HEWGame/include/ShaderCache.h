#ifndef __SHADERCACHE_H__
#define __SHADERCACHE_H__

#include <d3d11.h>
#include <map>
#include <memory>
#include <string>
#include "ShaderProgram.h"

//============================================================
// ShaderCache - シェーダー管理
//============================================================
class ShaderCache {
public:
    // シングルトンインスタンスを取得
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

    ShaderProgram& Load(const std::string& name, ID3D11Device* device, const std::string& vsPath, const std::string& psPath);
    ShaderProgram& Get(const std::string& name);

private:
    // コンストラクタをprivateに
    ShaderCache() = default;
    ~ShaderCache() = default;

    std::map<std::string, std::unique_ptr<ShaderProgram>> programs;
};
#endif //__SHADERCACHE_H__