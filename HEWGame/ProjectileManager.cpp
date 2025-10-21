#include "ProjectileManager.h"

#include <Windows.h>
#include <algorithm>
#include <sstream>

#include "Renderer3D.h"

void ProjectileManager::SpawnProjectile(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction, float speed, const DirectX::XMFLOAT4& color)
{
        using namespace DirectX;
        XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&direction));
        XMFLOAT3 normalizedDir{};
        XMStoreFloat3(&normalizedDir, dir);

        Projectile projectile{};
        projectile.position = position;
        projectile.velocity = { normalizedDir.x * speed, normalizedDir.y * speed, normalizedDir.z * speed };
        projectile.color = color;
        projectile.radius = 0.18f;
        projectile.lifetime = 3.0f;
        projectile.age = 0.0f;

        m_projectiles.emplace_back(projectile);

        std::ostringstream oss;
        oss << "[ProjectileManager] Spawned candy projectile. Active count=" << m_projectiles.size() << "\n";
        OutputDebugStringA(oss.str().c_str());
}

void ProjectileManager::Update(float deltaTime)
{
        if (deltaTime <= 0.0f)
        {
                return;
        }

        for (auto& projectile : m_projectiles)
        {
                projectile.age += deltaTime;
                projectile.position.x += projectile.velocity.x * deltaTime;
                projectile.position.y += projectile.velocity.y * deltaTime;
                projectile.position.z += projectile.velocity.z * deltaTime;
        }

        m_projectiles.erase(
                std::remove_if(m_projectiles.begin(), m_projectiles.end(), [](const Projectile& projectile)
                        {
                                return projectile.age >= projectile.lifetime;
                        }),
                m_projectiles.end());
}

void ProjectileManager::Reset()
{
        m_projectiles.clear();
}

const std::vector<Projectile>& ProjectileManager::GetProjectiles() const
{
        return m_projectiles;
}

std::vector<Projectile>& ProjectileManager::AccessProjectiles()
{
        return m_projectiles;
}

void ProjectileManager::DrawProjectiles(const Renderer3D::Mesh& mesh) const
{
        using namespace DirectX;
        for (const auto& projectile : m_projectiles)
        {
                XMMATRIX scale = XMMatrixScaling(0.25f, 0.25f, 0.25f);
                XMMATRIX translation = XMMatrixTranslation(projectile.position.x, projectile.position.y, projectile.position.z);
                Renderer3D::DrawMesh(mesh, scale * translation, projectile.color);
        }
}
