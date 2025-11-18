// ============================================================================
// 概要: IModelLoader に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once
#include <memory>
#include <string>

#include "ModelDX.h"

struct ID3D11Device;
class TextureCache;

class IModelLoader
{
public:
        IModelLoader() {};
        ~IModelLoader() = default;

        virtual std::shared_ptr<ModelDX> Load(const std::string& path, ID3D11Device* device, TextureCache& textureCache) = 0;
};
