// ============================================================================
// 概要: Pdding の挙動や関連ロジックを実装し、ゲーム全体 に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: ゲーム全体 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#include "Pdding.h"

namespace SweetsSpace
{
    Pudding::Pudding(const std::string& tag,
                     std::shared_ptr<ModelDX> model,
                     const float3& pos,
                     const float3& rot,
                     const float3& scl,
                     std::shared_ptr<Collision::ICollider> collider,
                     const float3& velocity,
                     const float3& force,
                     float weight,
                     float damping,
                     Core::UpdatePhase phase,
                     int priority)
        : BaseSweets(tag, model, pos, rot, scl, collider, velocity, force,
          weight, damping, phase, priority)
    {}
}
