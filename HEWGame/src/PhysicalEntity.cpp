// ============================================================================
// 概要: PhysicalEntity の挙動や関連ロジックを実装し、src に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: src 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

// 備考: PhysicalEntity.cpp に関する説明を日本語で補足している。
#include "PhysicalEntity.h"

namespace SceneSpace
{
    PhysicalEntity::PhysicalEntity(const std::string& tag,
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
        : CollisionEntity(tag, pos, rot, scl, std::move(collider), phase, priority)
        , m_physical(velocity, force, weight, damping)
    {
    }

    PhysicalEntity::~PhysicalEntity() = default;

    Physics::Physical& PhysicalEntity::GetPhysical()
    {
        return m_physical;
    }

    void TranslatePhysicalEntity(PhysicalEntity& entity, const float3& delta)
    {
        entity.CollisionEntity::ApplyTranslation(delta);
    }

}