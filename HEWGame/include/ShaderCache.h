// ============================================================================
// Tv: ShaderCache ɊւC^[tF[Xf[^\錾Ainclude ėpł悤ɂB
// ړI: ߂ẴW[ɐGJ҂łAӖƈˑ֌Wfcł悤ɍx̐t^ĂB
// Ag: include z̑t@CVXewƘA邽߁At@CԂ̊֌WǐՂۂ̖ڈƂėpłB
// ============================================================================

#ifndef __SHADERCACHE_H__
#define __SHADERCACHE_H__

#include <d3d11.h>
#include <map>
#include <memory>
#include <string>
#include "ShaderProgram.h"

//============================================================
#endif // l: __SHADERCACHE_H__ Ɋւ{ŕ⑫ĂB
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

    std::shared_ptr<ShaderProgram> Load(const std::string& name, ID3D11Device* device, const std::string& vsPath, const std::string& psPath);
    std::shared_ptr<ShaderProgram> Get(const std::string& name);

private:
    // コンストラクタをprivateに
    ShaderCache() = default;
    ~ShaderCache() = default;

    std::map<std::string, std::shared_ptr<ShaderProgram>> programs;
};
#endif //__SHADERCACHE_H__