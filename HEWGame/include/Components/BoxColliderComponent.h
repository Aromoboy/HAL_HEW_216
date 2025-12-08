#pragma once

#include <string>
#include <variant>
#include <vector>

#include <box2d/box2d.h>

#include "Transform.h"

namespace Systems
{
        class Box2DPhysicsSystem;
}

namespace Components
{
        struct BoxColliderSerialized
        {
                std::string shapeType;
                float2 size{ 1.0f, 1.0f };
                float radius{ 0.5f };
                float density{ 1.0f };
                float friction{ 0.6f };
                float restitution{ 0.0f };
        };

        class BoxColliderComponent
        {
        public:
                enum class Shape
                {
                        Box,
                        Circle,
                        Polygon,
                };

                struct PolygonShape
                {
                        std::vector<b2Vec2> vertices;
                };

                struct CircleShape
                {
                        float radius{ 0.5f };
                };

                struct BoxShape
                {
                        float2 size{ 1.0f, 1.0f };
                };

                struct Material
                {
                        float density{ 1.0f };
                        float friction{ 0.6f };
                        float restitution{ 0.0f };
                };

                using ShapeData = std::variant<std::monostate, BoxShape, CircleShape, PolygonShape>;

                BoxColliderComponent() = default;

                bool Initialize(Systems::Box2DPhysicsSystem& physicsSystem, const Transform& transform);
                bool Initialize(Systems::Box2DPhysicsSystem& physicsSystem, const Transform& transform, const BoxColliderSerialized& serialized);

                void Destroy(Systems::Box2DPhysicsSystem& physicsSystem);

                void SetShape(BoxShape box);
                void SetShape(CircleShape circle);
                void SetShape(PolygonShape polygon);
                void SetMaterial(const Material& material);
                void SetShapeKind(Shape shape);

                b2BodyId GetBodyId() const { return m_body; }
                b2ShapeId GetFixtureId() const { return m_fixture; }
                Shape GetShapeKind() const { return m_shapeKind; }
                const ShapeData& GetShapeData() const { return m_shapeData; }
                const Material& GetMaterial() const { return m_material; }

                BoxColliderSerialized Serialize() const;

        private:
                bool CreateBody(Systems::Box2DPhysicsSystem& physicsSystem, const Transform& transform);
                bool CreateFixture();
                void ResetHandles();

                Shape m_shapeKind{ Shape::Box };
                ShapeData m_shapeData{ BoxShape{} };
                Material m_material{};

                b2BodyId m_body{ b2_nullBodyId };
                b2ShapeId m_fixture{ b2_nullShapeId };
                std::variant<std::monostate, b2Polygon, b2Circle> m_runtimeShape;
                b2WorldId m_cachedWorld{ b2_nullWorldId };
        };
}

