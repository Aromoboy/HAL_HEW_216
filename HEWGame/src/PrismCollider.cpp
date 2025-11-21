#include "PrismCollider.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Defines.h"

namespace
{
        constexpr float kEpsilon = 1.0e-5f;

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

        float Dot2D(const float3& a, const float3& b)
        {
                return (a.x * b.x) + (a.y * b.y);
        }

        float Length2D(const float3& v)
        {
                return std::sqrt((v.x * v.x) + (v.y * v.y));
        }

        float3 Normalize2D(const float3& v)
        {
                const float len = Length2D(v);
                if (len < kEpsilon)
                {
                        return float3{ 0.0f, 0.0f, 0.0f };
                }

                return float3{ v.x / len, v.y / len, 0.0f };
        }

        float3 Perp(const float3& v)
        {
                return float3{ -v.y, v.x, 0.0f };
        }

        float3 Rotate2D(const float3& v, float radians)
        {
                const float c = std::cos(radians);
                const float s = std::sin(radians);
                return float3{ (v.x * c) - (v.y * s), (v.x * s) + (v.y * c), v.z };
        }

        bool IsValidPenetration(float penetration)
        {
                return std::isfinite(penetration) && penetration > 0.0f;
        }
}

namespace Collision
{
        Prism2DCollider::Prism2DCollider(std::vector<float3> localVertices, float depth, float3 offset, ColliderShape shape, CollisionLayer layer)
                : m_localVertices(std::move(localVertices))
                , m_worldVertices()
                , m_depth(depth)
                , m_offset(offset)
                , m_aabb()
                , m_center{ 0.0f, 0.0f, 0.0f }
                , m_shape(shape)
        {
                SetLayer(layer);
        }

        void Prism2DCollider::UpdateFromTransform(const Transform& transform)
        {
                if (m_localVertices.empty())
                {
                        m_aabb = AABB();
                        return;
                }

                m_worldVertices.clear();
                m_worldVertices.reserve(m_localVertices.size());

                const float radians = transform.rotation.z * (PI / 180.0f);
                float3 minBounds{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 0.0f };
                float3 maxBounds{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), 0.0f };

                float3 accumulated{ 0.0f, 0.0f, 0.0f };

                for (const float3& local : m_localVertices)
                {
                        float3 adjusted{ local.x + m_offset.x, local.y + m_offset.y, 0.0f };
                        adjusted.x *= transform.scale.x;
                        adjusted.y *= transform.scale.y;
                        adjusted = Rotate2D(adjusted, radians);
                        adjusted.x += transform.position.x;
                        adjusted.y += transform.position.y;
                        adjusted.z = transform.position.z + (m_offset.z * transform.scale.z);

                        minBounds.x = std::min(minBounds.x, adjusted.x);
                        minBounds.y = std::min(minBounds.y, adjusted.y);
                        maxBounds.x = std::max(maxBounds.x, adjusted.x);
                        maxBounds.y = std::max(maxBounds.y, adjusted.y);

                        accumulated = Add(accumulated, adjusted);
                        m_worldVertices.push_back(adjusted);
                }

                const float depthHalf = std::max(0.0f, m_depth * 0.5f * transform.scale.z);
                const float depthCenter = transform.position.z + (m_offset.z * transform.scale.z);
                minBounds.z = depthCenter - depthHalf;
                maxBounds.z = depthCenter + depthHalf;

                const float invCount = 1.0f / static_cast<float>(m_worldVertices.size());
                m_center = Scale(accumulated, invCount);

