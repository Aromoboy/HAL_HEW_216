#include "Systems/Physics/PhysicsMachine.h"
#include "Systems/Physics/Box2DPhysicsSystem.h"
#include "Systems/Physics/Box2DContactListener.h"

#include "Systems/Physics/ShapeConstants.h"
#include "ColliderRegistry.h"
#include "Entities/PhysicalEntity.h"

#include <algorithm>
#include <cctype>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <span>

#include <box2d/box2d.h>
#include <box2d/collision.h>
#include <box2d/math_functions.h>

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

#include "Core/IUpdatable.h"
#include "Core/StepTimer.h"

namespace
{
    constexpr uint16_t kDefaultCategory = 0x0001;
    constexpr uint16_t kPlayerCategory = 0x0002;
    constexpr uint16_t kFloorCategory = 0x0004;
    constexpr uint16_t kStageCategory = 0x0008;
    constexpr uint16_t kSweetsCategory = 0x0010;
    constexpr float kScaleAssertionEpsilon = 0.0001f;

    void DebugTrace(const std::string& message)
    {
#if defined(_WIN32)
#   if defined(_DEBUG)
        ::OutputDebugStringA(message.c_str());
#   else
        (void)message;
#   endif
#else
        (void)message;
#endif
    }

    uint16_t ToCategoryBits(Collision::CollisionLayer layer)
    {
        switch (layer)
        {
        case Collision::CollisionLayer::Player:
            return kPlayerCategory;
        case Collision::CollisionLayer::Floor:
            return kFloorCategory;
        case Collision::CollisionLayer::Stage:
            return kStageCategory;
        case Collision::CollisionLayer::Sweets:
            return kSweetsCategory;
        default:
            return kDefaultCategory;
        }
    }

    uint16_t ToMaskBits(Collision::CollisionLayer layer)
    {
        switch (layer)
        {
        case Collision::CollisionLayer::Player:
            // プレイヤーは全部と当たる
            return kDefaultCategory | kFloorCategory | kStageCategory | kSweetsCategory | kPlayerCategory;
        case Collision::CollisionLayer::Floor:
            // 床はプレイヤーとスイーツと当たる
            return kDefaultCategory | kPlayerCategory | kSweetsCategory;
        case Collision::CollisionLayer::Stage:
            // ステージ装飾はプレイヤーとスイーツと当たる（接地扱いは別）
            return kPlayerCategory | kSweetsCategory;
        case Collision::CollisionLayer::Sweets:
            // スイーツ同士も当てたいならここで制御
            return kDefaultCategory | kFloorCategory | kStageCategory | kPlayerCategory | kSweetsCategory;
        default:
            return kDefaultCategory | kPlayerCategory | kFloorCategory | kStageCategory | kSweetsCategory;
        }
    }

    const char* ToString(b2BodyType bodyType)
    {
        switch (bodyType)
        {
        case b2_staticBody:
            return "static";
        case b2_kinematicBody:
            return "kinematic";
        case b2_dynamicBody:
            return "dynamic";
        default:
            return "unknown";
        }
    }

    bool HasPhysicsDefinition(const Entities::PhysicalEntity& body)
    {
        const auto& colliderComponent = body.GetBoxColliderComponent();
        return static_cast<bool>(colliderComponent);
    }
}

namespace Systems
{
    class PhysicsMachine::ContactDispatchNode : public Core::UpdateNode
    {
    public:
        explicit ContactDispatchNode(PhysicsMachine& owner)
            : Core::UpdateNode(Core::UpdatePhase::PostPhysics, 0)
            , m_owner(owner)
        {
        }

        void OnUpdate(const Core::UpdateContext& ctx) override
        {
            (void)ctx;
            if (m_owner.m_contactListener)
            {
                m_owner.m_contactListener->ProcessContacts();
            }
        }

    private:
        PhysicsMachine& m_owner;
    };

