// ============================================================================
// 概要: IntegrateSystem に関するインターフェースやデータ構造を宣言し、include／Systems から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include／Systems 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once

#include "PhysicalEntity.h"

#include <cstddef>

namespace Systems
{
    class IntegrateSystem
    {
    public:
        IntegrateSystem();
        ~IntegrateSystem() = default;

        void SetVerboseLoggingEnabled(bool enabled);
        bool IsVerboseLoggingEnabled() const;

        float3 Integrate(SceneSpace::PhysicalEntity& entity, float dt);

    private:
        float m_translationEpsilon;
        bool m_verboseLogging;
    };
}
