#include "PlayerController.h"

#include "Core/Input.h"
#include "Entities/Player.h"
#include "PerspectiveCamera.h"
#include "MeteorsManager.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>

namespace
{
    constexpr float kPlayerSpeedBoostStep = 1.2f;
    constexpr float kMaxPlayerSpeedMultiplier = 2.0f;
    constexpr float kPlayerBoundsLogIntervalSeconds = 2.0f;
    constexpr float kCameraStateEpsilon = 1e-4f;
    constexpr bool kEnablePlayerBoundsLogging = false;

    bool HasMeaningfulDifference(float previous, float current, float epsilon = kCameraStateEpsilon)
    {
        if (!std::isfinite(previous) || !std::isfinite(current))
        {
            return true;
        }
        return std::fabs(previous - current) > epsilon;
    }
}

PlayerController::PlayerController(LogFn log, const char* logPrefix)
    : m_log(std::move(log))
    , m_logPrefix(logPrefix)
{
}

PlayerController::SpeedResult PlayerController::HandleSpeedInput(const SpeedInput& input) const
{
    SpeedResult result{ input.speedMultiplier };

    if (!input.player)
    {
        return result;
    }

    const auto adjustMoveSpeed = [&](float requestedMultiplier, const char* reason)
    {
        if (!(input.baseHorizontalSpeed > 0.0f) && !(input.baseVerticalSpeed > 0.0f))
        {
            std::ostringstream oss;
            oss << m_logPrefix << "[SpeedBoost] " << reason
                << " ignored; base speeds invalid (" << input.baseHorizontalSpeed
                << ',' << input.baseVerticalSpeed << ").\n";
            if (m_log)
            {
                m_log(oss.str());
            }
            return;
        }

        const float currentMultiplier = result.speedMultiplier;
        const float clampedMultiplier =
            std::clamp(requestedMultiplier, 0.0f, kMaxPlayerSpeedMultiplier);
        const bool clamped =
            std::fabs(clampedMultiplier - requestedMultiplier)
            > std::numeric_limits<float>::epsilon();

        if (std::fabs(currentMultiplier - clampedMultiplier)
            <= std::numeric_limits<float>::epsilon())
        {
            std::ostringstream oss;
            oss << m_logPrefix << "[SpeedBoost] " << reason
                << " requestedMultiplier=" << requestedMultiplier
                << " has no effect (current=" << currentMultiplier << ").\n";
            if (m_log)
            {
                m_log(oss.str());
            }
            return;
        }

        const float previousHorizontal = input.baseHorizontalSpeed * currentMultiplier;
        const float previousVertical = input.baseVerticalSpeed * currentMultiplier;

        const float newMultiplier = clampedMultiplier;
        result.speedMultiplier = newMultiplier;
        input.player->SetSpeedMultiplier(newMultiplier);

        const float newHorizontalSpeed = input.baseHorizontalSpeed * newMultiplier;
        const float newVerticalSpeed = input.baseVerticalSpeed * newMultiplier;

        std::ostringstream oss;
        oss << m_logPrefix << "[SpeedBoost] " << reason
            << " baseXY=(" << input.baseHorizontalSpeed << ',' << input.baseVerticalSpeed << ')'
            << " prevMultiplier=" << currentMultiplier
            << " newMultiplier=" << newMultiplier
            << " prevSpeedXY=(" << previousHorizontal << ',' << previousVertical << ')'
            << " newSpeedXY=(" << newHorizontalSpeed << ',' << newVerticalSpeed << ')'
            << " clamped=" << std::boolalpha << clamped << "\n";
        if (m_log)
        {
            m_log(oss.str());
        }
    };

    if (input.input.IsTrigger(Core::ActionId::SpeedBoost))
    {
        adjustMoveSpeed(result.speedMultiplier * kPlayerSpeedBoostStep, "Boost key '1'");
    }

    if (input.input.IsTrigger(Core::ActionId::SpeedReset))
    {
        adjustMoveSpeed(1.0f, "Reset key '2'");
    }

    return result;
}

bool PlayerController::HandleManualRetryInput(Core::Input& input, float elapsedSeconds, float peakHeight) const
{
    if (!input.IsTrigger(Core::ActionId::Retry))
    {
        return false;
    }

    std::ostringstream oss;
    oss << m_logPrefix << "[Countdown] Retry requested elapsed="
        << std::fixed << std::setprecision(2) << elapsedSeconds
        << "s peak=" << std::fixed << std::setprecision(2) << peakHeight << "m\n";
    if (m_log)
    {
        m_log(oss.str());
    }
    return true;
}