    PhysicsMachine::PhysicsMachine(const PhysicsTuningParameters& tuning)
        : Core::UpdateNode(Core::UpdatePhase::Physics, 0)
        , m_box2dSystem(std::make_shared<Box2DPhysicsSystem>(*this, tuning))
        , m_system(m_box2dSystem)
        , m_contactListener()
        , m_contactDispatchNode(std::make_unique<ContactDispatchNode>(*this))
        , m_bodies()
        , m_bodyMap()
        , m_defaultBodyDef()
        , m_defaultShapeDef()
        , m_tuning(tuning)
        , m_subStepCount(4)
        , m_loggedMissingSystem(false)
        , m_loggedMissingWorld(false)
        , m_loggedFirstTick(false)
    {
        m_defaultBodyDef = b2DefaultBodyDef();
        m_defaultBodyDef.linearDamping = std::max(0.0f, tuning.defaultLinearDamping);
        m_defaultBodyDef.angularDamping = std::max(0.0f, tuning.defaultAngularDamping);

        m_defaultShapeDef = b2DefaultShapeDef();
        m_defaultShapeDef.material.friction = std::max(0.0f, tuning.defaultFriction);
        m_defaultShapeDef.material.restitution = std::clamp(tuning.defaultRestitution, 0.0f, 1.0f);

        m_tuning.defaultLinearDamping = m_defaultBodyDef.linearDamping;
        m_tuning.defaultAngularDamping = m_defaultBodyDef.angularDamping;
        m_tuning.defaultFriction = m_defaultShapeDef.material.friction;
        m_tuning.defaultRestitution = m_defaultShapeDef.material.restitution;
        m_tuning.restitutionThreshold = std::max(0.0f, tuning.restitutionThreshold);
        m_tuning.hitEventThreshold = std::max(0.0f, tuning.hitEventThreshold);
        m_tuning.continuousCollisionForSweets = tuning.continuousCollisionForSweets;
        m_tuning.continuousCollisionForSmallDynamics = tuning.continuousCollisionForSmallDynamics;
        m_tuning.smallDynamicContinuousThreshold = std::max(0.0f, tuning.smallDynamicContinuousThreshold);
        m_tuning.continuousCollisionForPlayerProjectiles = tuning.continuousCollisionForPlayerProjectiles;
        m_tuning.continuousCollisionForPlayer = tuning.continuousCollisionForPlayer;
        m_tuning.creamGlueLinearHertz = std::max(0.0f, tuning.creamGlueLinearHertz);
        m_tuning.creamGlueAngularHertz = std::max(0.0f, tuning.creamGlueAngularHertz);
        m_tuning.creamGlueLinearDampingRatio = std::clamp(tuning.creamGlueLinearDampingRatio, 0.0f, 1.0f);
        m_tuning.creamGlueAngularDampingRatio = std::clamp(tuning.creamGlueAngularDampingRatio, 0.0f, 1.0f);
        m_tuning.creamGlueCollideConnected = tuning.creamGlueCollideConnected;
        m_tuning.creamGluePushOutEpsilon = std::max(0.0f, tuning.creamGluePushOutEpsilon);
        m_tuning.creamGlueMaxPushOut = std::max(0.0f, tuning.creamGlueMaxPushOut);
        m_tuning.creamGlueMaxSpeed = std::max(0.0f, tuning.creamGlueMaxSpeed);
        m_tuning.creamGlueBreakForce = std::max(0.0f, tuning.creamGlueBreakForce);
        m_tuning.maxStepDt = (std::isfinite(tuning.maxStepDt) && tuning.maxStepDt > 0.0f)
            ? tuning.maxStepDt
            : (1.0f / 60.0f);
        m_tuning.maxAccumulatedSteps = std::clamp(tuning.maxAccumulatedSteps, 1, 120);

        SetSubStepCount(tuning.subStepCount);
        m_tuning.subStepCount = m_subStepCount;
    }

    PhysicsMachine::~PhysicsMachine()
    {
        ClearBodies();
    }

    void PhysicsMachine::SetSystem(SystemPtr system)
    {
        if (m_system == system)
        {
            return;
        }

#if defined(_DEBUG)
        {
        std::ostringstream oss;
            oss << "[PhysicsMachine] SetSystem called. previous=" << m_system.get()
                << " new=" << (system ? system.get() : nullptr) << "\n";
            DebugTrace(oss.str());
        }
#endif

        m_system = std::move(system);
        m_box2dSystem = std::dynamic_pointer_cast<Box2DPhysicsSystem>(m_system);
        m_loggedMissingSystem = false;

        if (m_box2dSystem)
        {
            m_box2dSystem->SetSubStepCount(m_subStepCount);
        }

        if (m_box2dSystem && m_contactListener)
        {
            m_box2dSystem->SetContactListener(m_contactListener);
        }
    }

    void PhysicsMachine::SetContactListener(ContactListenerPtr listener)
    {
        m_contactListener = std::move(listener);
        if (m_box2dSystem)
        {
            // Box2D のイベントバッファはワールド管理で解放不要
            m_box2dSystem->SetContactListener(m_contactListener);
        }

    }

    void PhysicsMachine::SetSubStepCount(int subStepCount)
    {
        m_subStepCount = std::clamp(subStepCount, 1, 24);
        m_tuning.subStepCount = m_subStepCount;
        if (m_box2dSystem)
        {
            m_box2dSystem->SetSubStepCount(m_subStepCount);
        }
    }

    PhysicsMachine::SystemPtr PhysicsMachine::GetSystem() const
    {
        return m_system;
    }

    std::shared_ptr<Box2DPhysicsSystem> PhysicsMachine::GetBox2DSystem() const
    {
        return m_box2dSystem;
    }

