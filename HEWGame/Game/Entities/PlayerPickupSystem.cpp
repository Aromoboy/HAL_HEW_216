#include "Entities/PlayerPickupSystem.h"

#include "AABB.h"
#include "Core/Input.h"
#include "Entities/Player.h"
#include "Entities/PlayerModel.h"
#include "Sweets/BaseSweets.h"
#include "SweetsManager.h"
#include "SoundManagerEX.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

#include <box2d/box2d.h>
#include <box2d/math_functions.h>

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

    constexpr float kCarryVerticalOffset = 1.0f;
    constexpr float kCarryForwardOffset = 0.5f;

    constexpr int kDefaultMaxCarry = 3;
    constexpr float kDefaultPickupHalfExtent = 0.5f;
    constexpr float kDefaultPickupMaxDistance = 0.5f;
    constexpr float kDefaultPickupDepthThreshold = std::numeric_limits<float>::infinity();

    const char* PickupOffsetModeName(Stage::PickupOffsetMode mode)
    {
        switch (mode)
        {
        case Stage::PickupOffsetMode::None: return "none";
        case Stage::PickupOffsetMode::FacingDirection: return "facing-direction";
        case Stage::PickupOffsetMode::WorldX: return "world-x";
        default: return "unknown";
        }
    }

    const char* PickupResultName(SweetsSpace::PickupResult result)
    {
        switch (result)
        {
        case SweetsSpace::PickupResult::Success: return "success";
        case SweetsSpace::PickupResult::Failure: return "failure";
        default: return "unknown";
        }
    }

    const char* PickupFailReasonName(SweetsSpace::PickupFailReason reason)
    {
        switch (reason)
        {
        case SweetsSpace::PickupFailReason::None: return "none";
        case SweetsSpace::PickupFailReason::MissingSweetsManager: return "missing-sweets-manager";
        case SweetsSpace::PickupFailReason::MissingSystems: return "missing-systems";
        case SweetsSpace::PickupFailReason::InvalidConfiguration: return "invalid-configuration";
        case SweetsSpace::PickupFailReason::NoCandidates: return "no-candidates";
        case SweetsSpace::PickupFailReason::DistanceExceeded: return "distance-exceeded";
        case SweetsSpace::PickupFailReason::DepthExceeded: return "depth-exceeded";
        case SweetsSpace::PickupFailReason::StateTransitionFailed: return "state-transition-failed";
        default: return "unknown";
        }
    }
}

namespace Entities
{
    PlayerPickupSystem::PlayerPickupSystem(SweetsSpace::SweetsManager* manager)
        : m_sweetsManager(manager)
    {
        m_maxCarry = kDefaultMaxCarry;
        m_pickupMaxDistance = kDefaultPickupMaxDistance;
        m_pickupDepthThreshold = kDefaultPickupDepthThreshold;
        m_carryOffset = float3{ 0.0f, kCarryVerticalOffset, kCarryForwardOffset };
        m_lastCarryInputLoggedCount = m_carrieds.size();
    }

