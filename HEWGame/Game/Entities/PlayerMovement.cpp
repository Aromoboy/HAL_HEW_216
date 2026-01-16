#include "Entities/PlayerMovement.h"

#include "Core/Input.h"
#include "Entities/Player.h"
#include <algorithm>
#include <cmath>
#include <limits>
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
    inline void PlayerDebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    inline void PlayerDebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr const char* kPlayerLogPrefix = "[Player] ";

    float3 Add(const float3& a, const float3& b)
    {
        return float3{ a.x + b.x, a.y + b.y, a.z + b.z };
    }

    float3 Subtract(const float3& a, const float3& b)
    {
        return float3{ a.x - b.x, a.y - b.y, a.z - b.z };
    }

    float3 Scale(const float3& v, float s)
    {
        return float3{ v.x * s, v.y * s, v.z * s };
    }

    float Length(const float3& v)
    {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    float3 MoveTowards(const float3& current, const float3& target, float maxDelta)
    {
        if (!(maxDelta > 0.0f) || !std::isfinite(maxDelta))
        {
            return current;
        }

        const float3 delta = Subtract(target, current);
        const float distance = Length(delta);
        if (distance <= maxDelta || distance <= std::numeric_limits<float>::epsilon())
        {
            return target;
        }

        const float scale = maxDelta / distance;
        return Add(current, Scale(delta, scale));
    }

    float MoveTowards(float current, float target, float maxDelta)
    {
        if (!(maxDelta > 0.0f) || !std::isfinite(maxDelta))
        {
            return current;
        }

        const float delta = target - current;
        if (std::fabs(delta) <= maxDelta)
        {
            return target;
        }

        return current + (delta > 0.0f ? maxDelta : -maxDelta);
    }

    float Lerp(float start, float end, float t)
    {
        return start + (end - start) * t;
    }

    float3 ClampSpeed(const float3& velocity, float maxSpeed)
    {
        if (!(maxSpeed > 0.0f) || !std::isfinite(maxSpeed))
        {
            return velocity;
        }

        const float speed = Length(velocity);
        if (speed <= maxSpeed || speed <= std::numeric_limits<float>::epsilon())
        {
            return velocity;
        }

        const float scale = maxSpeed / speed;
        return Scale(velocity, scale);
    }

    constexpr float kTwoPi = 6.283185307179586f;

    float WrapRadians(float angle)
    {
        if (!std::isfinite(angle))
        {
            return 0.0f;
        }

        return std::remainder(angle, kTwoPi);
    }

    float TurnTowards(float current, float target, float maxDelta)
    {
        if (!(maxDelta > 0.0f) || !std::isfinite(maxDelta))
        {
            return current;
        }

        const float delta = std::remainder(target - current, kTwoPi);
        if (std::fabs(delta) <= maxDelta)
        {
            return target;
        }

        return current + (delta > 0.0f ? maxDelta : -maxDelta);
    }

    float3 ForwardFromYaw(float yaw)
    {
        return float3{ std::cos(yaw), std::sin(yaw), 0.0f };
    }

    struct YawAxisInfo
    {
        float yaw = 0.0f;
        bool useYAxis = false;
    };

    YawAxisInfo ResolveYawAxis(const float3& rotation)
    {
        constexpr float kYawAxisEpsilon = 1.0e-4f;
        const float absYawZ = std::fabs(rotation.z);
        const float absYawY = std::fabs(rotation.y);
        const bool useYAxis = absYawY > absYawZ + kYawAxisEpsilon;
        return YawAxisInfo{ useYAxis ? rotation.y : rotation.z, useYAxis };
    }
}