    Core::IUpdatable* PhysicsMachine::GetContactDispatchNode() const
    {
        return m_contactDispatchNode.get();
    }

    const std::vector<Entities::PhysicalEntity*>& PhysicsMachine::GetBodies() const
    {
        return m_bodies;
    }

    const PhysicsTuningParameters& PhysicsMachine::GetTuningParameters() const
    {
        return m_tuning;
    }

    void PhysicsMachine::SetGravity(float gravity)
    {
        if (m_box2dSystem)
        {
            m_box2dSystem->SetGravity(gravity);
            m_loggedMissingWorld = false;
        }
    }

    b2BodyId PhysicsMachine::FindBody(const Entities::PhysicalEntity& body) const
    {
        const auto it = m_bodyMap.find(const_cast<Entities::PhysicalEntity*>(&body));
        if (it != m_bodyMap.end())
        {
            return it->second;
        }
        return b2_nullBodyId;
    }

    b2WorldId PhysicsMachine::GetWorldId() const
    {
        return m_box2dSystem ? m_box2dSystem->GetWorldId() : b2_nullWorldId;
    }

    b2JointId PhysicsMachine::CreateWeldJoint(const Entities::PhysicalEntity& bodyA,
        const Entities::PhysicalEntity& bodyB,
        const b2Vec2& worldAnchor,
        float linearHertz,
        float linearDampingRatio,
        float angularHertz,
        float angularDampingRatio,
        bool collideConnected)
    {
        const b2WorldId worldId = GetWorldId();
        if (!b2World_IsValid(worldId))
        {
            DebugTrace("[PhysicsMachine] Box2D world is invalid; cannot create weld joint.\n");
            return b2_nullJointId;
        }

        const b2BodyId bodyIdA = FindBody(bodyA);
        const b2BodyId bodyIdB = FindBody(bodyB);
        if (!b2Body_IsValid(bodyIdA) || !b2Body_IsValid(bodyIdB))
        {
            DebugTrace("[PhysicsMachine] Missing Box2D body; cannot create weld joint.\n");
            return b2_nullJointId;
        }

        b2WeldJointDef jointDef = b2DefaultWeldJointDef();
        jointDef.base.bodyIdA = bodyIdA;
        jointDef.base.bodyIdB = bodyIdB;
        jointDef.base.localFrameA.p = b2Body_GetLocalPoint(bodyIdA, worldAnchor);
        jointDef.base.localFrameB.p = b2Body_GetLocalPoint(bodyIdB, worldAnchor);
        jointDef.base.collideConnected = collideConnected;
        jointDef.linearHertz = linearHertz;
        jointDef.linearDampingRatio = linearDampingRatio;
        jointDef.angularHertz = angularHertz;
        jointDef.angularDampingRatio = angularDampingRatio;

        return b2CreateWeldJoint(worldId, &jointDef);
    }

    void PhysicsMachine::DestroyJoint(b2JointId jointId, bool wakeAttached)
    {
        if (!b2Joint_IsValid(jointId))
        {
            return;
        }

        b2DestroyJoint(jointId, wakeAttached);
    }

    void PhysicsMachine::RegisterBody(Entities::PhysicalEntity& body)
    {
        auto* bodyPtr = &body;
        if (body.HasBodyCreationFailed() && !body.CanAttemptBodyCreation())
        {
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << "[PhysicsMachine] Skipping registration for tag='" << body.GetTag()
                << "' due to previous creation failure.\n";
            DebugTrace(oss.str());
#endif
            return;
        }

        if (body.HasBodyCreationFailed())
        {
            body.ResetBodyCreationFailure();
        }

        if (std::find(m_bodies.begin(), m_bodies.end(), bodyPtr) != m_bodies.end())
        {
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << "[PhysicsMachine] Duplicate body registration detected body=" << bodyPtr << "\n";
            DebugTrace(oss.str());
#endif
            return;
        }

        body.MarkFixtureBuildFailure(false);

        const b2BodyId existing = body.GetBox2DBodyId();
        if (b2Body_IsValid(existing))
        {
            // ワールド比較はやめる 単一ワールド前提で素直に使う
            m_bodies.push_back(bodyPtr);
            m_bodyMap.emplace(bodyPtr, existing);
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << "[PhysicsMachine] Registered pre-created body for tag='" << body.GetTag() << "'\n";
            DebugTrace(oss.str());
#endif
            return;
        }

        if (!HasPhysicsDefinition(body))
        {
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << "[PhysicsMachine] Deferred body registration for tag='" << body.GetTag()
                << "' (collider, shape config, and mesh data missing).\n";
            DebugTrace(oss.str());
#endif
            return;
        }

        m_bodies.push_back(bodyPtr);

        b2BodyId bodyId = CreateBody(body);
        if (b2Body_IsValid(bodyId))
        {
            body.SetBox2DBodyId(bodyId);
            m_bodyMap.emplace(bodyPtr, bodyId);
            return;
        }

        m_bodies.pop_back();
        body.MarkBodyCreationFailed();
    }

