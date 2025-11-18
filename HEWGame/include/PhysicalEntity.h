// ============================================================================
// 概要: PhysicalEntity に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

// 備考: PhysicalEntity.h に関する説明を日本語で補足している。

#pragma once

#include <memory>

#include "CollisionEntity.h"
#include "Physical.h"

namespace SceneSpace
{
    class PhysicalEntity : public CollisionEntity
    {
    public:
        PhysicalEntity(const std::string& tag,
            const float3& pos,
            const float3& rot,
            const float3& scl,
            std::shared_ptr<Collision::ICollider> collider,
            const float3& velocity,
            const float3& force,
            float weight = 1.0f,
            float damping = 0.98f,
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0);

        ~PhysicalEntity() override;

        Physics::Physical& GetPhysical();

    protected:
        Physics::Physical m_physical;
    };

    // PhysicalEntity は Collision に加えて Physical（速度・力・質量など）を持つ
    // Entity → CollisionEntity → PhysicalEntity の階層に統一
    // Transform と一緒に初期化できるようにした
    // PhysicsMachine がこの Physical を直接操作する
    // そのため参照で返している

    void TranslatePhysicalEntity(PhysicalEntity& entity, const float3& delta);
} // 備考: namespace SceneSpace に関する説明を日本語で補足している。
