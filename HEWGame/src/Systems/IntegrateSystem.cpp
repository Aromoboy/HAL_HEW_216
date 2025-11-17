#include "Systems/IntegrateSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>

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

    constexpr const char* kLogPrefix = "[IntegrateSystem] ";
}

namespace Systems
{
    IntegrateSystem::IntegrateSystem()
        : m_translationEpsilon(1e-4f)
        , m_verboseLogging(true)
    {
    }

    void IntegrateSystem::SetVerboseLoggingEnabled(bool enabled)
    {
        m_verboseLogging = enabled;
    }

    bool IntegrateSystem::IsVerboseLoggingEnabled() const
    {
        return m_verboseLogging;
    }

    float3 IntegrateSystem::Integrate(SceneSpace::PhysicalEntity& entity, float dt)
    {
        float3 translation{ 0.0f, 0.0f, 0.0f };
        if (!(dt > 0.0f))
        {
            return translation;
        }

        Physics::Physical& physical = entity.GetPhysical();
        if (!(physical.weight > 0.0f))
        {
            return translation;
        }

        const float safeDt = std::max(dt, 0.0f);
        translation.x = physical.velocity.x * safeDt;
        translation.y = physical.velocity.y * safeDt;
        translation.z = physical.velocity.z * safeDt;

        const float damping = std::clamp(physical.damping, 0.0f, 1.0f);
        translation.x *= damping;
        translation.y *= damping;
        translation.z *= damping;

        const float invWeight = (physical.weight > std::numeric_limits<float>::epsilon()) ? 1.0f / physical.weight : 0.0f;
        if (invWeight <= 0.0f)
        {
            return float3{ 0.0f, 0.0f, 0.0f };
        }

        translation.x *= invWeight;
        translation.y *= invWeight;
        translation.z *= invWeight;

        const bool hasTranslation = (std::fabs(translation.x) > m_translationEpsilon)
            || (std::fabs(translation.y) > m_translationEpsilon)
            || (std::fabs(translation.z) > m_translationEpsilon);
        if (!hasTranslation)
        {
            return float3{ 0.0f, 0.0f, 0.0f };
        }

        SceneSpace::TranslatePhysicalEntity(entity, translation);

#if defined(_DEBUG)
        if (m_verboseLogging)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Entity '" << entity.GetTag()
                << "' velocity=(" << physical.velocity.x << ',' << physical.velocity.y << ',' << physical.velocity.z << ')'
                << " dt=" << safeDt
                << " translation=(" << translation.x << ',' << translation.y << ',' << translation.z << ")\n";
            DebugTrace(oss.str());
        }
#endif

        return translation;
    }
}