    void PlayerPickupSystem::SetSweetsManager(SweetsSpace::SweetsManager* manager)
    {
        if (m_sweetsManager == manager)
        {
            return;
        }

        if (!manager && !m_carrieds.empty())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "保持中に SweetsManager が解放されました 強制的にリリースします \n");
            m_carrieds.clear();
            m_carriedRotations.clear();
        }

        m_sweetsManager = manager;
    }

    bool PlayerPickupSystem::IsCarryingSweets() const
    {
        return !m_carrieds.empty();
    }

    const std::vector<std::shared_ptr<SweetsSpace::BaseSweets>>& PlayerPickupSystem::GetCarriedSweets() const
    {
        return m_carrieds;
    }

    void PlayerPickupSystem::ClearCarriedSweets()
    {
        if (m_carrieds.empty())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "保持中のスイーツの参照をクリアします \n");
        }
        m_carrieds.clear();
        m_carriedRotations.clear();
    }

    void PlayerPickupSystem::SetPickupBox(float width, float height)
    {
        SetPickupArea(width, height, m_pickupOffset);
    }

    void PlayerPickupSystem::SetPickupArea(float width, float height, const Stage::PickupOffset& offset)
    {
        if (!std::isfinite(width) || !std::isfinite(height)
            || !std::isfinite(offset.localOffset.x)
            || !std::isfinite(offset.localOffset.y)
            || !std::isfinite(offset.localOffset.z))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限のピックアップボックス指定を無視しました \n");
            return;
        }

        Stage::PickupOffset sanitizedOffset = offset;
        switch (sanitizedOffset.mode)
        {
        case Stage::PickupOffsetMode::None:
        case Stage::PickupOffsetMode::FacingDirection:
        case Stage::PickupOffsetMode::WorldX:
            break;
        default:
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "未知のピックアップオフセットモードを WorldX にフォールバックします \n");
            sanitizedOffset.mode = Stage::PickupOffsetMode::WorldX;
            break;
        }

        const float sanitizedWidth = std::max(0.0f, width);
        const float sanitizedHeight = std::max(0.0f, height);
        const float halfWidth = sanitizedWidth * 0.5f;
        const float halfHeight = sanitizedHeight * 0.5f;

        const bool unchanged = std::fabs(m_pickupHalfWidth - halfWidth) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_pickupHalfHeight - halfHeight) < std::numeric_limits<float>::epsilon()
            && m_pickupOffset.mode == sanitizedOffset.mode
            && std::fabs(m_pickupOffset.localOffset.x - sanitizedOffset.localOffset.x) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_pickupOffset.localOffset.y - sanitizedOffset.localOffset.y) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_pickupOffset.localOffset.z - sanitizedOffset.localOffset.z) < std::numeric_limits<float>::epsilon();
        if (unchanged)
        {
            return;
        }

        m_pickupHalfWidth = halfWidth;
        m_pickupHalfHeight = halfHeight;
        m_pickupOffset = sanitizedOffset;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "ピックアップボックスを width=" << sanitizedWidth
            << " height=" << sanitizedHeight << " (half=" << m_pickupHalfWidth
            << ',' << m_pickupHalfHeight << ") offsetMode=" << PickupOffsetModeName(m_pickupOffset.mode)
            << " localOffset=(" << m_pickupOffset.localOffset.x << ',' << m_pickupOffset.localOffset.y
            << ',' << m_pickupOffset.localOffset.z << ") に設定しました \n";
        PlayerDebugTrace(oss.str());
    }

    void PlayerPickupSystem::SetPickupMaxDistance(float maxDistance)
    {
        if (!std::isfinite(maxDistance) || maxDistance <= 0.0f)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "無効なピックアップ距離を無視しました \n");
            return;
        }

        if (std::fabs(m_pickupMaxDistance - maxDistance) < std::numeric_limits<float>::epsilon())
        {
            return;
        }

        m_pickupMaxDistance = maxDistance;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "ピックアップ最大距離を " << m_pickupMaxDistance << " に設定しました \n";
        PlayerDebugTrace(oss.str());
    }

    void PlayerPickupSystem::SetPickupDepthThreshold(float depthThreshold)
    {
        if (!std::isfinite(depthThreshold) || depthThreshold < 0.0f)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "無効なピックアップ深度しきい値を無視しました \n");
            return;
        }

        if (std::fabs(m_pickupDepthThreshold - depthThreshold) < std::numeric_limits<float>::epsilon())
        {
            return;
        }

        m_pickupDepthThreshold = depthThreshold;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "ピックアップ深度しきい値を " << m_pickupDepthThreshold << " に設定しました \n";
        PlayerDebugTrace(oss.str());
    }

    void PlayerPickupSystem::SetPickupOriginOffset(const float3& offset)
    {
        if (!std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(offset.z))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限のピックアップ基準オフセットを無視しました \n");
            return;
        }

        const bool unchanged = std::fabs(m_pickupOriginOffset.x - offset.x) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_pickupOriginOffset.y - offset.y) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_pickupOriginOffset.z - offset.z) < std::numeric_limits<float>::epsilon();
        if (unchanged)
        {
            return;
        }

        m_pickupOriginOffset = offset;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "ピックアップ基準を transform オフセット(" << m_pickupOriginOffset.x
            << ',' << m_pickupOriginOffset.y << ',' << m_pickupOriginOffset.z << ") で更新しました \n";
        PlayerDebugTrace(oss.str());
    }

    void PlayerPickupSystem::SetColliderFootprint(const float3& halfExtents, const float3& centerOffset)
    {
        m_colliderFootprint = ColliderFootprint{ halfExtents, centerOffset };
    }

    void PlayerPickupSystem::SetFacingSign(float facingSign)
    {
        m_facingSign = facingSign;
    }

    const SweetsSpace::PickupQueryResult& PlayerPickupSystem::GetLastPickupResult() const noexcept
    {
        return m_lastPickupResult;
    }

    float3 PlayerPickupSystem::ComputeCarryAnchor(Player& player) const
    {
        if (player.HasBox2DBody())
        {
            const float3 position = Player::ToWorldPosition(b2Body_GetPosition(player.GetBox2DBodyId()));
            return float3{ position.x + m_carryOffset.x, position.y + m_carryOffset.y, position.z + m_carryOffset.z };
        }

        return float3{
            player.m_transform.position.x + m_carryOffset.x,
            player.m_transform.position.y + m_carryOffset.y,
            player.m_transform.position.z + m_carryOffset.z };
    }

    PlayerPickupSystem::PickupOrigin PlayerPickupSystem::ComputePickupOrigin(Player& player) const
    {
        PickupOrigin origin{};

        const float3 basePosition = player.HasBox2DBody()
            ? Player::ToWorldPosition(b2Body_GetPosition(player.GetBox2DBodyId()))
            : player.m_transform.position;

        origin.center = float3{ basePosition.x + m_pickupOriginOffset.x, basePosition.y + m_pickupOriginOffset.y, basePosition.z + m_pickupOriginOffset.z };
        origin.halfDepth = 0.05f;
        origin.source = "TransformOffset";

        if (m_colliderFootprint)
        {
            origin.center = float3
            {
                basePosition.x + m_colliderFootprint->centerOffset.x,
                basePosition.y + m_colliderFootprint->centerOffset.y,
                basePosition.z + m_colliderFootprint->centerOffset.z,
            };
            origin.halfDepth = std::max(0.05f, m_colliderFootprint->halfExtents.z + 0.05f);
            origin.source = "ColliderFootprint";
        }

        return origin;
    }

    void PlayerPickupSystem::UpdateCarriedSweets(Player& player)
    {
        if (m_carrieds.empty())
        {
            return;
        }

        for (auto& it : m_carrieds)
        {
            const Transform current = it ? it->GetTransform() : Transform{};
            Transform updated = current;
            updated.position = ComputeCarryAnchor(player);

            const auto rotationIt = m_carriedRotations.find(it.get());
            if (rotationIt != m_carriedRotations.end())
            {
                updated.rotation = rotationIt->second;
            }

            it->SetTransform(updated);
            it->SetLinearVelocity({ 0.0f, 0.0f, 0.0f });
        }
    }

    void PlayerPickupSystem::HandleCarryInput(Player& player, PlayerModel& model, const Core::UpdateContext& ctx, Core::Input& input)
    {
        const bool dropTriggered = input.IsTrigger(Core::ActionId::Drop);

        const bool carriedChanged = m_carrieds.size() != m_lastCarryInputLoggedCount;
        const bool shouldLogEntry = carriedChanged || dropTriggered;

        if (shouldLogEntry)
        {
            std::ostringstream oss;
            oss << kPlayerLogPrefix << "HandleCarryInput enter carried=" << m_carrieds.size()
                << "/" << m_maxCarry
                << " dropTrigger=" << dropTriggered
                << "\n";
            PlayerDebugTrace(oss.str());

            m_lastCarryInputLoggedCount = m_carrieds.size();
        }

        if (dropTriggered)
        {
            std::ostringstream oss;
            oss << kPlayerLogPrefix << "Drop evaluated carried=" << m_carrieds.size()
                << " maxCarry=" << m_maxCarry << "\n";
            PlayerDebugTrace(oss.str());
        }

        if (dropTriggered && !m_carrieds.empty())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "DropCarriedSweets invoked\n");
            DropCarriedSweets(player);
            return;
        }
    }

    void PlayerPickupSystem::TryPickupNearest(Player& player)
    {
        SweetsSpace::PickupQueryResult pickupResult{};
        std::shared_ptr<SweetsSpace::ActiveSweets> candidate;
        Transform candidateTransform{};
        AABB sweetsBounds{};
        float sweetsZ = std::numeric_limits<float>::quiet_NaN();

        PickupOrigin pickupOrigin{};
        float3 pickupCenter = pickupOrigin.center;
        float halfDepth = pickupOrigin.halfDepth;
        float depthThreshold = m_pickupDepthThreshold;
        bool pickupVolumeValid = false;

        const auto clampPickupExtent = [](float value)
        {
            const float sanitized = std::isfinite(value) ? value : kDefaultPickupHalfExtent;
            return std::max(sanitized, kDefaultPickupHalfExtent);
        };

        const auto clampPickupDistance = [](float value)
        {
            const float sanitized = std::isfinite(value) ? value : kDefaultPickupMaxDistance;
            return std::max(sanitized, kDefaultPickupMaxDistance);
        };

        const float pickupHalfWidth = clampPickupExtent(m_pickupHalfWidth);
        const float pickupHalfHeight = clampPickupExtent(m_pickupHalfHeight);
        const float pickupMaxDistance = clampPickupDistance(m_pickupMaxDistance);

        if (!m_sweetsManager)
        {
            pickupResult.failReason = SweetsSpace::PickupFailReason::MissingSweetsManager;
        }
        else if (!(pickupHalfWidth > 0.0f) || !(pickupHalfHeight > 0.0f) || !(pickupMaxDistance > 0.0f))
        {
            pickupResult.failReason = SweetsSpace::PickupFailReason::InvalidConfiguration;
        }
        else
        {
            pickupOrigin = ComputePickupOrigin(player);
            halfDepth = pickupOrigin.halfDepth;
            depthThreshold = m_pickupDepthThreshold;

            float3 pickupOffset{ 0.0f, 0.0f, 0.0f };
            switch (m_pickupOffset.mode)
            {
            case Stage::PickupOffsetMode::FacingDirection:
                pickupOffset.x = m_pickupOffset.localOffset.x * m_facingSign;
                pickupOffset.y = m_pickupOffset.localOffset.y;
                pickupOffset.z = m_pickupOffset.localOffset.z;
                break;

            case Stage::PickupOffsetMode::WorldX:
                pickupOffset = m_pickupOffset.localOffset;
                break;

            case Stage::PickupOffsetMode::None:
            default:
                break;
            }

            pickupCenter = float3
            {
                pickupOrigin.center.x + pickupOffset.x,
                pickupOrigin.center.y + pickupOffset.y,
                pickupOrigin.center.z + pickupOffset.z
            };

            const bool depthThresholdValid = std::isfinite(depthThreshold)
                ? depthThreshold >= 0.0f
                : std::isinf(depthThreshold);

            pickupVolumeValid = std::isfinite(halfDepth) && halfDepth > 0.0f
                && depthThresholdValid;

            if (!pickupVolumeValid)
            {
                pickupResult.failReason = SweetsSpace::PickupFailReason::InvalidConfiguration;

                std::ostringstream oss;
                oss << kPlayerLogPrefix << "ピックアップボリュームが無効です halfDepth=" << halfDepth
                    << " depthThreshold=" << depthThreshold << "\n";
                PlayerDebugTrace(oss.str());
            }
            else
            {
                pickupResult = m_sweetsManager->FindNearestCarryable(
                    pickupCenter,
                    pickupHalfWidth,
                    pickupHalfHeight,
                    halfDepth,
                    depthThreshold,
                    m_carrieds,
                    pickupMaxDistance);

                candidate = pickupResult.sweets;
                if (pickupResult.Succeeded())
                {
                    candidateTransform = candidate->GetTransform();
                    sweetsBounds = candidate->GetAABB();
                    sweetsZ = candidateTransform.position.z;

                    if (!m_sweetsManager->TryMarkAsCarried(candidate))
                    {
                        pickupResult.result = SweetsSpace::PickupResult::Failure;
                        pickupResult.failReason = SweetsSpace::PickupFailReason::StateTransitionFailed;
                        candidate.reset();
                    }
                    else
                    {
                        const float3 originalRotation = candidateTransform.rotation;
                        m_carriedRotations[candidate.get()] = originalRotation;

                        Transform updated = candidateTransform;
                        updated.position = ComputeCarryAnchor(player);
                        updated.rotation = originalRotation;
                        candidate->SetTransformViaPhysics(updated.position, updated.rotation);
                        candidate->SetLinearVelocity({ 0.0f, 0.0f, 0.0f });

                        m_carrieds.push_back(candidate);
                    }
                }
                else
                {
                    std::shared_ptr<SweetsSpace::ActiveSweets> nearest;
                    float nearestDistanceSq = std::numeric_limits<float>::max();

                    const auto evaluateList = [&](const SweetsSpace::SweetsManager::SweetsList& list)
                    {
                        for (const auto& sweets : list)
                        {
                            if (!sweets || sweets->IsPendingDestroy())
                            {
                                continue;
                            }

                            if (std::find(m_carrieds.begin(), m_carrieds.end(), sweets) != m_carrieds.end())
                            {
                                continue;
                            }

                            const Transform sweetsTransform = sweets->GetTransform();
                            const float dx = sweetsTransform.position.x - pickupCenter.x;
                            const float dy = sweetsTransform.position.y - pickupCenter.y;
                            const float distanceSq = (dx * dx) + (dy * dy);
                            if (distanceSq < nearestDistanceSq)
                            {
                                nearestDistanceSq = distanceSq;
                                nearest = sweets;
                            }
                        }
                    };

                    evaluateList(m_sweetsManager->GetByState(SweetsSpace::SweetsState::Ground));
                    evaluateList(m_sweetsManager->GetByState(SweetsSpace::SweetsState::Active));
                    evaluateList(m_sweetsManager->GetByState(SweetsSpace::SweetsState::Stacked));

                    if (nearest)
                    {
                        const float nearestDistance = std::sqrt(nearestDistanceSq);
                        pickupResult.distance = nearestDistance;

                        pickupResult.result = SweetsSpace::PickupResult::Success;
                        pickupResult.failReason = SweetsSpace::PickupFailReason::None;
                        candidate = nearest;
                        candidateTransform = candidate->GetTransform();
                        sweetsBounds = candidate->GetAABB();
                        sweetsZ = candidateTransform.position.z;

                        if (!m_sweetsManager->TryMarkAsCarried(candidate))
                        {
                            pickupResult.result = SweetsSpace::PickupResult::Failure;
                            pickupResult.failReason = SweetsSpace::PickupFailReason::StateTransitionFailed;
                            candidate.reset();
                        }
                        else
                        {
                            const float3 originalRotation = candidateTransform.rotation;
                            m_carriedRotations[candidate.get()] = originalRotation;

                            Transform updated = candidateTransform;
                            updated.position = ComputeCarryAnchor(player);
                            updated.rotation = originalRotation;
                            candidate->SetTransformViaPhysics(updated.position, updated.rotation);
                            candidate->SetLinearVelocity({ 0.0f, 0.0f, 0.0f });

                            m_carrieds.push_back(candidate);
                        }
                    }
                }
            }
        }

        m_lastPickupResult = pickupResult;

        // ピックアップ音を再生
        if (auto* sound = SoundManagerEX::GetInstance())
        {
            sound->PlaySoundEX("Assets/Audio/SE_Catch.wav", false, 0.5f);
        }

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "ピックアップ結果 result=" << PickupResultName(pickupResult.result)
            << " reason=" << PickupFailReasonName(pickupResult.failReason)
            << " boxHalf=(" << pickupHalfWidth << ',' << pickupHalfHeight << ")"
            << " offsetMode=" << PickupOffsetModeName(m_pickupOffset.mode)
            << " offsetLocal=(" << m_pickupOffset.localOffset.x << ',' << m_pickupOffset.localOffset.y
            << ',' << m_pickupOffset.localOffset.z << ")"
            << " maxDistance=" << pickupMaxDistance;

        if (pickupVolumeValid)
        {
            oss << " origin=" << pickupOrigin.source
                << " originCenter=(" << pickupOrigin.center.x << ',' << pickupOrigin.center.y << ',' << pickupOrigin.center.z << ')'
                << " originHalfDepth=" << halfDepth
                << " depthThreshold=" << depthThreshold
                << " pickupCenterZ=" << pickupCenter.z;
        }

        if (candidate)
        {
            oss << " tag='" << candidate->GetTag() << "'"
                << " sweetsZ=" << sweetsZ
                << " sweetsAABBZ=[" << sweetsBounds.min.z << ',' << sweetsBounds.max.z << ']';
        }
        if (std::isfinite(pickupResult.distance))
        {
            oss << " distance=" << pickupResult.distance;
        }
        if (std::isfinite(pickupResult.depthDelta))
        {
            oss << " depthDelta=" << pickupResult.depthDelta;
        }
        oss << "\n";
        PlayerDebugTrace(oss.str());
    }

    void PlayerPickupSystem::DropCarriedSweets(Player& player)
    {
        if (m_carrieds.empty())
        {
            return;
        };

        auto sweets = m_carrieds.back();
        sweets->SetPositionViaPhysics(ComputeCarryAnchor(player));

        m_carriedRotations.erase(sweets.get());

        const float3 bodyVelocity = player.GetLinearVelocity();
        float3 releaseVelocity = bodyVelocity;
        releaseVelocity.y = std::max(releaseVelocity.y, m_dropUpwardBoost);
        sweets->SetLinearVelocity(releaseVelocity);

        if (m_sweetsManager)
        {
            if (!m_sweetsManager->ReleaseCarriedSweets(sweets))
            {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "ドロップ警告: マネージャーが解放を拒否しました \n");
            }
        }
        else
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "ドロップ警告: SweetsManager がありません \n");
        }

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "ドロップ成功 tag='" << sweets->GetTag()
            << "' velocityY=" << sweets->GetLinearVelocity().y << "\n";
        PlayerDebugTrace(oss.str());

        m_carrieds.pop_back();

        // ドロップ音を再生
        if (auto* sound = SoundManagerEX::GetInstance())
        {
            sound->PlaySoundEX("Assets/Audio/SE_FruitFall.wav", false, 0.5f);
        }
    }
}
