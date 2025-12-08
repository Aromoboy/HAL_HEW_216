#include "Components/BoxColliderComponent.h"

#include <algorithm>
#include <cmath>

#include <box2d/collision.h>
#include <box2d/math_functions.h>

#include "Systems/Box2DPhysicsSystem.h"

namespace
{
        float Clamp01(float value)
        {
                return std::clamp(value, 0.0f, 1.0f);
        }

        float SanitizeDensity(float density)
        {
                if (!std::isfinite(density))
                {
                        return 0.0f;
                }
                return std::max(0.0f, density);
        }

        bool TryBuildPolygon(const Components::BoxColliderComponent::PolygonShape& polygonShape, b2Polygon& outShape)
        {
                if (polygonShape.vertices.size() < 3)
                {
                        return false;
                }

                const b2Hull hull = b2ComputeHull(polygonShape.vertices.data(), static_cast<int>(polygonShape.vertices.size()));
                if (!b2ValidateHull(&hull))
                {
                        return false;
                }

                outShape = b2MakePolygon(&hull, b2_polygonRadius);
                return true;
        }
}

namespace Components
{
        bool BoxColliderComponent::Initialize(Systems::Box2DPhysicsSystem& physicsSystem, const Transform& transform)
        {
                Destroy(physicsSystem);
                return CreateBody(physicsSystem, transform) && CreateFixture();
        }

        bool BoxColliderComponent::Initialize(Systems::Box2DPhysicsSystem& physicsSystem, const Transform& transform, const BoxColliderSerialized& serialized)
        {
                if (serialized.shapeType == "circle")
                {
                        SetShapeKind(Shape::Circle);
                        SetShape(CircleShape{ serialized.radius });
                }
                else if (serialized.shapeType == "polygon")
                {
                        SetShapeKind(Shape::Polygon);
                        PolygonShape polygon{};
                        polygon.vertices.reserve(4);
                        polygon.vertices.push_back(b2Vec2{ -serialized.size.x * 0.5f, -serialized.size.y * 0.5f });
                        polygon.vertices.push_back(b2Vec2{ serialized.size.x * 0.5f, -serialized.size.y * 0.5f });
                        polygon.vertices.push_back(b2Vec2{ serialized.size.x * 0.5f, serialized.size.y * 0.5f });
                        polygon.vertices.push_back(b2Vec2{ -serialized.size.x * 0.5f, serialized.size.y * 0.5f });
                        SetShape(std::move(polygon));
                }
                else
                {
                        SetShapeKind(Shape::Box);
                        SetShape(BoxShape{ serialized.size });
                }

                SetMaterial(Material{ serialized.density, serialized.friction, serialized.restitution });
                return Initialize(physicsSystem, transform);
        }

        void BoxColliderComponent::Destroy(Systems::Box2DPhysicsSystem& physicsSystem)
        {
                const b2WorldId worldId = physicsSystem.GetWorldId();
                if (b2World_IsValid(worldId))
                {
                        if (b2Shape_IsValid(m_fixture))
                        {
                                b2DestroyShape(m_fixture, true);
                        }
                        if (b2Body_IsValid(m_body))
                        {
                                b2DestroyBody(m_body);
                        }
                }
                ResetHandles();
        }

        void BoxColliderComponent::SetShape(BoxShape box)
        {
                m_shapeData = box;
        }

        void BoxColliderComponent::SetShape(CircleShape circle)
        {
                m_shapeData = circle;
        }

        void BoxColliderComponent::SetShape(PolygonShape polygon)
        {
                m_shapeData = std::move(polygon);
        }

        void BoxColliderComponent::SetMaterial(const Material& material)
        {
                m_material = material;
        }

        void BoxColliderComponent::SetShapeKind(Shape shape)
        {
                m_shapeKind = shape;
        }

