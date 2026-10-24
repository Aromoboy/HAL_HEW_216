#include "Entities/PlayerMovement.h"

#include "Core/Input.h"
#include "Entities/Player.h"
#include "Entities/PlayerModel.h"
#include "SoundManagerEX.h"
#include "EffectSystem.h"

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
}

namespace Entities
{
    void PlayerMovement::Update(Player& player, PlayerModel& a_modelSystem, const Core::UpdateContext& ctx, Core::Input& input)
    {
        const bool hasBody = player.HasBox2DBody();
        if (!hasBody && !m_missingBodyLogged)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) +
                "[Fatal] Missing Box2D body; falling back to transform-based update.\n");
            m_missingBodyLogged = true;
        }

        const float horizontal = input.GetAxis(Core::ActionId::MoveX);
        const float vertical = input.GetAxis(Core::ActionId::RotateYaw);

        if (horizontal > 0.0f)      m_facingSign = 1.0f;
        else if (horizontal < 0.0f) m_facingSign = -1.0f;

        const float effectiveHorizontalSpeed = m_baseHorizontalMoveSpeed * m_speedMultiplier;
        const float effectiveVerticalSpeed = m_baseVerticalMoveSpeed * m_speedMultiplier;

        bool isMovingNow = false;

        if (ctx.dt > 0.0f && (effectiveHorizontalSpeed > 0.0f || effectiveVerticalSpeed > 0.0f))
        {
            if (horizontal != 0.0f || vertical != 0.0f)
            {
                isMovingNow = true;

                const float3 desired{
                    horizontal * effectiveHorizontalSpeed,
                    vertical * effectiveVerticalSpeed,
                    0.0f
                };


                if (horizontal != 0.0f)
                {
                    player.SetRotation(float3(0.0f, 90.0f * m_facingSign, 0.0f));

                }

                if (hasBody)
                    player.SetLinearVelocity(desired);
                else
                    player.ApplyTranslation(float3{
                        desired.x * ctx.dt,
                        desired.y * ctx.dt,
                        desired.z * ctx.dt
                        });
                a_modelSystem.PlayMove(player);

                if (m_effectCollTime <= 0.0f)
                {
                    m_effectCollTime = m_maxEffectCoolTime;
                    m_effectSystem->SpawnEffect(EffectSystem::EffectCreateRequest("fly", player.GetTransform().position, float3()));
                }
                else
                {
                    m_effectCollTime -= ctx.dt;
                }
            }
            else 
            {
                a_modelSystem.PlayIdle(player);

                if (hasBody)
                {
                    player.SetLinearVelocity({ 0.0f, 0.0f, 0.0f });
                }
            }

            
        }
        else if (hasBody)
        {
            player.SetLinearVelocity({ 0.0f, 0.0f, 0.0f });
        }
        
        

        // ===== 移動音（フェードアウト付き）=====
        if (auto* sound = SoundManagerEX::GetInstance())
        {
            if (isMovingNow && !m_wasMovingLastFrame)
            {
                if (m_moveSoundFadingOut)
                {
                    m_moveSoundFadingOut = false;
                    sound->SetVolume(m_moveSoundHandle, 0.3f);
                }
                else
                {
                    m_moveSoundHandle = sound->PlaySoundEX(
                        "Assets/Audio/SE_Move.wav", true, 0.3f);
                }
            }
            else if (!isMovingNow && m_wasMovingLastFrame && !m_moveSoundFadingOut)
            {
                if (m_moveSoundHandle != -1)
                {
                    m_moveSoundFadingOut = true;
                    m_moveSoundFadeTimer = 0.0f;
                }
            }

            if (m_moveSoundFadingOut && m_moveSoundHandle != -1)
            {
                m_moveSoundFadeTimer += ctx.dt;
                if (m_moveSoundFadeTimer >= m_moveSoundFadeDuration)
                {
                    sound->StopSound(m_moveSoundHandle);
                    m_moveSoundHandle = -1;
                    m_moveSoundFadingOut = false;
                    m_moveSoundFadeTimer = 0.0f;
                }
                else
                {
                    const float t = m_moveSoundFadeTimer / m_moveSoundFadeDuration;
                    sound->SetVolume(m_moveSoundHandle, 0.3f * (1.0f - t));
                }
            }
        }
        m_wasMovingLastFrame = isMovingNow;
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

    void PlayerMovement:: SetEffectSystem(EffectSystem* system)
    {
        m_effectSystem = system;
    }

    void PlayerMovement::SetMaxEffectCoolTime(float maxTime)
    {
        m_maxEffectCoolTime = maxTime;
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