    void PhysicsMachine::UnregisterBody(Entities::PhysicalEntity& body, bool preserveColliderData)
    {
        if (m_bodies.empty()) return;
        auto* bodyPtr = &body;
        auto it = std::remove(m_bodies.begin(), m_bodies.end(), bodyPtr);
        if (it == m_bodies.end())
        {
#if defined(_DEBUG)
            if (!body.HasFixtureBuildFailure())
            {
                std::ostringstream oss;
                oss << "[PhysicsMachine] Unregister requested for unknown body body=" << bodyPtr << "\n";
                DebugTrace(oss.str());
            }
#endif
            DestroyBody(body, preserveColliderData);
            return;
        }

        m_bodies.erase(it, m_bodies.end());
        DestroyBody(body, preserveColliderData);
    }

    void PhysicsMachine::ClearBodies()
    {
        for (Entities::PhysicalEntity* body : m_bodies)
        {
            if (body)
            {
                DestroyBody(*body, false);
            }
        }
        m_bodyMap.clear();
        m_bodies.clear();
    }

    void PhysicsMachine::SyncBodies(float dt)
    {
        (void)dt;
        const b2WorldId worldId = GetWorldId();

        if (!b2World_IsValid(worldId))
        {
            if (!m_loggedMissingWorld)
            {
                DebugTrace("[PhysicsMachine] Box2D world is invalid; skipping sync.\n");
                m_loggedMissingWorld = true;
            }
            assert(false && "Box2D world must be valid before syncing bodies.");
            return;
        }

        for (Entities::PhysicalEntity* body : m_bodies)
        {
            if (!body)
            {
#if defined(_DEBUG)
                DebugTrace("[PhysicsMachine] Encountered null body pointer during update.\n");
#endif
                continue;
            }

            auto mapIt = m_bodyMap.find(body);
            if (mapIt == m_bodyMap.end() || !b2Body_IsValid(mapIt->second))
            {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[PhysicsMachine] Missing Box2D body during sync for tag='" << body->GetTag() << "'\n";
                DebugTrace(oss.str());
#endif
                assert(false && "Registered entity is missing a Box2D body.");
                continue;
            }

            const b2BodyId bodyId = mapIt->second;
            body->SetBox2DBodyId(bodyId);
            if (body->GetPhysical().GetBodyType() == b2_staticBody)
            {
                continue;
            }
            body->SyncTransformFromBox2DBody();
            body->SyncColliderBounds();
        }
    }

    void PhysicsMachine::OnUpdate(const Core::UpdateContext& ctx)
    {
        if (!m_system)
        {
            if (!m_loggedMissingSystem)
            {
                DebugTrace("[PhysicsMachine] No physics system configured; skipping step.\n");
                m_loggedMissingSystem = true;
            }
            return;
        }

#if defined(_DEBUG)
        if (!m_loggedFirstTick)
        {
            std::ostringstream oss;
            oss << "[PhysicsMachine] First tick: dt=" << ctx.dt
                << " bodies=" << m_bodies.size() << "\n";
            DebugTrace(oss.str());
            m_loggedFirstTick = true;
        }
#endif

        const b2WorldId worldId = GetWorldId();
        if (!b2World_IsValid(worldId))
        {
            if (!m_loggedMissingWorld)
            {
                DebugTrace("[PhysicsMachine] Box2D world is invalid; skipping step.\n");
                m_loggedMissingWorld = true;
            }
            assert(false && "PhysicsMachine requires a valid Box2D world.");
            return;
        }

        for (auto it = m_bodies.begin(); it != m_bodies.end();)
        {
            Entities::PhysicalEntity* body = *it;
            if (!body)
            {
#if defined(_DEBUG)
                DebugTrace("[PhysicsMachine] Encountered null body pointer before step.\n");
#endif
                ++it;
                continue;
            }

            auto mapIt = m_bodyMap.find(body);
            const bool hasBody = (mapIt != m_bodyMap.end()) && b2Body_IsValid(mapIt->second);
            if (!hasBody)
            {
                if (body->HasBodyCreationFailed() && !body->CanAttemptBodyCreation())
                {
                    ++it;
                    continue;
                }

                if (body->HasBodyCreationFailed())
                {
                    body->ResetBodyCreationFailure();
                }

                const b2BodyId bodyId = CreateBody(*body);
                if (!b2Body_IsValid(bodyId))
                {
#if defined(_DEBUG)
                    std::ostringstream oss;
                    oss << "[PhysicsMachine] Unable to create Box2D body before step for tag='" << body->GetTag() << "'\n";
                    DebugTrace(oss.str());
#endif
                    body->MarkBodyCreationFailed();
                    if (mapIt != m_bodyMap.end())
                    {
                        m_bodyMap.erase(mapIt);
                    }
                    it = m_bodies.erase(it);
                    continue;
                }

                body->SetBox2DBodyId(bodyId);
                m_bodyMap[body] = bodyId;
            }

            ++it;
        }

        if (m_box2dSystem)
        {
            m_box2dSystem->PrepareFrame(ctx.timer.FrameIndex());
        }

        m_system->Apply(ctx.dt);

        if (m_box2dSystem)
        {
            m_box2dSystem->AdvanceClampSummary(ctx.dt);
        }

        SyncBodies(ctx.dt);
    }

