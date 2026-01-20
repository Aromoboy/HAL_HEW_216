#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include <box2d/box2d.h>

#include "Entities/PhysicalEntity.h"
#include "Systems/Physics/Box2DMeshBuilder.h"
#include "SweetsPrototypeManifest.h"
#include "Vector.h"

namespace SweetsSpace
{
    class BaseSweets;
    using ActiveSweets = BaseSweets;

    struct WeightedSweetsType
    {
        SweetsType kind{ SweetsType::Cream };
        float weight = 1.0f;
    };

    struct SpawnTuningConfig
    {
        float spawnIntervalSeconds = 2.5f;
        float burstIntervalSeconds = 0.5f;
        int simultaneousSpawnCount = 1;
        int simultaneousLiveTarget = 1;
        std::vector<WeightedSweetsType> weights;
    };

    enum class ColliderShape : std::uint8_t
    {
        Box,
        Capsule,
        Circle,
    };

    struct ColliderPreset
    {
        ColliderShape shape{ ColliderShape::Box };
        float3 padding{ 0.0f, 0.0f, 0.0f };
    };

    enum class DeleteReason : std::uint8_t
    {
        Hit,
        Miss,
    };

    enum class UseOutcome : std::uint8_t
    {
        Hit,
        Miss,
    };

    enum class PickupResult : std::uint8_t
    {
        Success,
        Failure,
    };

    enum class PickupFailReason : std::uint8_t
    {
        None,
        MissingSweetsManager,
        MissingSystems,
        InvalidConfiguration,
        NoCandidates,
        DistanceExceeded,
        DepthExceeded,
        StateTransitionFailed,
    };

    struct PickupQueryResult
    {
        PickupResult result{ PickupResult::Failure };
        PickupFailReason failReason{ PickupFailReason::None };
        std::shared_ptr<ActiveSweets> sweets;
        float distance{ std::numeric_limits<float>::quiet_NaN() };
        float depthDelta{ std::numeric_limits<float>::quiet_NaN() };

        bool Succeeded() const noexcept
        {
            return result == PickupResult::Success && sweets != nullptr;
        }
    };

    struct DeleteBatchContext
    {
        int usesRemaining = 0;
    };

    enum class SweetsState : std::uint8_t
    {
        Active,  // 現在制御中または落下中の生成されたお菓子
        Stacked, // タワーの一部であるお菓子を積み重ねることに成功した
        Ground,  // 地面に置かれたお菓子が、拾われるのを待っている
        Carried  // プレイヤーが保持しているため物理・状態遷移を停止
    };

    enum class ColliderSource
    {
        Config,
        Generated,
        Fallback,
    };

    struct ColliderBuildResult
    {
        Physics::Box2DShapeConfig shapeConfig{};
        std::vector<std::vector<b2Vec2>> meshPolygons;
        std::vector<b2Vec2> meshBoundary;
        ColliderSource source{ ColliderSource::Fallback };
        float thickness{ 0.0f };
        float2 centroidOffset{ 0.0f, 0.0f };
    };

    struct FactoryPreset
    {
        float spawnWeight = 1.0f;
        float3 scale{ 1.0f, 1.0f, 1.0f };
        float3 rotationMinDegrees{ 0.0f, 0.0f, 0.0f };
        float3 rotationMaxDegrees{ 0.0f, 0.0f, 0.0f };
        ColliderPreset colliderPreset;
    };
}
