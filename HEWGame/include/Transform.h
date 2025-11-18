// ============================================================================
// 概要: Transform に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#ifndef ___TRANSFORM_H___
#define ___TRANSFORM_H___

#include "Vector.h"

namespace Components
{
        struct Transform
        {
                float3 position;
                float3 rotation;
                float3 scale;
        };
}

using Transform = Components::Transform;

#endif
