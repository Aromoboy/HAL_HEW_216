#include "Systems/PhysicsMachine.h"
#include "Systems/Box2DPhysicsSystem.h"
#include "Systems/Box2DContactListener.h"

#include "Physics/ShapeConstants.h"

#include <algorithm>
#include <cctype>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

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

namespace
{
    constexpr uint16_t kDefaultCategory = 0x0001;
    constexpr uint16_t kPlayerCategory = 0x0002;
    constexpr uint16_t kFloorCategory = 0x0004;
    constexpr uint16_t kSweetsCategory = 0x0008;
    constexpr float kScaleAssertionEpsilon = 0.0001f;

    float ToBox2DLength(float renderLength, float axisScale, const Transform& transform)
    {
#if defined(_DEBUG)
        assert(std::isfinite(renderLength));
        assert(std::isfinite(axisScale));
        const float scaleX = transform.scale.x;
        const float scaleY = transform.scale.y;
        const float uniformScale = std::max(scaleX, scaleY);
        const bool matchesAxis = std::fabs(axisScale - scaleX) <= kScaleAssertionEpsilon
            || std::fabs(axisScale - scaleY) <= kScaleAssertionEpsilon
            || std::fabs(axisScale - uniformScale) <= kScaleAssertionEpsilon;
        assert(matchesAxis && "Render-space collider length must be scaled by transform.scale before Box2D conversion.");
#endif
        return Entities::PhysicalEntity::ToBox2DLength(renderLength * axisScale);
    }

    b2Vec2 ToBox2DPosition(const b2Vec2& renderPosition, const Transform& transform)
    {
        return b2Vec2{
            ToBox2DLength(renderPosition.x, transform.scale.x, transform),
            ToBox2DLength(renderPosition.y, transform.scale.y, transform),
        };
    }

    std::vector<b2Vec2> ToBox2DVertices(const std::vector<b2Vec2>& renderVertices, const Transform& transform)
    {
        std::vector<b2Vec2> scaledVertices = renderVertices;
        for (auto& vertex : scaledVertices)
        {
            vertex = ToBox2DPosition(vertex, transform);
        }

        return scaledVertices;
    }

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
            return kDefaultCategory | kFloorCategory | kSweetsCategory | kPlayerCategory;
        case Collision::CollisionLayer::Floor:
            // 床はプレイヤーとスイーツと当たる
            return kDefaultCategory | kPlayerCategory | kSweetsCategory;
        case Collision::CollisionLayer::Sweets:
            // スイーツ同士も当てたいならここで制御
            return kDefaultCategory | kFloorCategory | kPlayerCategory | kSweetsCategory;
        default:
            return kDefaultCategory | kPlayerCategory | kFloorCategory | kSweetsCategory;
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

    const char* ToString(Physics::Box2DShapeConfig::Type shapeType)
    {
        switch (shapeType)
        {
        case Physics::Box2DShapeConfig::Type::None:
            return "none";
        case Physics::Box2DShapeConfig::Type::Box:
            return "box";
        case Physics::Box2DShapeConfig::Type::Polygon:
            return "polygon";
        case Physics::Box2DShapeConfig::Type::Circle:
            return "circle";
        default:
            return "unknown";
        }
    }

    bool HasPhysicsDefinition(const Entities::PhysicalEntity& body)
    {
        const auto& colliderComponent = body.GetBoxColliderComponent();
        const bool hasColliderComponent = colliderComponent && !colliderComponent->Empty();

        const auto shapeConfig = body.GetBox2DShapeConfig();
        const bool hasShapeConfig = shapeConfig && shapeConfig->type != Physics::Box2DShapeConfig::Type::None;

        const bool hasMeshData = body.HasBox2DMesh() || !body.GetBox2DBoundary().empty();
        return hasColliderComponent || hasShapeConfig || hasMeshData;
    }
}