namespace Entities
{
    void PlayerMovement::Update(Player& player, const Core::UpdateContext& ctx, Core::Input& input)
    {
        const bool hasBody = player.HasBox2DBody();
        if (!hasBody && !m_missingBodyLogged)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) +
                "[Fatal] Missing Box2D body; falling back to transform-based update.\n");
            m_missingBodyLogged = true;
        }

        if (!m_hasBaseRotation)
        {
            m_baseRotation = player.GetTransform().rotation;
            const auto resolvedYaw = ResolveYawAxis(m_baseRotation);
            m_baseYaw = resolvedYaw.yaw;
            m_useYawAxisY = resolvedYaw.useYAxis;
            m_hasBaseRotation = true;
            m_currentHeading = 0.0f;
            m_hasCurrentHeading = true;
        }

        const float rawHorizontal = input.GetAxis(Core::ActionId::MoveX);
        const float rawVertical = input.GetAxis(Core::ActionId::RotateYaw);
        if (!m_hasSmoothedInput)
        {
            m_smoothedInputX = rawHorizontal;
            m_smoothedInputY = rawVertical;
            m_hasSmoothedInput = true;
        }

        float horizontal = rawHorizontal;
        float vertical = rawVertical;
        if (ctx.dt > 0.0f && m_inputSmoothing > 0.0f)
        {
            const float smoothingFactor = 1.0f - std::exp(-ctx.dt * m_inputSmoothing);
            m_smoothedInputX = Lerp(m_smoothedInputX, rawHorizontal, smoothingFactor);
            m_smoothedInputY = Lerp(m_smoothedInputY, rawVertical, smoothingFactor);
            horizontal = m_smoothedInputX;
            vertical = m_smoothedInputY;
        }
        else
        {
            m_smoothedInputX = rawHorizontal;
            m_smoothedInputY = rawVertical;
        }

        if (horizontal > 0.0f)      m_facingSign = 1.0f;
        else if (horizontal < 0.0f) m_facingSign = -1.0f;

        const float effectiveHorizontalSpeed = m_baseHorizontalMoveSpeed * m_speedMultiplier;
        const float effectiveVerticalSpeed = m_baseVerticalMoveSpeed * m_speedMultiplier;

        if (!m_hasCurrentHeading)
        {
            const float currentYaw = (m_useYawAxisY ? player.GetTransform().rotation.y : player.GetTransform().rotation.z)
                - m_yawOffsetRadians;
            m_currentHeading = currentYaw - m_baseYaw;
            m_currentHeading = WrapRadians(m_currentHeading);
            m_hasCurrentHeading = true;
        }

        const float inputSq = (horizontal * horizontal) + (vertical * vertical);
        const bool hasInput = inputSq > std::numeric_limits<float>::epsilon();

        const float desiredHeading = hasInput ? std::atan2(vertical, horizontal) : m_currentHeading;

        if (ctx.dt > 0.0f)
        {
            if (hasInput)
            {
                m_currentHeading = TurnTowards(m_currentHeading, desiredHeading, m_turnRate * ctx.dt);
                m_currentHeading = WrapRadians(m_currentHeading);
            }

            const float headingDelta = std::remainder(desiredHeading - m_currentHeading, kTwoPi);
            const float turnIntensity = hasInput
                ? std::clamp(headingDelta / m_rollInputRange, -1.0f, 1.0f)
                : 0.0f;
            const float targetRoll = turnIntensity * m_rollMaxRadians;
            m_currentRoll = MoveTowards(m_currentRoll, targetRoll, m_rollResponse * ctx.dt);

            const float3 desiredInput{
                horizontal * effectiveHorizontalSpeed,
                vertical * effectiveVerticalSpeed,
                0.0f
            };
            const float desiredSpeed = Length(desiredInput);
            const float3 forward = ForwardFromYaw(m_currentHeading);
            const float3 desired = Scale(forward, desiredSpeed);

            float3 updatedRotation = player.GetTransform().rotation;
            updatedRotation.x = m_baseRotation.x + m_currentRoll;
            const float yawWithOffset = m_baseYaw + m_currentHeading + m_yawOffsetRadians;
            if (m_useYawAxisY)
            {
                updatedRotation.y = yawWithOffset;
                updatedRotation.z = m_baseRotation.z;
            }
            else
            {
                updatedRotation.z = yawWithOffset;
                updatedRotation.y = m_baseRotation.y;
            }

            if (hasInput)
            {
                float3 targetRotation = updatedRotation;

                if (hasBody)
                {
                    player.SetRotation(updatedRotation);
                    const float3 currentPosition = player.GetTransform().position;
                    player.SetTransformViaPhysics(currentPosition, targetRotation);
                }
                else
                {
                    player.SetRotation(targetRotation);
                }
            }
            else
            {
                player.SetRotation(updatedRotation);
            }

            if (effectiveHorizontalSpeed > 0.0f || effectiveVerticalSpeed > 0.0f)
            {
                const float3 currentVelocity = hasBody ? player.GetLinearVelocity() : m_fallbackVelocity;
                const float acceleration = hasInput ? m_accel : m_brake;
                const float maxDelta = acceleration * ctx.dt;
                float3 nextVelocity = MoveTowards(currentVelocity, desired, maxDelta);
                if (m_maxSpeed > 0.0f)
                {
                    nextVelocity = ClampSpeed(nextVelocity, m_maxSpeed);
                }

                if (hasBody)
                {
                    player.SetLinearVelocity(nextVelocity);
                }
                else
                {
                    m_fallbackVelocity = nextVelocity;
                    player.ApplyTranslation(Scale(nextVelocity, ctx.dt));
                }
            }
        }
        else
        {
            if (hasBody)
            {
                player.SetLinearVelocity({ 0.0f, 0.0f, 0.0f });
            }
            else
            {
                m_fallbackVelocity = { 0.0f, 0.0f, 0.0f };
            }
        }
    }

    void PlayerMovement::ConfigureMovement(float accel, float brake, float turnRate, float rollResponse, float inputSmoothing, float maxSpeed)
    {
        auto applySetting = [&](float value, float& target, const char* label) -> bool
        {
            if (!std::isfinite(value))
            {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限の移動設定を無視しました: " + label + "\n");
                return false;
            }

            const float sanitized = std::max(0.0f, value);
            if (std::fabs(target - sanitized) < std::numeric_limits<float>::epsilon())
            {
                return false;
            }

            target = sanitized;
            return true;
        };

        bool updated = false;
        updated = applySetting(accel, m_accel, "accel") || updated;
        updated = applySetting(brake, m_brake, "brake") || updated;
        updated = applySetting(turnRate, m_turnRate, "turnRate") || updated;
        updated = applySetting(rollResponse, m_rollResponse, "rollResponse") || updated;
        updated = applySetting(inputSmoothing, m_inputSmoothing, "inputSmoothing") || updated;
        updated = applySetting(maxSpeed, m_maxSpeed, "maxSpeed") || updated;

        if (updated)
        {
            std::ostringstream oss;
            oss << kPlayerLogPrefix << "移動設定を更新しました "
                << " accel=" << m_accel
                << " brake=" << m_brake
                << " turnRate=" << m_turnRate
                << " rollResponse=" << m_rollResponse
                << " smoothing=" << m_inputSmoothing
                << " maxSpeed=" << m_maxSpeed << "\n";
            PlayerDebugTrace(oss.str());
        }
    }

    void PlayerMovement::SetMoveSpeed(float speed)
    {
        SetMoveSpeeds(speed, speed);
    }

    void PlayerMovement::SetMoveSpeeds(float horizontal, float vertical)
    {
        SetBaseMoveSpeed(horizontal, vertical);
        SetSpeedMultiplier(1.0f);
    }

    void PlayerMovement::SetBaseMoveSpeed(float horizontal, float vertical)
    {
        if (!std::isfinite(horizontal) || !std::isfinite(vertical))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限の移動速度指定を無視しました \n");
            return;
        }

        const float sanitizedHorizontal = std::max(0.0f, horizontal);
        const float sanitizedVertical = std::max(0.0f, vertical);

        if (sanitizedHorizontal != horizontal || sanitizedVertical != vertical)
        {
            std::ostringstream oss;
            oss << kPlayerLogPrefix << "移動速度を (" << horizontal << ',' << vertical
                << ") から (" << sanitizedHorizontal << ',' << sanitizedVertical << ") に正規化しました（負値は不可） \n";
            PlayerDebugTrace(oss.str());
        }

        const bool unchanged = std::fabs(m_baseHorizontalMoveSpeed - sanitizedHorizontal) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_baseVerticalMoveSpeed - sanitizedVertical) < std::numeric_limits<float>::epsilon();
        if (unchanged)
        {
            return;
        }

        m_baseHorizontalMoveSpeed = sanitizedHorizontal;
        m_baseVerticalMoveSpeed = sanitizedVertical;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "基準移動速度を (" << m_baseHorizontalMoveSpeed
            << ", " << m_baseVerticalMoveSpeed << ") units/sec に設定しました \n";
        PlayerDebugTrace(oss.str());
    }

    void PlayerMovement::SetSpeedMultiplier(float multiplier)
    {
        if (!std::isfinite(multiplier))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限の速度倍率指定を無視しました \n");
            return;
        }

        const float sanitizedMultiplier = std::max(0.0f, multiplier);
        const bool unchanged = std::fabs(m_speedMultiplier - sanitizedMultiplier) < std::numeric_limits<float>::epsilon();
        if (unchanged)
        {
            return;
        }

        m_speedMultiplier = sanitizedMultiplier;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "速度倍率を " << m_speedMultiplier << "x に設定しました \n";
        PlayerDebugTrace(oss.str());
    }

    void PlayerMovement::SetYawOffsetRadians(float yawOffsetRadians)
    {
        if (!std::isfinite(yawOffsetRadians))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限のヨーオフセット指定を無視しました \n");
            return;
        }

        if (std::fabs(m_yawOffsetRadians - yawOffsetRadians) < std::numeric_limits<float>::epsilon())
        {
            return;
        }

        m_yawOffsetRadians = yawOffsetRadians;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "ヨーオフセットを " << m_yawOffsetRadians << " rad に設定しました \n";
        PlayerDebugTrace(oss.str());
    }

    float PlayerMovement::GetMoveSpeed() const
    {
        return GetHorizontalMoveSpeed();
    }

    float PlayerMovement::GetHorizontalMoveSpeed() const
    {
        return m_baseHorizontalMoveSpeed * m_speedMultiplier;
    }

    float PlayerMovement::GetVerticalMoveSpeed() const
    {
        return m_baseVerticalMoveSpeed * m_speedMultiplier;
    }

    float PlayerMovement::GetFacingSign() const
    {
        return m_facingSign;
    }
}
