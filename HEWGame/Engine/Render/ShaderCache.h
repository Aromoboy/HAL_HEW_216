// シェーダ資源を共有する保管庫をまとめている
// 描画初期化と描画処理から参照される

#ifndef __SHADERCACHE_H__
#define __SHADERCACHE_H__

#include <d3d11.h>
#include <map>
#include <memory>
#include <string>
#include "ShaderProgram.h"

//============================================================
// ShaderCache - VF[_[Ǘ
//============================================================
class ShaderCache {
public:
    // VOgCX^X擾
    static ShaderCache& Instance()
    {
        static ShaderCache instance;
        return instance;
    }

    // Rs[E[u֎~
    ShaderCache(const ShaderCache&) = delete;
    ShaderCache& operator=(const ShaderCache&) = delete;
    ShaderCache(ShaderCache&&) = delete;
    ShaderCache& operator=(ShaderCache&&) = delete;

    std::shared_ptr<ShaderProgram> Load(const std::string& name, ID3D11Device* device, const std::string& vsPath, const std::string& psPath);
    std::shared_ptr<ShaderProgram> Get(const std::string& name);

private:
    // RXgN^private
    ShaderCache() = default;
    ~ShaderCache() = default;

    std::map<std::string, std::shared_ptr<ShaderProgram>> programs;
};
#endif //__SHADERCACHE_H__