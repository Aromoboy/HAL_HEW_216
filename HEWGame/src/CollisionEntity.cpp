// CollisionEntity.cpp
#include "CollisionEntity.h"

#include <sstream>

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
}

namespace SceneSpace
{
    CollisionEntity::CollisionEntity(const std::string& tag,
        const float3& pos,
        const float3& rot,
        const float3& scl,
        std::shared_ptr<Collision::ICollider> collider,
        Core::UpdatePhase phase,
        int priority)
        : Entity(tag, pos, rot, scl, phase, priority)
        , m_collider(std::move(collider))
    {
        SyncColliderBounds();
    }

    CollisionEntity::~CollisionEntity() = default;

    Collision::ICollider* CollisionEntity::GetCollider()
    {
        return m_collider.get();
    }

    void CollisionEntity::SyncColliderBounds()
    {
        if (!m_collider)
        {
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << "[CollisionEntity] SyncColliderBounds skipped for entity '" << m_tag
                << "' because collider is null.\n";
            DebugTrace(oss.str());
#endif
            return;
        }

        m_collider->UpdateBounds(m_transform);
    }
}