namespace Systems
{
    PhysicsMachine::PhysicsMachine(const PhysicsTuningParameters& tuning)
        : Core::UpdateNode(Core::UpdatePhase::Physics, 0)
        , m_box2dSystem(std::make_shared<Box2DPhysicsSystem>(tuning))
        , m_system(m_box2dSystem)
        , m_contactListener()
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
        m_tuning.maxStepDt = (std::isfinite(tuning.maxStepDt) && tuning.maxStepDt > 0.0f)
            ? tuning.maxStepDt
            : (1.0f / 60.0f);
        m_tuning.maxAccumulatedSteps = std::clamp(tuning.maxAccumulatedSteps, 1, 120);

        SetSubStepCount(tuning.subStepCount);
        m_tuning.subStepCount = m_subStepCount;
        if (m_box2dSystem)
        {
            m_box2dSystem->SetPhysicsMachine(this);
        }
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
            m_box2dSystem->SetPhysicsMachine(this);
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
            // Box2D のイベントバッファは World 管理 解放不要 
            m_box2dSystem->SetContactListener(m_contactListener);
        }

        if (m_contactListener)
        {
            m_contactListener->SetPhysicsMachine(this);
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

    const std::vector<Entities::PhysicalEntity*>& PhysicsMachine::GetBodies() const
    {
        return m_bodies;
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

    void PhysicsMachine::RegisterBody(Entities::PhysicalEntity& body)
    {
        auto* bodyPtr = &body;
        if (std::find(m_bodies.begin(), m_bodies.end(), bodyPtr) != m_bodies.end())
        {
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << "[PhysicsMachine] Duplicate body registration detected body=" << bodyPtr << "\n";
            DebugTrace(oss.str());
#endif
            return;
        }

        const b2BodyId existing = body.GetBox2DBodyId();
        if (b2Body_IsValid(existing))
        {
            // world の比較はやめる 単一 world 前提で素直に使う
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
    }

    void PhysicsMachine::UnregisterBody(Entities::PhysicalEntity& body)
    {
        if (m_bodies.empty()) return;
        auto* bodyPtr = &body;
        auto it = std::remove(m_bodies.begin(), m_bodies.end(), bodyPtr);
        if (it == m_bodies.end())
        {
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << "[PhysicsMachine] Unregister requested for unknown body body=" << bodyPtr << "\n";
            DebugTrace(oss.str());
#endif
            DestroyBody(body);
            return;
        }

        m_bodies.erase(it, m_bodies.end());
        DestroyBody(body);
    }

    void PhysicsMachine::ClearBodies()
    {
        for (Entities::PhysicalEntity* body : m_bodies)
        {
            if (body)
            {
                DestroyBody(*body);
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

    void PhysicsMachine::OnUpdate(Core::UpdateContext& ctx)
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

        for (Entities::PhysicalEntity* body : m_bodies)
        {
            if (!body)
            {
#if defined(_DEBUG)
                DebugTrace("[PhysicsMachine] Encountered null body pointer before step.\n");
#endif
                continue;
            }

            auto mapIt = m_bodyMap.find(body);
            const bool hasBody = (mapIt != m_bodyMap.end()) && b2Body_IsValid(mapIt->second);
            if (!hasBody)
            {
                const b2BodyId bodyId = CreateBody(*body);
                if (!b2Body_IsValid(bodyId))
                {
#if defined(_DEBUG)
                    std::ostringstream oss;
                    oss << "[PhysicsMachine] Unable to create Box2D body before step for tag='" << body->GetTag() << "'\n";
                    DebugTrace(oss.str());
#endif
                    assert(false && "Failed to create Box2D body before physics step.");
                    continue;
                }

                body->SetBox2DBodyId(bodyId);
                m_bodyMap[body] = bodyId;
            }
        }

        m_system->Apply(ctx.dt);
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
        const bool hasColliderComponent = colliderComponent && !colliderComponent->Empty();
        const auto shapeConfig = body.GetBox2DShapeConfig();
        const bool hasShapeConfig = shapeConfig && shapeConfig->type != Physics::Box2DShapeConfig::Type::None;

        const auto& meshPolygons = body.GetBox2DPolygons();
        const auto& meshBoundary = body.GetBox2DBoundary();
        const bool hasMeshFixtures = !meshPolygons.empty();
        const bool hasMeshBoundary = !meshBoundary.empty();

        const std::string& tag = body.GetTag();
        const bool isSweetTag = tag.rfind("sweet_", 0) == 0;

        const b2BodyType intendedBodyType = hasShapeConfig ? shapeConfig->bodyType : b2_dynamicBody;
        const bool isDynamicBodyType = intendedBodyType == b2_dynamicBody;
        const bool meshAllowedForSweets = !isSweetTag || !isDynamicBodyType || m_tuning.allowMeshFixturesForDynamicSweets;
        const bool useMeshFixtures = hasMeshFixtures && meshAllowedForSweets;
        const bool useMeshBoundary = hasMeshBoundary && meshAllowedForSweets;
        const bool useMeshData = useMeshFixtures || useMeshBoundary;

        if (!hasColliderComponent && !hasShapeConfig && !useMeshData)
        {
            std::ostringstream oss;
            oss << "[PhysicsMachine] Cannot create physics body for tag='" << body.GetTag()
                << "': collider component, shape config, and mesh data are all missing.\n";
            DebugTrace(oss.str());
            return b2_nullBodyId;
        }

#if defined(_DEBUG)
        if (!meshAllowedForSweets && hasMeshData)
        {
            std::ostringstream oss;
            oss << "[PhysicsMachine] Mesh fixtures suppressed for tag='" << tag
                << "' (dynamic sweets require explicit collider definitions).\n";
            DebugTrace(oss.str());
        }
#endif

        const bool useColliderComponent = hasColliderComponent && !useMeshData;

        const float minBox2DDimension = Entities::PhysicalEntity::ToBox2DLength(
            Physics::kMinimumShapeDimensionMeters);

        const auto enforceMinimumDimension = [&](float length, const char* context)
        {
            if (!(length > 0.0f))
            {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[PhysicsMachine] " << context << " invalid or zero for tag='" << tag
                    << "'; raising to minimum dimension=" << minBox2DDimension << "\n";
                DebugTrace(oss.str());
#endif
                return minBox2DDimension;
            }

            if (length < minBox2DDimension)
            {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[PhysicsMachine] " << context << " below minimum for tag='" << tag
                    << "': " << length << " < " << minBox2DDimension << " (clamped)\n";
                DebugTrace(oss.str());
#endif
                return minBox2DDimension;
            }

            return length;
        };

        const auto isPolygonTooSmall = [&](const std::vector<b2Vec2>& vertices)
        {
            if (vertices.size() < 3)
            {
                return true;
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
            const bool tooSmall = width < minBox2DDimension || height < minBox2DDimension;
#if defined(_DEBUG)
            if (tooSmall)
            {
                std::ostringstream oss;
                oss << "[PhysicsMachine] Polygon collider rejected for tag='" << tag
                    << "' (width=" << width << ", height=" << height
                    << ") below minimum dimension=" << minBox2DDimension << "\n";
                DebugTrace(oss.str());
            }
#endif
            return tooSmall;
        };

        const auto clampRange = [](float value, float minValue, float maxValue)
        {
            return std::clamp(value, minValue, maxValue);
        };

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

        if (useColliderComponent)
        {
            for (const auto& shape : colliderComponent->shapes)
            {
                switch (shape.type)
                {
                case Physics::ColliderShapeType::Circle:
                {
                    const float scaledRadius = ToBox2DLength(shape.radius, std::max(transform.scale.x, transform.scale.y), transform);
                    accumulateCircleExtent(scaledRadius);
                    break;
                }
                case Physics::ColliderShapeType::Box:
                case Physics::ColliderShapeType::Polygon:
                default:
                {
                    accumulatePolygonExtent(ToBox2DVertices(shape.vertices, transform));
                    break;
                }
                }
            }
        }

        if (useMeshFixtures)
        {
            for (const auto& polygon : meshPolygons)
            {
                accumulatePolygonExtent(ToBox2DVertices(polygon, transform));
            }
        }

        if (useMeshBoundary)
        {
            accumulatePolygonExtent(ToBox2DVertices(meshBoundary, transform));
        }

        if (hasShapeConfig)
        {
            switch (shapeConfig->type)
            {
            case Physics::Box2DShapeConfig::Type::Box:
            {
                const float hx = ToBox2DLength(0.5f, transform.scale.x, transform);
                const float hy = ToBox2DLength(0.5f, transform.scale.y, transform);
                accumulatePolygonExtent({
                    b2Vec2{ -hx, -hy },
                    b2Vec2{ hx, -hy },
                    b2Vec2{ hx, hy },
                    b2Vec2{ -hx, hy },
                });
                break;
            }
            case Physics::Box2DShapeConfig::Type::Polygon:
            {
                accumulatePolygonExtent(ToBox2DVertices(shapeConfig->vertices, transform));
                break;
            }
            case Physics::Box2DShapeConfig::Type::Circle:
            {
                const float scaledRadius = ToBox2DLength(shapeConfig->radius, std::max(transform.scale.x, transform.scale.y), transform);
                accumulateCircleExtent(scaledRadius);
                break;
            }
            default:
                break;
            }
        }

        // BodyDef 設定
        b2BodyDef bodyDef = m_defaultBodyDef;
        bodyDef.position = Entities::PhysicalEntity::ToBox2DPosition(transform.position);
        bodyDef.type = intendedBodyType;
        bodyDef.rotation = b2MakeRot(Entities::PhysicalEntity::ToBox2DAngle(transform.rotation.z));
        bodyDef.motionLocks.angularZ = hasShapeConfig ? shapeConfig->fixedRotation : false;
        if (hasShapeConfig && shapeConfig->angularDamping > 0.0f)
        {
            bodyDef.angularDamping = shapeConfig->angularDamping;
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
        if (m_tuning.continuousCollisionForSweets && (layer == Collision::CollisionLayer::Sweets || isSweetTag))
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

        if (enableContinuousCollision)
        {
            bodyDef.isBullet = true;
        }

        if (isSweetTag)
        {
            bodyDef.type = b2_dynamicBody;
            bodyDef.gravityScale = 1.0f;
            bodyDef.linearDamping = clampRange(bodyDef.linearDamping, 0.5f, 1.0f);
            bodyDef.angularDamping = clampRange(bodyDef.angularDamping, 1.0f, 3.0f);
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
        b2ShapeDef shapeDef = m_defaultShapeDef;
        shapeDef.userData = &body;
        if (hasColliderComponent)
        {
            shapeDef = colliderComponent->shapeDef;
            shapeDef.userData = &body;
            if (!hasShapeConfig)
            {
                if (std::fabs(shapeDef.material.friction - box2dDefaultShapeDef.material.friction) <= std::numeric_limits<float>::epsilon())
                {
                    shapeDef.material.friction = m_defaultShapeDef.material.friction;
                }
                if (std::fabs(shapeDef.material.restitution - box2dDefaultShapeDef.material.restitution) <= std::numeric_limits<float>::epsilon())
                {
                    shapeDef.material.restitution = m_defaultShapeDef.material.restitution;
                }
            }
        }

        const float configuredDensity = hasShapeConfig
            ? std::max(0.0f, shapeConfig->density)
            : 1.0f;
        shapeDef.density = bodyDef.type == b2_dynamicBody ? configuredDensity : 0.0f;
        if (hasShapeConfig)
        {
            shapeDef.material.friction = std::max(0.0f, shapeConfig->friction);
            shapeDef.material.restitution = std::clamp(shapeConfig->restitution, 0.0f, 1.0f);
        }
        else
        {
            shapeDef.material.friction = std::max(0.0f, shapeDef.material.friction);
            shapeDef.material.restitution = std::clamp(shapeDef.material.restitution, 0.0f, 1.0f);
        }
        shapeDef.enableContactEvents = true;
        shapeDef.enableHitEvents = true;
        shapeDef.filter.categoryBits = ToCategoryBits(layer);
        shapeDef.filter.maskBits = ToMaskBits(layer);

// Debug info for any body (all shapes/types)
#if defined(_DEBUG)
{
        std::ostringstream oss;
    oss << "[PhysicsMachine] Creating body tag='" << body.GetTag() << "'"
        << " bodyType=" << ToString(bodyDef.type)
        << " lockAngularZ=" << std::boolalpha << bodyDef.motionLocks.angularZ
        << " angularDamping=" << bodyDef.angularDamping
        << " shapeType=" << ToString(hasShapeConfig ? shapeConfig->type : Physics::Box2DShapeConfig::Type::None)
        << " density=" << shapeDef.density
        << " friction=" << shapeDef.material.friction
        << " restitution=" << shapeDef.material.restitution << "\n";
    DebugTrace(oss.str());
}
#endif

if (isSweetTag)
{
    shapeDef.material.friction = clampRange(shapeDef.material.friction, 0.6f, 0.8f);
    shapeDef.material.restitution = clampRange(shapeDef.material.restitution, 0.1f, 0.25f);

    auto toBodyTypeString = [](b2BodyType type)
    {
        switch (type)
        {
        case b2_staticBody: return "static";
        case b2_kinematicBody: return "kinematic";
        default: return "dynamic";
        }
    };

    std::ostringstream oss;
    oss << "[PhysicsMachine] sweet body params tag='" << tag
        << "' type=" << toBodyTypeString(bodyDef.type)
        << " gravityScale=" << bodyDef.gravityScale
        << " linearDamping=" << bodyDef.linearDamping
        << " angularDamping=" << bodyDef.angularDamping
        << " friction=" << shapeDef.material.friction
        << " restitution=" << shapeDef.material.restitution << "\n";
    DebugTrace(oss.str());
}


        auto createPolygonFixture = [&](std::vector<b2Vec2> vertices, float radius = 0.0f)
        {
            if (vertices.size() < 3 || isPolygonTooSmall(vertices))
            {
                return b2_nullShapeId;
            }

            const b2Hull hull = b2ComputeHull(vertices.data(), static_cast<int>(vertices.size()));
            if (!b2ValidateHull(&hull))
            {
                return b2_nullShapeId;
            }

            const b2Polygon polygon = b2MakePolygon(&hull, radius);
            return b2CreatePolygonShape(bodyId, &shapeDef, &polygon);
        };

        auto createCircleFixture = [&](float radius)
        {
            if (radius <= 0.0f)
            {
                return b2_nullShapeId;
            }

            const float clamped = enforceMinimumDimension(radius, "Circle radius");
            const b2Circle circle{ b2Vec2{0.0f, 0.0f}, clamped };
            return b2CreateCircleShape(bodyId, &shapeDef, &circle);
        };

        int fixtureCount = 0;

        auto recordFixture = [&](b2ShapeId id)
        {
            if (b2Shape_IsValid(id))
            {
                ++fixtureCount;
                return true;
            }
            return false;
        };

        if (useColliderComponent)
        {
            colliderComponent->createdFixtureCount = 0;
            for (const auto& shape : colliderComponent->shapes)
            {
                switch (shape.type)
                {
                case Physics::ColliderShapeType::Circle:
                {
                    const float scaledRadius = ToBox2DLength(shape.radius, std::max(transform.scale.x, transform.scale.y), transform);
                    recordFixture(createCircleFixture(enforceMinimumDimension(scaledRadius, "Collider circle radius")));
                    break;
                }
                case Physics::ColliderShapeType::Box:
                case Physics::ColliderShapeType::Polygon:
                default:
                {
                    recordFixture(createPolygonFixture(ToBox2DVertices(shape.vertices, transform)));
                    break;
                }
                }
            }

            colliderComponent->shapeDef = shapeDef;
            colliderComponent->createdFixtureCount = fixtureCount;
        }

        const bool createdFromComponent = useColliderComponent && fixtureCount > 0;
        if (createdFromComponent)
        {
            return bodyId;
        }

        // モデル由来のメッシュポリゴンがあれば優先的に使う（動的スイーツは除外）
        fixtureCount = 0;
        if (useMeshFixtures)
        {
            colliderComponent->Clear();
            colliderComponent->shapeDef = shapeDef;

            for (const auto& polygon : meshPolygons)
            {
                if (polygon.size() < 3)
                {
                    continue;
                }

                colliderComponent->shapes.push_back({ Physics::ColliderShapeType::Polygon, polygon });

                recordFixture(createPolygonFixture(ToBox2DVertices(polygon, transform)));
            }

            if (fixtureCount == 0 && useMeshBoundary && meshBoundary.size() >= 3)
            {
                colliderComponent->shapes.push_back({ Physics::ColliderShapeType::Polygon, meshBoundary });

                recordFixture(createPolygonFixture(ToBox2DVertices(meshBoundary, transform)));
            }

            colliderComponent->createdFixtureCount = fixtureCount;

            if (fixtureCount > 0)
            {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[PhysicsMachine] Created mesh fixtures for body tag='" << body.GetTag() << "'"
                    << " polygons=" << meshPolygons.size()
                    << " boundaryPoints=" << meshBoundary.size() << "\n";
                DebugTrace(oss.str());
#endif
                return bodyId;
            }
        }

        // ShapeConfig から形状を作る
        fixtureCount = 0;
        if (hasShapeConfig)
        {
            colliderComponent->Clear();
            colliderComponent->shapeDef = shapeDef;
            switch (shapeConfig->type)
            {
            case Physics::Box2DShapeConfig::Type::Box:
            {
                const float hx = enforceMinimumDimension(ToBox2DLength(0.5f, transform.scale.x, transform), "Box half-width");
                const float hy = enforceMinimumDimension(ToBox2DLength(0.5f, transform.scale.y, transform), "Box half-height");
                colliderComponent->shapes.push_back({ Physics::ColliderShapeType::Box, {
                    b2Vec2{ -hx, -hy },
                    b2Vec2{ hx, -hy },
                    b2Vec2{ hx, hy },
                    b2Vec2{ -hx, hy },
                } });
                recordFixture(createPolygonFixture(colliderComponent->shapes.back().vertices));
                if (fixtureCount > 0)
                {
                    colliderComponent->createdFixtureCount = fixtureCount;
                    return bodyId;
                }
                break;
            }
            case Physics::Box2DShapeConfig::Type::Polygon:
            {
                colliderComponent->shapes.push_back({ Physics::ColliderShapeType::Polygon, shapeConfig->vertices });

                recordFixture(createPolygonFixture(ToBox2DVertices(shapeConfig->vertices, transform)));
                if (fixtureCount > 0)
                {
                    colliderComponent->createdFixtureCount = fixtureCount;
                    return bodyId;
                }
                break;
            }
            case Physics::Box2DShapeConfig::Type::Circle:
            {
                colliderComponent->shapes.push_back({ Physics::ColliderShapeType::Circle, {}, shapeConfig->radius });

                const float scaledRadius = ToBox2DLength(shapeConfig->radius, std::max(transform.scale.x, transform.scale.y), transform);
                recordFixture(createCircleFixture(enforceMinimumDimension(scaledRadius, "ShapeConfig circle radius")));
                if (fixtureCount > 0)
                {
                    colliderComponent->createdFixtureCount = fixtureCount;
                    return bodyId;
                }
                break;
            }
            default:
                break;
            }
            colliderComponent->createdFixtureCount = fixtureCount;
        }

        std::ostringstream oss;
        oss << "[PhysicsMachine] Failed to build fixtures for tag='" << body.GetTag() << "'.\n";
        DebugTrace(oss.str());

        colliderComponent->Clear();
        b2DestroyBody(bodyId);
        body.SetBox2DBodyId(b2_nullBodyId);
        return b2_nullBodyId;
    }

    void PhysicsMachine::DestroyBody(const Entities::PhysicalEntity& body)
    {
        auto it = m_bodyMap.find(const_cast<Entities::PhysicalEntity*>(&body));
        if (it == m_bodyMap.end())
        {
            return;
        }

        if (b2Body_IsValid(it->second))
        {
            const b2WorldId bodyWorld = b2Body_GetWorld(it->second);
            if (b2World_IsValid(bodyWorld))
            {
                auto& colliderComponent = const_cast<Entities::PhysicalEntity&>(body).GetBoxColliderComponent();
                if (colliderComponent)
                {
                    colliderComponent->Clear();
                }
                b2DestroyBody(it->second);
            }
        }

        const_cast<Entities::PhysicalEntity&>(body).SetBox2DBodyId(b2_nullBodyId);
        m_bodyMap.erase(it);
    }
}