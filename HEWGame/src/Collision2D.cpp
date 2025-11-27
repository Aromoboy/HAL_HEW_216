#include "Collision2D.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
        float2 Add(const float2& a, const float2& b)
        {
                return float2{ a.x + b.x, a.y + b.y };
        }

        float2 Sub(const float2& a, const float2& b)
        {
                return float2{ a.x - b.x, a.y - b.y };
        }

        float2 Scale(const float2& v, float s)
        {
                return float2{ v.x * s, v.y * s };
        }

        float Dot(const float2& a, const float2& b)
        {
                return (a.x * b.x) + (a.y * b.y);
        }

        float LengthSq(const float2& v)
        {
                return Dot(v, v);
        }

        float2 Normalize(const float2& v)
        {
                const float lenSq = LengthSq(v);
                if (lenSq <= 0.0f)
                {
                        return float2{ 0.0f, 0.0f };
                }
                const float invLen = 1.0f / std::sqrt(lenSq);
                return Scale(v, invLen);
        }

        float2 Perp(const float2& v)
        {
                return float2{ -v.y, v.x };
        }

        float Cross(const float2& a, const float2& b)
        {
                return a.x * b.y - a.y * b.x;
        }

        float2 Rotate(const float2& v, float radians)
        {
                const float c = std::cos(radians);
                const float s = std::sin(radians);
                return float2{ v.x * c - v.y * s, v.x * s + v.y * c };
        }

#if defined(_WIN32) && defined(_DEBUG)
        void DebugTrace(const std::string& message)
        {
                ::OutputDebugStringA(message.c_str());
        }
#else
        void DebugTrace(const std::string& message)
        {
                (void)message;
        }
#endif
}

namespace Collision
{
        void Collider2DBase::UpdateZBounds(const Transform& transform, float thicknessScale)
        {
                const float halfThickness = m_halfThickness * std::max(thicknessScale, 0.0f);
                m_aabb.max.z = transform.position.z + halfThickness;
                m_aabb.min.z = transform.position.z - halfThickness;
        }

        CircleCollider2D::CircleCollider2D(float radius, float2 offset, float thickness, CollisionLayer layer)
                : Collider2DBase(thickness)
                , m_offset(offset)
                , m_radius(std::max(radius, 0.0f))
                , m_center{ 0.0f, 0.0f }
        {
                SetLayer(layer);
        }

        bool CircleCollider2D::CheckCollision(const Collision::ICollider& other)
        {
                ContactManifold2D unused{};
                const auto* other2D = dynamic_cast<const ICollider2D*>(&other);
                return other2D && ComputeContact(*other2D, unused);
        }

        void CircleCollider2D::UpdateBounds(const Transform& transform)
        {
                const float radiusScale = std::max(transform.scale.x, transform.scale.y);
                if (std::abs(radiusScale - m_radiusScale) > 1e-4f)
                {
                        std::ostringstream oss;
                        oss << "[CircleCollider2D] radius scale updated from " << m_radiusScale << " to " << radiusScale
                                << " (base radius: " << m_radius << ", scaled radius: " << (m_radius * radiusScale) << ", offset: ("
                                << m_offset.x << ", " << m_offset.y << ") )\n";
                        DebugTrace(oss.str());
                        m_radiusScale = radiusScale;
                }
                m_scaledRadius = m_radius * m_radiusScale;
                const float2 scaledOffset{ m_offset.x * transform.scale.x, m_offset.y * transform.scale.y };

                m_center = Add(float2{ transform.position.x, transform.position.y }, scaledOffset);

                m_aabb.min.x = m_center.x - m_scaledRadius;
                m_aabb.max.x = m_center.x + m_scaledRadius;
                m_aabb.min.y = m_center.y - m_scaledRadius;
                m_aabb.max.y = m_center.y + m_scaledRadius;

                UpdateZBounds(transform, transform.scale.z);
        }

        bool CircleCollider2D::ComputeContact(const ICollider2D& other, ContactManifold2D& manifold) const
        {
                if (const auto* circle = dynamic_cast<const CircleCollider2D*>(&other))
                {
                        return ComputeContact(*circle, &manifold);
                }
                if (const auto* hull = dynamic_cast<const ConvexHullCollider2D*>(&other))
                {
                        return hull->ComputeContact(*this, manifold);
                }
                if (const auto* compound = dynamic_cast<const CompoundCollider2D*>(&other))
                {
                        bool hit = false;
                        ContactManifold2D best{};
                        best.penetration = std::numeric_limits<float>::max();
                        for (const auto& child : compound->GetChildren())
                        {
                                const auto* child2D = dynamic_cast<const ICollider2D*>(child.get());
                                if (!child2D)
                                {
                                        continue;
                                }
                                ContactManifold2D candidate{};
                                if (ComputeContact(*child2D, candidate) && candidate.penetration < best.penetration)
                                {
                                        best = candidate;
                                        hit = true;
                                }
                        }
                        if (hit)
                        {
                                manifold = best;
                        }
                        return hit;
                }

                return false;
        }

