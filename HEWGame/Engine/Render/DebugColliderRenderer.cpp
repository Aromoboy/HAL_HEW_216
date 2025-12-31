// デバッグ用にコライダー形状を描画する
// 開発中の表示確認から呼ばれる

#include "DebugColliderRenderer.h"

#include <cmath>
#include <DirectXMath.h>

#include "Geometory.h"

namespace
{
        DirectX::XMFLOAT4 ToFloat4(const Color& c)
        {
                return DirectX::XMFLOAT4{ c.r, c.g, c.b, c.a };
        }

        DirectX::XMFLOAT3 ToFloat3(const float2& point, float z)
        {
                return DirectX::XMFLOAT3{ point.x, point.y, z };
        }
}

namespace DebugColliderRenderer
{
        void DrawAABB(const AABB& bounds, Color c)
        {
                const float z = (bounds.min.z + bounds.max.z) * 0.5f;
                const DirectX::XMFLOAT3 leftUp{ bounds.min.x, bounds.max.y, z };
                const DirectX::XMFLOAT3 rightUp{ bounds.max.x, bounds.max.y, z };
                const DirectX::XMFLOAT3 leftDown{ bounds.min.x, bounds.min.y, z };
                const DirectX::XMFLOAT3 rightDown{ bounds.max.x, bounds.min.y, z };
                const DirectX::XMFLOAT4 color = ToFloat4(c);

                Geometory::AddLine(leftUp, leftDown, color);
                Geometory::AddLine(leftUp, rightUp, color);
                Geometory::AddLine(rightDown, leftDown, color);
                Geometory::AddLine(rightDown, rightUp, color);
        }

        void DrawHull(const std::vector<float2>& pts, float z, Color c)
        {
                if (pts.size() < 2)
                {
                        return;
                }

                const DirectX::XMFLOAT4 color = ToFloat4(c);
                for (size_t i = 0; i < pts.size(); ++i)
                {
                        const size_t next = (i + 1) % pts.size();
                        Geometory::AddLine(ToFloat3(pts[i], z), ToFloat3(pts[next], z), color);
                }
        }

        void DrawCircle(float2 center, float radius, float z, Color c)
        {
                constexpr int kSegments = 32;
                const float step = DirectX::XM_2PI / static_cast<float>(kSegments);
                const DirectX::XMFLOAT4 color = ToFloat4(c);

                for (int i = 0; i < kSegments; ++i)
                {
                        const float startAngle = step * static_cast<float>(i);
                        const float endAngle = step * static_cast<float>(i + 1);
                        const DirectX::XMFLOAT3 start{
                                center.x + std::cos(startAngle) * radius,
                                center.y + std::sin(startAngle) * radius,
                                z };
                        const DirectX::XMFLOAT3 end{
                                center.x + std::cos(endAngle) * radius,
                                center.y + std::sin(endAngle) * radius,
                                z };
                        Geometory::AddLine(start, end, color);
                }
        }

        void DrawCapsule(float2 p0, float2 p1, float radius, float z, Color c)
        {
                const float dx = p1.x - p0.x;
                const float dy = p1.y - p0.y;
                const float length = std::sqrt(dx * dx + dy * dy);

                float2 axis{ 0.0f, 1.0f };
                if (length > 1e-4f)
                {
                        const float invLen = 1.0f / length;
                        axis.x = dx * invLen;
                        axis.y = dy * invLen;
                }
                const float2 normal{ -axis.y, axis.x };
                const float2 offset{ normal.x * radius, normal.y * radius };
                const DirectX::XMFLOAT4 color = ToFloat4(c);

                const float2 a0{ p0.x + offset.x, p0.y + offset.y };
                const float2 a1{ p1.x + offset.x, p1.y + offset.y };
                const float2 b0{ p0.x - offset.x, p0.y - offset.y };
                const float2 b1{ p1.x - offset.x, p1.y - offset.y };

                Geometory::AddLine(ToFloat3(a0, z), ToFloat3(a1, z), color);
                Geometory::AddLine(ToFloat3(b0, z), ToFloat3(b1, z), color);

                const float startAngle = std::atan2(axis.y, axis.x);
                const float step = DirectX::XM_PI / 16.0f;

                for (int i = 0; i <= 16; ++i)
                {
                        const float angle0 = startAngle - DirectX::XM_PIDIV2 + step * static_cast<float>(i);
                        const float angle1 = startAngle - DirectX::XM_PIDIV2 + step * static_cast<float>(i + 1);
                        const DirectX::XMFLOAT3 arcStart{
                                p1.x + std::cos(angle0) * radius,
                                p1.y + std::sin(angle0) * radius,
                                z };
                        const DirectX::XMFLOAT3 arcEnd{
                                p1.x + std::cos(angle1) * radius,
                                p1.y + std::sin(angle1) * radius,
                                z };
                        Geometory::AddLine(arcStart, arcEnd, color);
                }

                for (int i = 0; i <= 16; ++i)
                {
                        const float angle0 = startAngle + DirectX::XM_PIDIV2 + step * static_cast<float>(i);
                        const float angle1 = startAngle + DirectX::XM_PIDIV2 + step * static_cast<float>(i + 1);
                        const DirectX::XMFLOAT3 arcStart{
                                p0.x + std::cos(angle0) * radius,
                                p0.y + std::sin(angle0) * radius,
                                z };
                        const DirectX::XMFLOAT3 arcEnd{
                                p0.x + std::cos(angle1) * radius,
                                p0.y + std::sin(angle1) * radius,
                                z };
                        Geometory::AddLine(arcStart, arcEnd, color);
                }
        }
}

