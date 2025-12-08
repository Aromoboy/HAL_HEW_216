#include "Systems/PhysicsMachine.h"
#include "Systems/Box2DPhysicsSystem.h"
#include "Systems/Box2DContactListener.h"

#include <algorithm>
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
}

namespace Systems
{
    PhysicsMachine::PhysicsMachine(float gravity, int subStepCount)
        : Core::UpdateNode(Core::UpdatePhase::Physics, 0)
        , m_box2dSystem(std::make_shared<Box2DPhysicsSystem>(gravity))
        , m_system(m_box2dSystem)
        , m_contactListener()
        , m_bodies()
        , m_bodyMap()
        , m_subStepCount(4)
        , m_loggedMissingSystem(false)
        , m_loggedMissingWorld(false)
        , m_loggedFirstTick(false)
    {
        SetSubStepCount(subStepCount);
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
            // Box2D のイベントバッファは World 管理。解放不要。
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
            // world の比較はやめる。単一 world 前提で素直に使う。
            m_bodies.push_back(bodyPtr);
            m_bodyMap.emplace(bodyPtr, existing);
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << "[PhysicsMachine] Registered pre-created body for tag='" << body.GetTag() << "'\n";
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
        const bool hasMeshData = hasMeshFixtures || hasMeshBoundary;

        if (!hasColliderComponent && !hasShapeConfig && !hasMeshData)
        {
            std::ostringstream oss;
            oss << "[PhysicsMachine] Cannot create physics body for tag='" << body.GetTag()
                << "': collider component, shape config, and mesh data are all missing.\n";
            DebugTrace(oss.str());
            return b2_nullBodyId;
        }

        const std::string& tag = body.GetTag();
        const bool isSweetTag = tag.rfind("sweet_", 0) == 0;

        const auto clampRange = [](float value, float minValue, float maxValue)
        {
            return std::clamp(value, minValue, maxValue);
        };

        // BodyDef 設定
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.position = Entities::PhysicalEntity::ToBox2DPosition(transform.position);
        bodyDef.type = hasShapeConfig ? shapeConfig->bodyType : b2_dynamicBody;
        bodyDef.rotation = b2MakeRot(Entities::PhysicalEntity::ToBox2DAngle(transform.rotation.z));
        bodyDef.fixedRotation = hasShapeConfig ? shapeConfig->fixedRotation : false;
        bodyDef.angularDamping = hasShapeConfig ? std::max(0.0f, shapeConfig->angularDamping) : bodyDef.angularDamping;
        // 重力スケール（動的ボディのみ適用）
        bodyDef.gravityScale = (bodyDef.type == b2_dynamicBody) ? 1.0f : 0.0f;
        bodyDef.userData = &body;

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
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.userData = &body;
        if (hasColliderComponent)
        {
            shapeDef = colliderComponent->shapeDef;
            shapeDef.userData = &body;
        }

        const float configuredDensity = hasShapeConfig
            ? std::max(0.0f, shapeConfig->density)
            : 1.0f;
        shapeDef.density = bodyDef.type == b2_dynamicBody ? configuredDensity : 0.0f;
        shapeDef.material.friction = hasShapeConfig ? shapeConfig->friction : 0.6f;
        shapeDef.material.restitution = hasShapeConfig ? shapeConfig->restitution : shapeDef.material.restitution;
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
        << " fixedRotation=" << std::boolalpha << bodyDef.fixedRotation
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


        const bool useColliderComponent = hasColliderComponent && !hasMeshFixtures;
        bool createdFixtures = false;

        if (useColliderComponent)
        {
            colliderComponent->Destroy(worldId);
            auto originalPolygons = std::move(colliderComponent->polygons);
            auto originalCircles = std::move(colliderComponent->circles);

            std::vector<b2Polygon> scaledPolygons;
            scaledPolygons.reserve(originalPolygons.size());
            for (const auto& polygon : originalPolygons)
            {
                std::vector<b2Vec2> scaledVertices;
                scaledVertices.reserve(static_cast<std::size_t>(polygon.count));

                for (int i = 0; i < polygon.count; ++i)
                {
                    scaledVertices.push_back(b2Vec2{
                        Entities::PhysicalEntity::ToBox2DLength(polygon.vertices[i].x * transform.scale.x),
                        Entities::PhysicalEntity::ToBox2DLength(polygon.vertices[i].y * transform.scale.y),
                    });
                }

                if (scaledVertices.size() >= 3)
                {
                    const b2Hull hull = b2ComputeHull(scaledVertices.data(), static_cast<int>(scaledVertices.size()));
                    if (b2ValidateHull(&hull))
                    {
                        scaledPolygons.push_back(b2MakePolygon(&hull, polygon.radius));
                    }
                }
            }

            std::vector<b2Circle> scaledCircles;
            scaledCircles.reserve(originalCircles.size());
            for (const auto& circle : originalCircles)
            {
                const float scaledRadius = Entities::PhysicalEntity::ToBox2DLength(
                    circle.radius * std::max(transform.scale.x, transform.scale.y));
                const b2Vec2 scaledCenter{
                    Entities::PhysicalEntity::ToBox2DLength(circle.center.x * transform.scale.x),
                    Entities::PhysicalEntity::ToBox2DLength(circle.center.y * transform.scale.y)
                };
                scaledCircles.push_back(b2Circle{ scaledCenter, scaledRadius });
            }

            colliderComponent->polygons = std::move(scaledPolygons);
            colliderComponent->circles = std::move(scaledCircles);
            colliderComponent->shapeDef = shapeDef;

            colliderComponent->CreateFixtures(bodyId, shapeDef);

            colliderComponent->polygons = std::move(originalPolygons);
            colliderComponent->circles = std::move(originalCircles);
            createdFixtures = !colliderComponent->fixtures.empty();
        }

        // モデル由来のメッシュポリゴンがあれば優先的に使う
        int meshShapes = 0;
        if (hasMeshFixtures)
        {
            colliderComponent->Destroy(worldId);
            colliderComponent->Clear();
            for (const auto& polygon : meshPolygons)
            {
                if (polygon.size() < 3)
                {
                    continue;
                }

                std::vector<b2Vec2> scaled = polygon;
                for (auto& v : scaled)
                {
                    v.x = Entities::PhysicalEntity::ToBox2DLength(v.x * transform.scale.x);
                    v.y = Entities::PhysicalEntity::ToBox2DLength(v.y * transform.scale.y);
                }

                const b2Hull hull = b2ComputeHull(scaled.data(), static_cast<int>(scaled.size()));
                if (!b2ValidateHull(&hull))
                {
                    continue;
                }

                const b2Polygon shape = b2MakePolygon(&hull, 0.0f);
                colliderComponent->polygons.push_back(shape);
                ++meshShapes;
            }

            if (meshShapes == 0 && hasMeshBoundary && meshBoundary.size() >= 3)
            {
                std::vector<b2Vec2> scaled = meshBoundary;
                for (auto& v : scaled)
                {
                    v.x = Entities::PhysicalEntity::ToBox2DLength(v.x * transform.scale.x);
                    v.y = Entities::PhysicalEntity::ToBox2DLength(v.y * transform.scale.y);
                }

                const b2Hull boundaryHull = b2ComputeHull(scaled.data(), static_cast<int>(scaled.size()));
                if (b2ValidateHull(&boundaryHull))
                {
                    const b2Polygon shape = b2MakePolygon(&boundaryHull, 0.0f);
                    colliderComponent->polygons.push_back(shape);
                    ++meshShapes;
                }
            }

            if (meshShapes > 0)
            {
                colliderComponent->shapeDef = shapeDef;
                colliderComponent->CreateFixtures(bodyId, shapeDef);
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[PhysicsMachine] Created mesh fixtures for body tag='" << body.GetTag()
                    << "' polygons=" << meshPolygons.size()
                    << " boundaryPoints=" << meshBoundary.size() << "\n";
                DebugTrace(oss.str());
#endif
                createdFixtures = !colliderComponent->fixtures.empty();
            }
        }

        if (createdFixtures)
        {
            return bodyId;
        }

        // ShapeConfig から形状を作る
        if (hasShapeConfig)
        {
            colliderComponent->shapeDef = shapeDef;
            switch (shapeConfig->type)
            {
            case Physics::Box2DShapeConfig::Type::Box:
            {
                const float hx = Entities::PhysicalEntity::ToBox2DLength(std::max(0.01f, 0.5f * transform.scale.x));
                const float hy = Entities::PhysicalEntity::ToBox2DLength(std::max(0.01f, 0.5f * transform.scale.y));
                const b2Polygon box = b2MakeBox(hx, hy);
                colliderComponent->Clear();
                colliderComponent->shapeDef = shapeDef;
                colliderComponent->polygons.push_back(box);
                colliderComponent->CreateFixtures(bodyId, shapeDef);
                createdFixtures = !colliderComponent->fixtures.empty();
                if (createdFixtures)
                {
                    return bodyId;
                }
                break;
            }
            case Physics::Box2DShapeConfig::Type::Polygon:
            {
                std::vector<b2Vec2> scaled = shapeConfig->vertices;
                for (auto& v : scaled)
                {
                    v.x = Entities::PhysicalEntity::ToBox2DLength(v.x * transform.scale.x);
                    v.y = Entities::PhysicalEntity::ToBox2DLength(v.y * transform.scale.y);
                }

                if (scaled.size() >= 3)
                {
                    const b2Hull hull = b2ComputeHull(scaled.data(), static_cast<int>(scaled.size()));
                    if (b2ValidateHull(&hull))
                    {
                        const b2Polygon shape = b2MakePolygon(&hull, 0.0f);
                        colliderComponent->Clear();
                        colliderComponent->shapeDef = shapeDef;
                        colliderComponent->polygons.push_back(shape);
                        colliderComponent->CreateFixtures(bodyId, shapeDef);
                        createdFixtures = !colliderComponent->fixtures.empty();
                        if (createdFixtures)
                        {
                            return bodyId;
                        }
                    }
                }
                break;
            }
            case Physics::Box2DShapeConfig::Type::Circle:
            {
                const float radius = std::max(0.01f, shapeConfig->radius);
                const float scaledRadius = Entities::PhysicalEntity::ToBox2DLength(
                    radius * std::max(transform.scale.x, transform.scale.y));
                const b2Circle circle{ b2Vec2{0.0f, 0.0f}, scaledRadius };
                colliderComponent->Clear();
                colliderComponent->shapeDef = shapeDef;
                colliderComponent->circles.push_back(circle);
                colliderComponent->CreateFixtures(bodyId, shapeDef);
                createdFixtures = !colliderComponent->fixtures.empty();
                if (createdFixtures)
                {
                    return bodyId;
                }
                break;
            }
            default:
                break;
            }
        }

        std::ostringstream oss;
        oss << "[PhysicsMachine] Failed to build fixtures for tag='" << body.GetTag() << "'.\n";
        DebugTrace(oss.str());

        colliderComponent->Destroy(worldId);
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
                const bool colliderOwnsBody =
                    colliderComponent &&
                    B2_ID_EQUALS(colliderComponent->bodyId, it->second);

                if (colliderComponent)
                {
                    colliderComponent->Destroy(bodyWorld);
                    colliderComponent->Clear();
                }
                if (!colliderOwnsBody)
                {
                    b2DestroyBody(it->second);
                }
            }
        }

        const_cast<Entities::PhysicalEntity&>(body).SetBox2DBodyId(b2_nullBodyId);
        m_bodyMap.erase(it);
    }
}