        bool CircleCollider2D::ComputeContact(const CircleCollider2D& other, ContactManifold2D* outContact) const
        {
                const float2 diff = Sub(other.m_center, m_center);
                const float combinedRadius = m_scaledRadius + other.m_scaledRadius;
                const float distSq = LengthSq(diff);
                if (distSq > combinedRadius * combinedRadius)
                {
                        return false;
                }

                if (outContact)
                {
                        const float distance = std::sqrt(std::max(distSq, 0.0f));
                        const float penetration = combinedRadius - distance;
                        const float2 normal = (distance > 1e-4f) ? Scale(diff, 1.0f / distance) : float2{ 0.0f, 1.0f };
                        const float2 contactPoint = Add(m_center, Scale(normal, m_scaledRadius));
                        outContact->normal = normal;
                        outContact->penetration = penetration;
                        outContact->contactPoint = contactPoint;
                }

                return true;
        }

        bool CircleCollider2D::ComputeContact(const ConvexHullCollider2D& hull, ContactManifold2D* outContact) const
        {
                return hull.ComputeContact(*this, outContact);
        }

        ConvexHullCollider2D::ConvexHullCollider2D(std::vector<float2> points, float2 offset, float thickness, CollisionLayer layer)
                : Collider2DBase(thickness)
                , m_points(std::move(points))
                , m_worldPoints(m_points)
                , m_offset(offset)
                , m_rotation(0.0f)
        {
                SetLayer(layer);
        }

        bool ConvexHullCollider2D::CheckCollision(const Collision::ICollider& other)
        {
                ContactManifold2D unused{};
                const auto* other2D = dynamic_cast<const ICollider2D*>(&other);
                return other2D && ComputeContact(*other2D, unused);
        }

        void ConvexHullCollider2D::UpdateBounds(const Transform& transform)
        {
                m_rotation = transform.rotation.z;
                const float2 offsetScaled{ m_offset.x * transform.scale.x, m_offset.y * transform.scale.y };
                const float2 center{ transform.position.x + offsetScaled.x, transform.position.y + offsetScaled.y };

                float3 minBounds{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 0.0f };
                float3 maxBounds{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), 0.0f };
                m_worldPoints.clear();
                m_worldPoints.reserve(m_points.size());

                for (const float2& p : m_points)
                {
                        const float2 scaled{ p.x * transform.scale.x, p.y * transform.scale.y };
                        const float2 rotated = Rotate(scaled, m_rotation);
                        const float2 world = Add(center, rotated);
                        m_worldPoints.push_back(world);

                        minBounds.x = std::min(minBounds.x, world.x);
                        minBounds.y = std::min(minBounds.y, world.y);
                        maxBounds.x = std::max(maxBounds.x, world.x);
                        maxBounds.y = std::max(maxBounds.y, world.y);
                }

