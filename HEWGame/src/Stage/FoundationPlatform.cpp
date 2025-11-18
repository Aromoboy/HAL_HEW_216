// ============================================================================
// 概要: FoundationPlatform の挙動や関連ロジックを実装し、src／Stage に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: src／Stage 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#include "Stage/FoundationPlatform.h"

#include <sstream>
#include <string>
#include <utility>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    void DebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr const char* kLogPrefix = "[FoundationPlatform] ";
}

namespace Stage
{
    FoundationPlatform::FoundationPlatform(const std::string& tag,
        const Transform& transform,
        std::shared_ptr<Collision::ICollider> collider,
        Core::UpdatePhase phase,
        int priority)
        : SceneSpace::PhysicalEntity(tag,
            transform.position,
            transform.rotation,
            transform.scale,
            std::move(collider),
            float3{ 0.0f, 0.0f, 0.0f },
            float3{ 0.0f, 0.0f, 0.0f },
            0.0f,
            1.0f,
            phase,
            priority)
        , m_anchorTransform(transform)
    {
        SyncColliderBounds();
        DebugTrace(std::string(kLogPrefix) + "Initialized foundation platform.\n");
    }

    void FoundationPlatform::SetAnchorTransform(const Transform& transform)
    {
        m_anchorTransform = transform;
        auto& current = GetTransform();
        current = m_anchorTransform;
        SyncColliderBounds();

#if defined(_DEBUG)
        std::ostringstream oss;
        oss << kLogPrefix << "Anchor transform updated for platform '" << GetTag() << "'.\n";
        DebugTrace(oss.str());
#endif
    }

    const Transform& FoundationPlatform::GetAnchorTransform() const noexcept
    {
        return m_anchorTransform;
    }

    void FoundationPlatform::OnUpdate(Core::UpdateContext& ctx)
    {
        (void)ctx;
        auto& physical = GetPhysical();
        physical.velocity = float3{ 0.0f, 0.0f, 0.0f };
        physical.force = float3{ 0.0f, 0.0f, 0.0f };

        auto& current = GetTransform();
        if (current.position.x != m_anchorTransform.position.x
            || current.position.y != m_anchorTransform.position.y
            || current.position.z != m_anchorTransform.position.z
            || current.rotation.x != m_anchorTransform.rotation.x
            || current.rotation.y != m_anchorTransform.rotation.y
            || current.rotation.z != m_anchorTransform.rotation.z
            || current.scale.x != m_anchorTransform.scale.x
            || current.scale.y != m_anchorTransform.scale.y
            || current.scale.z != m_anchorTransform.scale.z)
        {
            current = m_anchorTransform;
            SyncColliderBounds();
        }
    }
}
