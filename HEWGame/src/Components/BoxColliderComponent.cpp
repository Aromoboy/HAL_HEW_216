#include "Components/BoxColliderComponent.h"

#include <algorithm>
#include <cmath>

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
}

namespace Components
{
        bool BoxColliderComponent::Initialize(Systems::Box2DPhysicsSystem& physicsSystem, const Transform& transform)
        {
                (void)physicsSystem;
                (void)transform;
                m_descriptor = BuildDescriptor();
                return !m_descriptor.Empty();
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
                (void)physicsSystem;
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

        Physics::BoxColliderComponent BoxColliderComponent::BuildDescriptor() const
        {
                Physics::BoxColliderComponent descriptor{};
                descriptor.shapeDef.density = SanitizeDensity(m_material.density);
                descriptor.shapeDef.material.friction = Clamp01(m_material.friction);
                descriptor.shapeDef.material.restitution = Clamp01(m_material.restitution);

                switch (m_shapeKind)
                {
                case Shape::Circle:
                {
                        const CircleShape* circleShape = std::get_if<CircleShape>(&m_shapeData);
                        const float radius = circleShape ? circleShape->radius : 0.5f;
                        descriptor.AddCircle(std::max(0.001f, radius));
                        break;
                }
                case Shape::Polygon:
                {
                        const PolygonShape* polygonShape = std::get_if<PolygonShape>(&m_shapeData);
                        if (polygonShape && polygonShape->vertices.size() >= 3)
                        {
                                descriptor.AddPolygon(polygonShape->vertices);
                        }
                        break;
                }
                case Shape::Box:
                default:
                {
                        const BoxShape box = std::get<BoxShape>(m_shapeData);
                        const float hx = std::max(0.001f, box.size.x * 0.5f);
                        const float hy = std::max(0.001f, box.size.y * 0.5f);
                        descriptor.AddBox(hx, hy);
                        break;
                }
                }

                return descriptor;
        }

        void BoxColliderComponent::ResetHandles()
        {
                m_descriptor.Clear();
        }
}

