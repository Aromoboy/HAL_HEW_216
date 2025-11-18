// ============================================================================
// 概要: ModelCache に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#ifndef	__MODELCACHE_H__
#define __MODELCACHE_H__

#include <map>
#include <memory>
#include <string>
#include "ModelDX.h"
#include "IModelLoader.h"

struct ID3D11Device;
class TextureCache;

namespace Render
{
    class ModelCache
    {
    public:
        static ModelCache& Instance();

        std::shared_ptr<ModelDX> Get(const std::string& path);
        void SetLoader(std::shared_ptr<IModelLoader> loader, ID3D11Device* device, TextureCache& textureCache);
        void Clear();

    private:
        ModelCache() = default;
        ~ModelCache() = default;
        ModelCache(const ModelCache&) = delete;
        ModelCache& operator=(const ModelCache&) = delete;

    private:
        std::shared_ptr<IModelLoader> m_loader;
        ID3D11Device* m_device = nullptr;
        TextureCache* m_textureCache = nullptr;
        std::map<std::string, std::shared_ptr<ModelDX>> m_models;
    };
}

#endif // 備考: __MODELCACHE_H__ に関する説明を日本語で補足している。