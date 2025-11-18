// ============================================================================
// 概要: Sherbet に関するインターフェースやデータ構造を宣言し、ゲーム全体 から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: ゲーム全体 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once
#include"Sweets.h"

namespace SweetsSpace
{
    class Sherbet : public BaseSweets
    {
    public:
        Sherbet(const std::string& tag,
                std::shared_ptr<ModelDX> model,
                const float3& pos,
                const float3& rot,
                const float3& scl,
                std::shared_ptr<Collision::ICollider> collider,
                const float3& velocity,
                const float3& force,
                float weight = 1.0f,
                float damping = 0.98f,
                Core::UpdatePhase phase = Core::UpdatePhase::Physics,
                int priority = 0);
    };
}
