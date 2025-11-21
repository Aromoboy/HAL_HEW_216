#include "Stage/FoundationPlatform.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>

#include "MaterialDX.h"
#include "ModelCache.h"
#include "ModelDX.h"

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
    constexpr const char* kPlatformModelPath = "Assets/Model/SweetsPrototype/CakeCube.obj";
    const DirectX::XMFLOAT4 kDefaultPlatformColor{ 0.2f, 0.8f, 0.35f, 1.0f };

    DirectX::XMFLOAT4 SanitizeColor(const DirectX::XMFLOAT4& color)
    {
        auto clampComponent = [](float value)
        {
            return std::clamp(value, 0.0f, 1.0f);
        };
        return DirectX::XMFLOAT4(
            clampComponent(color.x),
            clampComponent(color.y),
            clampComponent(color.z),
            clampComponent(color.w));
    }
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
            Collision::CollisionLayer::Floor,
            float3{ 0.0f, 0.0f, 0.0f },
            float3{ 0.0f, 0.0f, 0.0f },
            0.0f,
            1.0f,
            phase,
            priority)
        , m_anchorTransform(transform)
        , m_model(nullptr)
        , m_debugColor(kDefaultPlatformColor)
    {
        SyncColliderBounds();
        LoadPlatformModel();
        SetDebugColor(kDefaultPlatformColor);
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

    ModelDX* FoundationPlatform::GetRenderableModel()
    {
        return m_model.get();
    }

    const ModelDX* FoundationPlatform::GetRenderableModel() const
    {
        return m_model.get();
    }

    void FoundationPlatform::SetDebugColor(const DirectX::XMFLOAT4& color)
    {
        const DirectX::XMFLOAT4 sanitized = SanitizeColor(color);
        m_debugColor = sanitized;
        ApplyColorToModelMaterials();

        std::ostringstream oss;
        oss << kLogPrefix << "Debug color set to RGBA("
            << m_debugColor.x << ',' << m_debugColor.y << ',' << m_debugColor.z << ',' << m_debugColor.w << ")";
        if (!m_model)
        {
            oss << " but model not yet available.";
        }
        oss << "\n";
        DebugTrace(oss.str());
    }

    const DirectX::XMFLOAT4& FoundationPlatform::GetDebugColor() const noexcept
    {
        return m_debugColor;
    }

    void FoundationPlatform::LoadPlatformModel()
    {
        Render::ModelCache& cache = Render::ModelCache::Instance();
        m_model = cache.Get(kPlatformModelPath);
        if (!m_model)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Failed to load render model at '" << kPlatformModelPath << "'.\n";
            DebugTrace(oss.str());
            return;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "Loaded platform model from '" << kPlatformModelPath
            << "' (meshes=" << m_model->GetMeshCount() << ").\n";
        DebugTrace(oss.str());
    }

    void FoundationPlatform::ApplyColorToModelMaterials()
    {
        if (!m_model)
        {
            return;
        }

        const unsigned int meshCount = m_model->GetMeshCount();
        unsigned int appliedCount = 0;
        unsigned int skippedMeshes = 0;
        for (unsigned int meshIndex = 0; meshIndex < meshCount; ++meshIndex)
        {
            auto mesh = m_model->GetMesh(static_cast<int>(meshIndex)).lock();
            if (!mesh)
            {
                ++skippedMeshes;
                continue;
            }

            const int materialIndex = static_cast<int>(mesh->GetMaterialID());
            auto material = m_model->GetMaterial(materialIndex).lock();
            if (!material)
            {
                ++skippedMeshes;
                continue;
            }

            material->SetDiffuse(m_debugColor);
            DirectX::XMFLOAT4 ambient = m_debugColor;
            ambient.w = 1.0f;
            material->SetAmbient(ambient);
            ++appliedCount;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "Applied debug color to " << appliedCount << '/' << meshCount
            << " materials (" << skippedMeshes << " skipped).\n";
        DebugTrace(oss.str());
    }
}