    b2BodyId PhysicsMachine::CreateBody(Entities::PhysicalEntity& body)
    {
        const b2WorldId worldId = GetWorldId();

        if (!b2World_IsValid(worldId))
        {
            if (!m_loggedMissingWorld)
            {
                DebugTrace("[PhysicsMachine] Cannot create body; Box2D world is invalid.\n");
                m_loggedMissingWorld = true;
            }
            assert(false && "Cannot create Box2D body without a valid world.");
            return b2_nullBodyId;
        }

        const Transform& transform = body.GetTransform();
        const auto layer = body.GetCollisionLayer();
        if (!body.GetBoxColliderComponent())
        {
            body.SetBoxColliderComponent(Physics::BoxColliderComponent{});
        }

        auto& colliderComponent = body.GetBoxColliderComponent();
        if (!colliderComponent)
        {
            return b2_nullBodyId;
        }

        const auto shapeConfig = body.GetBox2DShapeConfig();
        const std::string& tag = body.GetTag();

        auto failBodyCreation = [&](const std::string& reason)
        {
            colliderComponent->createdFixtureCount = 0;

            if (!body.HasBodyCreationFailed())
            {
                std::ostringstream oss;
                oss << "[PhysicsMachine] " << reason << " tag='" << tag << "'\n";
                DebugTrace(oss.str());
            }

            body.MarkBodyCreationFailed();
            return b2_nullBodyId;
        };

        constexpr float kDimensionEpsilon = std::numeric_limits<float>::epsilon();
        const auto isFinite = [](float value)
        {
            return std::isfinite(value);
        };
        const auto isFiniteVec2 = [&](const b2Vec2& value)
        {
            return isFinite(value.x) && isFinite(value.y);
        };

        const auto registryDefinition = (!colliderComponent->colliderId.empty() && colliderComponent->shapes.empty())
            ? Physics::ColliderRegistry::Instance().Find(colliderComponent->colliderId)
            : std::nullopt;

        if (registryDefinition)
        {
            colliderComponent->shapes = registryDefinition->shapes;

            const b2ShapeDef defaultShapeDef = b2DefaultShapeDef();
            const auto applyIfDefault = [&](float& target, float defaultValue, float source)
            {
                constexpr float kMaterialEpsilon = 1e-4f;
                if (std::fabs(target - defaultValue) <= kMaterialEpsilon)
                {
                    target = source;
                }
            };

            applyIfDefault(
                colliderComponent->shapeDef.material.friction,
                defaultShapeDef.material.friction,
                registryDefinition->shapeDef.material.friction);
            applyIfDefault(
                colliderComponent->shapeDef.material.restitution,
                defaultShapeDef.material.restitution,
                registryDefinition->shapeDef.material.restitution);
            applyIfDefault(
                colliderComponent->shapeDef.density,
                defaultShapeDef.density,
                registryDefinition->shapeDef.density);
        }
        else if (!colliderComponent->colliderId.empty() && colliderComponent->shapes.empty())
        {
            std::ostringstream oss;
            oss << "[PhysicsMachine] Collider id='" << colliderComponent->colliderId
                << "' could not be resolved; collider will not be created.\n";
            DebugTrace(oss.str());
        }

        const b2Vec2 colliderOffset = shapeConfig
            ? shapeConfig->offset
            : colliderComponent->colliderOffset;

        if (!isFiniteVec2(colliderOffset))
        {
            return failBodyCreation("Collider offset is not finite for");
        }

        if (colliderComponent->Empty())
        {
            return failBodyCreation("Collider shapes missing for");
        }

        float maxColliderExtent = 0.0f;
        const auto accumulatePolygonExtent = [&](const std::vector<b2Vec2>& vertices)
        {
            if (vertices.size() < 3)
            {
                return;
            }

            b2Vec2 min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
            b2Vec2 max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
            for (const b2Vec2& vertex : vertices)
            {
                min.x = std::min(min.x, vertex.x);
                min.y = std::min(min.y, vertex.y);
                max.x = std::max(max.x, vertex.x);
                max.y = std::max(max.y, vertex.y);
            }

            const float width = max.x - min.x;
            const float height = max.y - min.y;
            maxColliderExtent = std::max(maxColliderExtent, std::max(width, height));
        };

        const auto accumulateCircleExtent = [&](float radius)
        {
            if (radius <= 0.0f)
            {
                return;
            }

            maxColliderExtent = std::max(maxColliderExtent, radius * 2.0f);
        };

        struct ScaledShape
        {
            Physics::ColliderShapeType type{ Physics::ColliderShapeType::Polygon };
            std::vector<b2Vec2> vertices{};
            b2Vec2 center{ 0.0f, 0.0f };
            float radius{ 0.0f };
        };

        const auto toBox2DVector = [](const b2Vec2& vertex)
        {
            return Entities::PhysicalEntity::ToBox2DVector(float3{ vertex.x, vertex.y, 0.0f });
        };

        const auto toBox2DVertices = [&](const std::vector<b2Vec2>& vertices)
        {
            std::vector<b2Vec2> converted;
            converted.reserve(vertices.size());
            for (const auto& vertex : vertices)
            {
                converted.push_back(toBox2DVector(vertex));
            }
            return converted;
        };

        const b2Vec2 offsetInBox2D = toBox2DVector(colliderOffset);
        std::vector<Physics::ColliderShapeDescriptor> sourceShapes = colliderComponent->shapes;

        std::vector<ScaledShape> scaledShapes;
        scaledShapes.reserve(sourceShapes.size());
        for (const auto& shape : sourceShapes)
        {
            ScaledShape scaled{};
            scaled.type = shape.type;

            switch (shape.type)
            {
            case Physics::ColliderShapeType::Circle:
            {
                scaled.center = shape.vertices.empty()
                    ? b2Vec2{ 0.0f, 0.0f }
                    : toBox2DVector(shape.vertices.front());
                scaled.center.x += offsetInBox2D.x;
                scaled.center.y += offsetInBox2D.y;
                scaled.radius = Entities::PhysicalEntity::ToBox2DLength(shape.radius);
                break;
            }
            case Physics::ColliderShapeType::Box:
            case Physics::ColliderShapeType::Polygon:
            default:
            {
                scaled.vertices = toBox2DVertices(shape.vertices);
                for (auto& vertex : scaled.vertices)
                {
                    vertex.x += offsetInBox2D.x;
                    vertex.y += offsetInBox2D.y;
                }
                break;
            }
            }

            scaledShapes.push_back(std::move(scaled));
        }

        for (const auto& shape : scaledShapes)
        {
            switch (shape.type)
            {
            case Physics::ColliderShapeType::Circle:
                if (!(shape.radius > kDimensionEpsilon) || !isFinite(shape.radius) || !isFiniteVec2(shape.center))
                {
                    return failBodyCreation("Collider circle is invalid for");
                }

                accumulateCircleExtent(shape.radius);
                break;
            case Physics::ColliderShapeType::Box:
            case Physics::ColliderShapeType::Polygon:
            default:
            {
                if (shape.vertices.size() < 3)
                {
                    return failBodyCreation("Polygon collider has insufficient vertices for");
                }

                b2Vec2 min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
                b2Vec2 max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
                for (const b2Vec2& vertex : shape.vertices)
                {
                    if (!isFiniteVec2(vertex))
                    {
                        return failBodyCreation("Polygon collider contains non-finite vertex for");
                    }

                    min.x = std::min(min.x, vertex.x);
                    min.y = std::min(min.y, vertex.y);
                    max.x = std::max(max.x, vertex.x);
                    max.y = std::max(max.y, vertex.y);
                }

                const float width = max.x - min.x;
                const float height = max.y - min.y;
                if (!(width > kDimensionEpsilon) || !(height > kDimensionEpsilon))
                {
                    return failBodyCreation("Polygon collider dimension too small for");
                }

                accumulatePolygonExtent(shape.vertices);
                break;
            }
            }
        }

        // BodyDef 設定
        b2BodyDef bodyDef = m_defaultBodyDef;
        bodyDef.position = Entities::PhysicalEntity::ToBox2DPosition(transform.position);
        bodyDef.type = shapeConfig ? shapeConfig->bodyType : b2_dynamicBody;
        bodyDef.rotation = b2MakeRot(Entities::PhysicalEntity::ToBox2DAngle(transform.rotation.z));
        bodyDef.motionLocks.angularZ = shapeConfig ? shapeConfig->fixedRotation : false;
        if (shapeConfig && shapeConfig->angularDamping > 0.0f)
        {
            bodyDef.angularDamping = shapeConfig->angularDamping;
        }
        if (shapeConfig && shapeConfig->linearDamping > 0.0f)
        {
            bodyDef.linearDamping = shapeConfig->linearDamping;
        }
        // 重力スケール（動的ボディのみ適用）
        bodyDef.gravityScale = (bodyDef.type == b2_dynamicBody) ? 1.0f : 0.0f;
        bodyDef.userData = &body;

        const bool isDynamicBody = bodyDef.type == b2_dynamicBody;
        const bool enableSmallDynamicCcd = m_tuning.continuousCollisionForSmallDynamics
            && isDynamicBody
            && m_tuning.smallDynamicContinuousThreshold > 0.0f
            && maxColliderExtent > 0.0f
            && maxColliderExtent <= Entities::PhysicalEntity::ToBox2DLength(m_tuning.smallDynamicContinuousThreshold);

        bool enableContinuousCollision = false;
        if (m_tuning.continuousCollisionForSweets && layer == Collision::CollisionLayer::Sweets)
        {
            enableContinuousCollision = true;
        }

        if (!enableContinuousCollision && enableSmallDynamicCcd)
        {
            enableContinuousCollision = true;
        }

        if (!enableContinuousCollision && m_tuning.continuousCollisionForPlayerProjectiles)
        {
            std::string lowerTag = tag;
            std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

            if (lowerTag.find("projectile") != std::string::npos)
            {
                enableContinuousCollision = true;
            }
        }

        if (!enableContinuousCollision && m_tuning.continuousCollisionForPlayer
            && layer == Collision::CollisionLayer::Player)
        {
            enableContinuousCollision = true;
        }

        if (enableContinuousCollision)
        {
            bodyDef.isBullet = true;
        }

        b2BodyId bodyId = b2CreateBody(worldId, &bodyDef);
        if (!b2Body_IsValid(bodyId))
        {
            DebugTrace("[PhysicsMachine] Failed to create Box2D body.\n");
            assert(false && "b2CreateBody returned invalid id.");
            return b2_nullBodyId;
        }

        // ShapeDef 設定
        const b2ShapeDef box2dDefaultShapeDef = b2DefaultShapeDef();
        b2ShapeDef shapeDef = colliderComponent->shapeDef;
        shapeDef.userData = &body;
        if (std::fabs(shapeDef.material.friction - box2dDefaultShapeDef.material.friction) <= std::numeric_limits<float>::epsilon())
        {
            shapeDef.material.friction = m_defaultShapeDef.material.friction;
        }
        if (std::fabs(shapeDef.material.restitution - box2dDefaultShapeDef.material.restitution) <= std::numeric_limits<float>::epsilon())
        {
            shapeDef.material.restitution = m_defaultShapeDef.material.restitution;
        }

        const float configuredDensity = shapeConfig
            ? std::max(0.0f, shapeConfig->density)
            : 1.0f;
        shapeDef.density = bodyDef.type == b2_dynamicBody ? configuredDensity : 0.0f;
        if (shapeConfig)
        {
            shapeDef.material.friction = std::max(0.0f, shapeConfig->friction);
            shapeDef.material.restitution = std::clamp(shapeConfig->restitution, 0.0f, 1.0f);
        }
        shapeDef.material.friction = std::max(0.0f, shapeDef.material.friction);
        shapeDef.material.restitution = std::clamp(shapeDef.material.restitution, 0.0f, 1.0f);
        shapeDef.enableContactEvents = true;
        shapeDef.enableHitEvents = true;
        shapeDef.filter.categoryBits = ToCategoryBits(layer);
        shapeDef.filter.maskBits = ToMaskBits(layer);

// すべてのボディ（全形状/型）向けデバッグ情報。
#if defined(_DEBUG)
{
        std::ostringstream oss;
    oss << "[PhysicsMachine] Creating body tag='" << body.GetTag() << "'"
        << " bodyType=" << ToString(bodyDef.type)
        << " lockAngularZ=" << std::boolalpha << bodyDef.motionLocks.angularZ
        << " angularDamping=" << bodyDef.angularDamping
        << " linearDamping=" << bodyDef.linearDamping
        << " shapes=" << colliderComponent->shapes.size()
        << " density=" << shapeDef.density
        << " friction=" << shapeDef.material.friction
        << " restitution=" << shapeDef.material.restitution << "\n";
    DebugTrace(oss.str());
}
#endif

        auto createPolygonFixture = [&](std::vector<b2Vec2> vertices)
        {
            if (vertices.size() < 3)
            {
                return b2_nullShapeId;
            }

            const b2Hull hull = b2ComputeHull(vertices.data(), static_cast<int>(vertices.size()));
            if (!b2ValidateHull(&hull))
            {
                return b2_nullShapeId;
            }

            float maxHullRadius = std::numeric_limits<float>::max();
            if (hull.count >= 3)
            {
                b2Vec2 min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
                b2Vec2 max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
                float minEdgeLength = std::numeric_limits<float>::max();

                for (int i = 0; i < hull.count; ++i)
                {
                    const b2Vec2& current = hull.points[i];
                    const b2Vec2& next = hull.points[(i + 1) % hull.count];
                    min.x = std::min(min.x, current.x);
                    min.y = std::min(min.y, current.y);
                    max.x = std::max(max.x, current.x);
                    max.y = std::max(max.y, current.y);

                    const float edgeLength = std::hypot(next.x - current.x, next.y - current.y);
                    minEdgeLength = std::min(minEdgeLength, edgeLength);
                }

                const float minExtent = std::min(max.x - min.x, max.y - min.y);
                const float minFeature = std::min(minEdgeLength, minExtent);
                maxHullRadius = 0.5f * minFeature;
            }

            const b2Polygon polygon = b2MakePolygon(&hull, 0.0f);
            return b2CreatePolygonShape(bodyId, &shapeDef, &polygon);
        };

        auto createCircleFixture = [&](float radius, const b2Vec2& center)
        {
            if (radius <= 0.0f)
            {
                return b2_nullShapeId;
            }

            const b2Circle circle{ center, radius };
            return b2CreateCircleShape(bodyId, &shapeDef, &circle);
        };

        int fixtureCount = 0;
        bool allFixturesCreated = true;


        colliderComponent->createdFixtureCount = 0;
        std::vector<Physics::ColliderShapeDescriptor> createdShapes;
        createdShapes.reserve(sourceShapes.size());

        for (std::size_t i = 0; i < scaledShapes.size(); ++i)
        {
            const auto& scaled = scaledShapes[i];
            const auto& original = sourceShapes[i];

            bool created = false;
            switch (scaled.type)
            {
            case Physics::ColliderShapeType::Circle:
            {
                const b2ShapeId shapeId = createCircleFixture(scaled.radius, scaled.center);
                created = b2Shape_IsValid(shapeId);
                break;
            }
            case Physics::ColliderShapeType::Box:
            case Physics::ColliderShapeType::Polygon:
            default:
            {
                const b2ShapeId shapeId = createPolygonFixture(scaled.vertices);
                created = b2Shape_IsValid(shapeId);
                break;
            }
            }

            if (created)
            {
                createdShapes.push_back(original);
                ++fixtureCount;
            }
            else
            {
                allFixturesCreated = false;
                break;
            }
        }

        if (allFixturesCreated && fixtureCount > 0)
        {
            colliderComponent->shapeDef = shapeDef;
            colliderComponent->shapes = std::move(createdShapes);
            colliderComponent->createdFixtureCount = fixtureCount;
            return bodyId;
        }

        colliderComponent->createdFixtureCount = 0;
        body.MarkFixtureBuildFailure(true);
        b2DestroyBody(bodyId);
        body.SetBox2DBodyId(b2_nullBodyId);
        return failBodyCreation("Failed to build fixtures for");
    }

