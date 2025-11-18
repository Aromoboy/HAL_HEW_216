// ============================================================================
// 概要: FoundationPlatform に関するインターフェースやデータ構造を宣言し、include／Stage から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include／Stage 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once

#include <memory>
#include <string>

#include "PhysicalEntity.h"
#include "Transform.h"

namespace Core
{
    struct UpdateContext;
}

namespace Stage
{
    class FoundationPlatform : public SceneSpace::PhysicalEntity
    {
    public:
        FoundationPlatform(const std::string& tag,
            const Transform& transform,
            std::shared_ptr<Collision::ICollider> collider,
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0);

        void SetAnchorTransform(const Transform& transform);
        const Transform& GetAnchorTransform() const noexcept;

        void OnUpdate(Core::UpdateContext& ctx) override;

    private:
        Transform m_anchorTransform;
    };
}