        BoxColliderSerialized BoxColliderComponent::Serialize() const
        {
                BoxColliderSerialized data{};
                data.density = m_material.density;
                data.friction = m_material.friction;
                data.restitution = m_material.restitution;

                switch (m_shapeKind)
                {
                case Shape::Circle:
                        data.shapeType = "circle";
                        if (const auto* circle = std::get_if<CircleShape>(&m_shapeData))
                        {
                                data.radius = circle->radius;
                        }
                        break;
                case Shape::Polygon:
                        data.shapeType = "polygon";
                        if (const auto* polygon = std::get_if<PolygonShape>(&m_shapeData))
                        {
                                if (polygon->vertices.size() >= 2)
                                {
                                        b2Vec2 min = polygon->vertices.front();
                                        b2Vec2 max = polygon->vertices.front();
                                        for (const b2Vec2& v : polygon->vertices)
                                        {
                                                min.x = std::min(min.x, v.x);
                                                min.y = std::min(min.y, v.y);
                                                max.x = std::max(max.x, v.x);
                                                max.y = std::max(max.y, v.y);
                                        }
                                        data.size = float2{ std::fabs(max.x - min.x), std::fabs(max.y - min.y) };
                                }
                        }
                        break;
                case Shape::Box:
                default:
                        data.shapeType = "box";
                        if (const auto* box = std::get_if<BoxShape>(&m_shapeData))
                        {
                                data.size = box->size;
                        }
                        break;
                }

                return data;
        }

        bool BoxColliderComponent::CreateBody(Systems::Box2DPhysicsSystem& physicsSystem, const Transform& transform)
        {
                m_cachedWorld = physicsSystem.GetWorldId();
                if (!b2World_IsValid(m_cachedWorld))
                {
                        return false;
                }

                b2BodyDef bodyDef = b2DefaultBodyDef();
                bodyDef.position = b2Vec2{ transform.position.x, transform.position.y };
                bodyDef.rotation = b2MakeRot(transform.rotation.z);
                bodyDef.type = (SanitizeDensity(m_material.density) > 0.0f) ? b2_dynamicBody : b2_staticBody;
                bodyDef.gravityScale = (bodyDef.type == b2_dynamicBody) ? 1.0f : 0.0f;

                m_body = b2CreateBody(m_cachedWorld, &bodyDef);
                return b2Body_IsValid(m_body);
        }

        bool BoxColliderComponent::CreateFixture()
        {
                if (!b2Body_IsValid(m_body))
                {
                        return false;
                }

                b2ShapeDef shapeDef = b2DefaultShapeDef();
                shapeDef.density = (b2Body_GetType(m_body) == b2_dynamicBody) ? SanitizeDensity(m_material.density) : 0.0f;
                shapeDef.material.friction = Clamp01(m_material.friction);
                shapeDef.material.restitution = Clamp01(m_material.restitution);
                shapeDef.enableContactEvents = true;

                switch (m_shapeKind)
                {
                case Shape::Circle:
                {
                        const CircleShape* circleShape = std::get_if<CircleShape>(&m_shapeData);
                        const float radius = std::max(0.001f, circleShape ? circleShape->radius : 0.5f);
                        m_runtimeShape = b2Circle{ b2Vec2{ 0.0f, 0.0f }, radius };
                        m_fixture = b2CreateCircleShape(m_body, &shapeDef, &std::get<b2Circle>(m_runtimeShape));
                        break;
                }
                case Shape::Polygon:
                {
                        b2Polygon polygon{};
                        const PolygonShape* polygonShape = std::get_if<PolygonShape>(&m_shapeData);
                        if (!polygonShape || !TryBuildPolygon(*polygonShape, polygon))
                        {
                                return false;
                        }
                        m_runtimeShape = polygon;
                        m_fixture = b2CreatePolygonShape(m_body, &shapeDef, &std::get<b2Polygon>(m_runtimeShape));
                        break;
                }
                case Shape::Box:
                default:
                {
                        const BoxShape box = std::get<BoxShape>(m_shapeData);
                        const float hx = std::max(0.001f, box.size.x * 0.5f);
                        const float hy = std::max(0.001f, box.size.y * 0.5f);
                        m_runtimeShape = b2MakeBox(hx, hy);
                        m_fixture = b2CreatePolygonShape(m_body, &shapeDef, &std::get<b2Polygon>(m_runtimeShape));
                        break;
                }
                }

                if (!b2Shape_IsValid(m_fixture))
                {
                        b2DestroyBody(m_body);
                        ResetHandles();
                        return false;
                }

                return true;
        }

        void BoxColliderComponent::ResetHandles()
        {
                m_body = b2_nullBodyId;
                m_fixture = b2_nullShapeId;
                m_runtimeShape = std::monostate{};
                m_cachedWorld = b2_nullWorldId;
        }
}