    void PhysicsMachine::DestroyBody(const Entities::PhysicalEntity& body, bool preserveColliderData)
    {
        auto it = m_bodyMap.find(const_cast<Entities::PhysicalEntity*>(&body));
        if (it == m_bodyMap.end())
        {
            return;
        }

        if (b2Body_IsValid(it->second))
        {
            std::vector<b2ShapeId> shapes;

            const int shapeCount = b2Body_GetShapeCount(it->second);
            if (shapeCount > 0)
            {
                shapes.resize(static_cast<size_t>(shapeCount));
                const int writtenShapes = b2Body_GetShapes(it->second, shapes.data(), shapeCount);
                shapes.resize(static_cast<size_t>(writtenShapes));
                for (int i = 0; i < writtenShapes; ++i)
                {
                    b2Shape_SetUserData(shapes[static_cast<size_t>(i)], nullptr);
                }
            }

            if (m_contactListener)
            {
                m_contactListener->NotifyDestroyedBody(it->second, shapes);
            }

            b2Body_SetUserData(it->second, nullptr);

            auto& colliderComponent = const_cast<Entities::PhysicalEntity&>(body).GetBoxColliderComponent();
            if (colliderComponent)
            {
                if (preserveColliderData)
                {
                    colliderComponent->createdFixtureCount = 0;
                }
                else
                {
                    colliderComponent->Clear();
                }
            }

            const b2WorldId bodyWorld = b2Body_GetWorld(it->second);
            if (b2World_IsValid(bodyWorld))
            {
                b2DestroyBody(it->second);
            }
        }

        const_cast<Entities::PhysicalEntity&>(body).SetBox2DBodyId(b2_nullBodyId);
        m_bodyMap.erase(it);
    }
}