                m_aabb = AABB(maxBounds, minBounds);
        }

        ContactManifold Prism2DCollider::ComputePrismContact(const Prism2DCollider& other) const
        {
                ContactManifold manifold{};

                if (m_worldVertices.empty() || other.m_worldVertices.empty())
                {
                        return manifold;
                }

                float minOverlap = std::numeric_limits<float>::max();
                float3 bestAxis{ 0.0f, 0.0f, 0.0f };

                const auto evaluateAxes = [&](const std::vector<float3>& vertices)
                {
                        for (size_t i = 0; i < vertices.size(); ++i)
                        {
                                const float3& current = vertices[i];
                                const float3& next = vertices[(i + 1) % vertices.size()];
                                const float3 edge = Subtract(next, current);
                                float3 axis = Normalize2D(Perp(edge));
                                if (Length2D(axis) < kEpsilon)
                                {
                                        continue;
                                }

                                float minA = std::numeric_limits<float>::max();
                                float maxA = std::numeric_limits<float>::lowest();
                                for (const float3& v : m_worldVertices)
                                {
                                        const float projection = Dot2D(v, axis);
                                        minA = std::min(minA, projection);
                                        maxA = std::max(maxA, projection);
                                }

                                float minB = std::numeric_limits<float>::max();
                                float maxB = std::numeric_limits<float>::lowest();
                                for (const float3& v : other.m_worldVertices)
                                {
                                        const float projection = Dot2D(v, axis);
                                        minB = std::min(minB, projection);
                                        maxB = std::max(maxB, projection);
                                }

                                const float overlap = std::min(maxA, maxB) - std::max(minA, minB);
                                if (overlap < 0.0f)
                                {
                                        minOverlap = 0.0f;
                                        return false;
                                }

                                if (overlap < minOverlap)
                                {
                                        minOverlap = overlap;
                                        bestAxis = axis;
                                }
                        }
                        return true;
                };

                if (!evaluateAxes(m_worldVertices))
                {
                        return manifold;
                }

                if (!evaluateAxes(other.m_worldVertices))
                {
                        return manifold;
                }

                if (!IsValidPenetration(minOverlap))
                {
                        return manifold;
                }

                const float3 centerDelta = Subtract(other.m_center, m_center);
                if (Dot2D(centerDelta, bestAxis) < 0.0f)
                {
                        bestAxis = Scale(bestAxis, -1.0f);
                }

                manifold.intersects = true;
                manifold.normal = bestAxis;
                manifold.penetration = minOverlap;
                return manifold;
        }

        ContactManifold Prism2DCollider::ComputeContact(const Collision::ICollider& other) const
        {
                if (const auto* prism = dynamic_cast<const Prism2DCollider*>(&other))
                {
                        return ComputePrismContact(*prism);
                }

                ContactManifold manifold{};
                const AABB otherAabb = other.GetAABB();
                const bool overlapX = !(m_aabb.max.x < otherAabb.min.x || m_aabb.min.x > otherAabb.max.x);
                const bool overlapY = !(m_aabb.max.y < otherAabb.min.y || m_aabb.min.y > otherAabb.max.y);
                const bool overlapZ = !(m_aabb.max.z < otherAabb.min.z || m_aabb.min.z > otherAabb.max.z);
                if (!(overlapX && overlapY && overlapZ))
                {
                        return manifold;
                }

                float3 normal{ 0.0f, 0.0f, 0.0f };
                float penetration = std::numeric_limits<float>::max();

                const float overlapXAmount = std::min(m_aabb.max.x, otherAabb.max.x) - std::max(m_aabb.min.x, otherAabb.min.x);
                if (overlapXAmount < penetration)
                {
                        penetration = overlapXAmount;
                        normal = float3{ (m_center.x < (otherAabb.max.x + otherAabb.min.x) * 0.5f) ? -1.0f : 1.0f, 0.0f, 0.0f };
                }

                const float overlapYAmount = std::min(m_aabb.max.y, otherAabb.max.y) - std::max(m_aabb.min.y, otherAabb.min.y);
                if (overlapYAmount < penetration)
                {
                        penetration = overlapYAmount;
                        normal = float3{ 0.0f, (m_center.y < (otherAabb.max.y + otherAabb.min.y) * 0.5f) ? -1.0f : 1.0f, 0.0f };
                }

                if (!IsValidPenetration(penetration))
                {
                        return manifold;
                }

                manifold.intersects = true;
                manifold.normal = normal;
                manifold.penetration = penetration;
                return manifold;
        }

        BoxCollider::BoxCollider(const float3& halfExtents, const float3& offset, CollisionLayer layer)
                : Prism2DCollider(
                        {
                                float3{ -halfExtents.x, -halfExtents.y, 0.0f },
                                float3{ halfExtents.x, -halfExtents.y, 0.0f },
                                float3{ halfExtents.x, halfExtents.y, 0.0f },
                                float3{ -halfExtents.x, halfExtents.y, 0.0f },
                        },
                        std::max(0.0f, halfExtents.z * 2.0f),
                        offset,
                        ColliderShape::Box,
                        layer)
        {
        }
}