                m_aabb.min = minBounds;
                m_aabb.max = maxBounds;
                UpdateZBounds(transform, transform.scale.z);
        }

        float2 ComputeCentroid(const std::vector<float2>& points)
        {
                float2 centroid{ 0.0f, 0.0f };
                if (points.empty())
                {
                        return centroid;
                }
                for (const auto& p : points)
                {
                        centroid.x += p.x;
                        centroid.y += p.y;
                }
                const float invCount = 1.0f / static_cast<float>(points.size());
                centroid.x *= invCount;
                centroid.y *= invCount;
                return centroid;
        }

        float2 SupportPoint(const std::vector<float2>& points, const float2& direction)
        {
                if (points.empty())
                {
                        return float2{ 0.0f, 0.0f };
                }
                float bestDot = Dot(points.front(), direction);
                float2 bestPoint = points.front();
                for (const auto& p : points)
                {
                        const float d = Dot(p, direction);
                        if (d > bestDot)
                        {
                                bestDot = d;
                                bestPoint = p;
                        }
                }
                return bestPoint;
        }

        bool ConvexHullCollider2D::ComputeContact(const ICollider2D& other, ContactManifold2D& manifold) const
        {
                if (const auto* hull = dynamic_cast<const ConvexHullCollider2D*>(&other))
                {
                        return ComputeContact(*hull, &manifold);
                }
                if (const auto* circle = dynamic_cast<const CircleCollider2D*>(&other))
                {
                        return ComputeContact(*circle, &manifold);
                }
                if (const auto* compound = dynamic_cast<const CompoundCollider2D*>(&other))
                {
                        bool hit = false;
                        ContactManifold2D best{};
                        best.penetration = std::numeric_limits<float>::max();
                        for (const auto& child : compound->GetChildren())
                        {
                                const auto* child2D = dynamic_cast<const ICollider2D*>(child.get());
                                if (!child2D)
                                {
                                        continue;
                                }
                                ContactManifold2D candidate{};
                                if (ComputeContact(*child2D, candidate) && candidate.penetration < best.penetration)
                                {
                                        best = candidate;
                                        hit = true;
                                }
                        }
                        if (hit)
                        {
                                manifold = best;
                        }
                        return hit;
                }

                return false;
        }

        bool ConvexHullCollider2D::ComputeContact(const ConvexHullCollider2D& other, ContactManifold2D* outContact) const
        {
                const auto& aPoints = GetWorldPoints();
                const auto& bPoints = other.GetWorldPoints();
                if (aPoints.empty() || bPoints.empty())
                {
                        return false;
                }

                const auto satTest = [&](const std::vector<float2>& polyA, const std::vector<float2>& polyB, ContactManifold2D& bestContact) -> bool
                {
                        for (size_t i = 0; i < polyA.size(); ++i)
                        {
                                const float2 p1 = polyA[i];
                                const float2 p2 = polyA[(i + 1) % polyA.size()];
                                const float2 edge = Sub(p2, p1);
                                float2 axis = Normalize(Perp(edge));
                                if (LengthSq(axis) <= 0.0f)
                                {
                                        continue;
                                }

                                float minA = std::numeric_limits<float>::max();
                                float maxA = std::numeric_limits<float>::lowest();
                                for (const float2& v : polyA)
                                {
                                        const float proj = Dot(v, axis);
                                        minA = std::min(minA, proj);
                                        maxA = std::max(maxA, proj);
                                }

                                float minB = std::numeric_limits<float>::max();
                                float maxB = std::numeric_limits<float>::lowest();
                                for (const float2& v : polyB)
                                {
                                        const float proj = Dot(v, axis);
                                        minB = std::min(minB, proj);
                                        maxB = std::max(maxB, proj);
                                }

                                const float overlap = std::min(maxA, maxB) - std::max(minA, minB);
                                if (overlap <= 0.0f)
                                {
                                        return false;
                                }

                                if (overlap < bestContact.penetration || bestContact.penetration <= 0.0f)
                                {
                                        bestContact.penetration = overlap;
                                        bestContact.normal = axis;
                                }
                        }
                        return true;
                };

                ContactManifold2D contact{};
                if (!satTest(aPoints, bPoints, contact))
                {
                        return false;
                }
                if (!satTest(bPoints, aPoints, contact))
                {
                        return false;
                }

                const float2 centroidA = ComputeCentroid(aPoints);
                const float2 centroidB = ComputeCentroid(bPoints);
                float2 normal = Normalize(contact.normal);
                if (LengthSq(normal) <= 0.0f)
                {
                        return false;
                }
                if (Dot(Sub(centroidB, centroidA), normal) < 0.0f)
                {
                        normal = Scale(normal, -1.0f);
                }
                const float2 supportA = SupportPoint(aPoints, normal);
                const float2 supportB = SupportPoint(bPoints, Scale(normal, -1.0f));
                contact.normal = normal;
                contact.contactPoint = Scale(Add(supportA, supportB), 0.5f);

                if (outContact)
                {
                        *outContact = contact;
                }

                return true;
        }

        bool ConvexHullCollider2D::ComputeContact(const CircleCollider2D& circle, ContactManifold2D* outContact) const
        {
                const auto& hull = GetWorldPoints();
                if (hull.empty())
                {
                        return false;
                }

                ContactManifold2D bestContact{};
                bestContact.penetration = std::numeric_limits<float>::max();

                // Test polygon axes
                for (size_t i = 0; i < hull.size(); ++i)
                {
                        const float2 p1 = hull[i];
                        const float2 p2 = hull[(i + 1) % hull.size()];
                        const float2 edge = Sub(p2, p1);
                        float2 axis = Normalize(Perp(edge));
                        if (LengthSq(axis) <= 0.0f)
                        {
                                continue;
                        }

                        float minA = std::numeric_limits<float>::max();
                        float maxA = std::numeric_limits<float>::lowest();
                        for (const float2& v : hull)
                        {
                                const float proj = Dot(v, axis);
                                minA = std::min(minA, proj);
                                maxA = std::max(maxA, proj);
                        }

                        const float centerProj = Dot(circle.GetCenter(), axis);
                        const float minB = centerProj - circle.GetRadius();
                        const float maxB = centerProj + circle.GetRadius();

                        const float overlap = std::min(maxA, maxB) - std::max(minA, minB);
                        if (overlap <= 0.0f)
                        {
                                return false;
                        }
                        if (overlap < bestContact.penetration)
                        {
                                bestContact.penetration = overlap;
                                bestContact.normal = axis;
                        }
                }

                // Test circle center to closest point axis
                float2 closest = hull.front();
                float closestDistSq = LengthSq(Sub(circle.GetCenter(), closest));
                for (const float2& v : hull)
                {
                        const float distSq = LengthSq(Sub(circle.GetCenter(), v));
                        if (distSq < closestDistSq)
                        {
                                closest = v;
                                closestDistSq = distSq;
                        }
                }

                const float2 axisToCenterRaw = Sub(circle.GetCenter(), closest);
                const float2 axisToCenter = Normalize(axisToCenterRaw);
                if (LengthSq(axisToCenter) > 0.0f)
                {
                        const float centerProj = Dot(circle.GetCenter(), axisToCenter);
                        float minA = std::numeric_limits<float>::max();
                        float maxA = std::numeric_limits<float>::lowest();
                        for (const float2& v : hull)
                        {
                                const float proj = Dot(v, axisToCenter);
                                minA = std::min(minA, proj);
                                maxA = std::max(maxA, proj);
                        }

                        const float minB = centerProj - circle.GetRadius();
                        const float maxB = centerProj + circle.GetRadius();
                        const float overlap = std::min(maxA, maxB) - std::max(minA, minB);
                        if (overlap <= 0.0f)
                        {
                                return false;
                        }
                        if (overlap < bestContact.penetration)
                        {
                                bestContact.penetration = overlap;
                                bestContact.normal = axisToCenter;
                        }
                }

                float2 normal = Normalize(bestContact.normal);
                if (LengthSq(normal) <= 0.0f)
                {
                        return false;
                }
                const float2 hullCentroid = ComputeCentroid(hull);
                if (Dot(Sub(circle.GetCenter(), hullCentroid), normal) < 0.0f)
                {
                        normal = Scale(normal, -1.0f);
                }
                bestContact.normal = normal;
                const float2 hullSupport = SupportPoint(hull, normal);
                const float2 circlePoint = Add(circle.GetCenter(), Scale(normal, -circle.GetRadius()));
                bestContact.contactPoint = Scale(Add(hullSupport, circlePoint), 0.5f);

                if (outContact)
                {
                        *outContact = bestContact;
                }

                return bestContact.penetration > 0.0f;
        }

        CompoundCollider2D::CompoundCollider2D(std::vector<std::shared_ptr<Collision::ICollider>> children,
                float thickness,
                CollisionLayer layer)
                : Collider2DBase(thickness)
                , m_children(std::move(children))
        {
                SetLayer(layer);
        }

        bool CompoundCollider2D::CheckCollision(const Collision::ICollider& other)
        {
                ContactManifold2D unused{};
                const auto* other2D = dynamic_cast<const ICollider2D*>(&other);
                return other2D && ComputeContact(*other2D, unused);
        }

        void CompoundCollider2D::UpdateBounds(const Transform& transform)
        {
                float3 minBounds{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 0.0f };
                float3 maxBounds{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), 0.0f };

                for (const auto& child : m_children)
                {
                        if (!child)
                        {
                                continue;
                        }

                        child->UpdateBounds(transform);
                        const AABB childAABB = child->GetAABB();
                        minBounds.x = std::min(minBounds.x, childAABB.min.x);
                        minBounds.y = std::min(minBounds.y, childAABB.min.y);
                        minBounds.z = std::min(minBounds.z, childAABB.min.z);
                        maxBounds.x = std::max(maxBounds.x, childAABB.max.x);
                        maxBounds.y = std::max(maxBounds.y, childAABB.max.y);
                        maxBounds.z = std::max(maxBounds.z, childAABB.max.z);
                }

                m_aabb.min = minBounds;
                m_aabb.max = maxBounds;
                UpdateZBounds(transform, transform.scale.z);
        }

        bool CompoundCollider2D::ComputeContact(const ICollider2D& other, ContactManifold2D& manifold) const
        {
                bool hit = false;
                ContactManifold2D best{};
                best.penetration = std::numeric_limits<float>::max();

                for (const auto& child : m_children)
                {
                        const auto* child2D = dynamic_cast<const ICollider2D*>(child.get());
                        if (!child2D)
                        {
                                continue;
                        }

                        ContactManifold2D candidate{};
                        if (child2D->ComputeContact(other, candidate) && candidate.penetration < best.penetration)
                        {
                                best = candidate;
                                hit = true;
                        }
                }

                if (hit)
                {
                        manifold = best;
                }

                return hit;
        }
}