PlayerController::PostGameAction PlayerController::HandlePostGameInput(Core::Input& input, float peakHeight, int score) const
{
    if (input.IsTrigger(Core::ActionId::Confirm))
    {
        std::ostringstream oss;
        oss << m_logPrefix << "[Result] Confirm accepted peakTop=" << std::fixed << std::setprecision(2)
            << peakHeight << "m score=" << score << "\n";
        if (m_log)
        {
            m_log(oss.str());
        }
        return PostGameAction::ConfirmRestart;
    }

    if (input.IsTrigger(Core::ActionId::Retry))
    {
        std::ostringstream oss;
        oss << m_logPrefix << "[Result] Retry input ignored until confirm. peakTop="
            << std::fixed << std::setprecision(2) << peakHeight << "m\n";
        if (m_log)
        {
            m_log(oss.str());
        }
        return PostGameAction::RetryIgnored;
    }

    return PostGameAction::None;
}

void PlayerController::UpdatePlayerBounds(const BoundsInput& input)
{
    if (!input.player || !input.camera)
    {
        return;
    }

    if (input.meteorsManager)
    {
        input.meteorsManager->SetPlayerWorldPosition(input.player->GetTransform().position);
    }

    const float playerZ = input.player->GetTransform().position.z;
    const float3 camPos = input.camera->GetPosition();

    float d = std::fabs(camPos.z - playerZ);
    if (d < 0.001f)
    {
        d = 0.001f;
    }

    const float fovY = DirectX::XMConvertToRadians(input.camera->GetFieldOfViewDegrees());
    const float aspect = input.camera->GetAspectRatio();
    const float fovX = 2.0f * std::atan(std::tan(fovY * 0.5f) * aspect);

    const float halfWidth = d * std::tan(fovX * 0.5f);
    const float halfHeight = d * std::tan(fovY * 0.5f);

    const float minX = camPos.x - halfWidth;
    const float maxX = camPos.x + halfWidth;
    const float minY = camPos.y - halfHeight;
    const float maxY = camPos.y + halfHeight;

    const bool changed =
        !m_cachedPlayerBoundsValid ||
        HasMeaningfulDifference(m_cachedPlayerMinX, minX) ||
        HasMeaningfulDifference(m_cachedPlayerMaxX, maxX) ||
        HasMeaningfulDifference(m_cachedPlayerMinY, minY) ||
        HasMeaningfulDifference(m_cachedPlayerMaxY, maxY);

    if (kEnablePlayerBoundsLogging)
    {
        if (m_playerBoundsLogCooldownSeconds > 0.0f)
        {
            m_playerBoundsLogCooldownSeconds = std::max(0.0f, m_playerBoundsLogCooldownSeconds - input.deltaSeconds);
        }

        if (changed && m_playerBoundsLogCooldownSeconds <= 0.0f)
        {
            std::ostringstream oss;
            oss << "[SceneGame] [PlayerBounds] camX=" << camPos.x
                << " camY=" << camPos.y
                << " d=" << d
                << " fovX=" << DirectX::XMConvertToDegrees(fovX)
                << " halfWidth=" << halfWidth
                << " halfHeight=" << halfHeight
                << " => minX=" << minX
                << " maxX=" << maxX
                << " minY=" << minY
                << " maxY=" << maxY << "\n";
            if (m_log)
            {
                m_log(oss.str());
            }
            m_playerBoundsLogCooldownSeconds = kPlayerBoundsLogIntervalSeconds;
        }
    }

    input.player->SetScreenBounds(minX, maxX, minY, maxY);

    m_cachedPlayerMinX = minX;
    m_cachedPlayerMaxX = maxX;
    m_cachedPlayerMinY = minY;
    m_cachedPlayerMaxY = maxY;
    m_cachedPlayerBoundsValid = true;
}

bool PlayerController::TryGetCachedPlayerBounds(float& minX, float& maxX, float& minY, float& maxY) const
{
    if (!m_cachedPlayerBoundsValid)
    {
        return false;
    }

    minX = m_cachedPlayerMinX;
    maxX = m_cachedPlayerMaxX;
    minY = m_cachedPlayerMinY;
    maxY = m_cachedPlayerMaxY;
    return true;
}
