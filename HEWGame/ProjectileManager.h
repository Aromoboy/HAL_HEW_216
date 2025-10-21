#pragma once

#include <DirectXMath.h>
#include <vector>

namespace Renderer3D
{
        struct Mesh;
}

struct Projectile
{
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 velocity;
        DirectX::XMFLOAT4 color;
        float radius;
        float lifetime;
        float age;
};

class ProjectileManager
{
public:
        void SpawnProjectile(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction, float speed, const DirectX::XMFLOAT4& color);
        void Update(float deltaTime);
        void Reset();

        const std::vector<Projectile>& GetProjectiles() const;
        std::vector<Projectile>& AccessProjectiles();

        void DrawProjectiles(const Renderer3D::Mesh& mesh) const;

private:
        std::vector<Projectile> m_projectiles;
};
