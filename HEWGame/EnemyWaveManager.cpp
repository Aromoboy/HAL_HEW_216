#include "EnemyWaveManager.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <sstream>

#include "Renderer3D.h"

namespace
{
        const DirectX::XMFLOAT4 kEnemyPalette[] = {
                { 0.95f, 0.35f, 0.55f, 1.0f },
                { 0.55f, 0.9f, 0.75f, 1.0f },
                { 0.95f, 0.75f, 0.35f, 1.0f },
                { 0.6f, 0.5f, 0.95f, 1.0f },
                { 0.9f, 0.55f, 0.8f, 1.0f }
        };

        int RequiredCandyForWave(int waveIndex)
        {
                return 5 + (waveIndex - 1) * 4;
        }
}

EnemyWaveManager::EnemyWaveManager()
        : m_currentWave(0)
        , m_candyBank(0)
        , m_waitingForNextWave(false)
        , m_waveCountdown(0.0f)
        , m_rng(std::random_device{}())
{
}

void EnemyWaveManager::Initialize()
{
        Reset();
        SpawnWave(1);
}

void EnemyWaveManager::Reset()
{
        m_enemies.clear();
        m_currentWave = 0;
        m_candyBank = 0;
        m_waitingForNextWave = false;
        m_waveCountdown = 0.0f;
}

void EnemyWaveManager::Update(float deltaTime, const DirectX::XMFLOAT3& playerPosition)
{
        if (deltaTime <= 0.0f)
        {
                return;
        }

        using namespace DirectX;

        if (m_enemies.empty() && !m_waitingForNextWave)
        {
                m_waitingForNextWave = true;
                m_waveCountdown = 2.0f;
                OutputDebugStringA("[EnemyWaveManager] Wave cleared. Preparing next wave...\n");
        }

        if (m_waitingForNextWave)
        {
                m_waveCountdown = std::max(0.0f, m_waveCountdown - deltaTime);
        }

        XMVECTOR target = XMLoadFloat3(&playerPosition);
        for (auto& enemy : m_enemies)
        {
                XMVECTOR pos = XMLoadFloat3(&enemy.position);
                XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(target, pos));
                XMVECTOR velocity = XMVectorScale(dir, enemy.speed);
                XMStoreFloat3(&enemy.velocity, velocity);

                enemy.position.x += enemy.velocity.x * deltaTime;
                enemy.position.y = 1.0f + 0.2f * std::sinf(enemy.position.x + enemy.position.z + deltaTime * 2.0f);
                enemy.position.z += enemy.velocity.z * deltaTime;
        }
}

void EnemyWaveManager::Draw(const Renderer3D::Mesh& mesh) const
{
        using namespace DirectX;
        for (const auto& enemy : m_enemies)
        {
                XMMATRIX rotation = XMMatrixRotationY(0.0f);
                XMMATRIX scale = XMMatrixScaling(0.9f, 0.9f, 0.9f);
                XMMATRIX translation = XMMatrixTranslation(enemy.position.x, enemy.position.y, enemy.position.z);
                Renderer3D::DrawMesh(mesh, scale * rotation * translation, enemy.color);
        }
}

const std::vector<Enemy>& EnemyWaveManager::GetEnemies() const
{
        return m_enemies;
}

std::vector<Enemy>& EnemyWaveManager::AccessEnemies()
{
        return m_enemies;
}

bool EnemyWaveManager::IsWaveCleared() const
{
        return m_enemies.empty();
}

void EnemyWaveManager::DepositCandy(int amount)
{
        m_candyBank += amount;
        std::ostringstream oss;
        oss << "[EnemyWaveManager] Candy bank increased to " << m_candyBank << "\n";
        OutputDebugStringA(oss.str().c_str());
}

void EnemyWaveManager::StartNextWaveIfReady()
{
        if (!m_waitingForNextWave || m_waveCountdown > 0.0f)
        {
                return;
        }

        int requiredCandy = GetRequiredCandyForNextWave();
        if (m_candyBank < requiredCandy)
        {
                return;
        }

        m_candyBank -= requiredCandy;
        SpawnWave(m_currentWave + 1);

        std::ostringstream oss;
        oss << "[EnemyWaveManager] Consumed " << requiredCandy << " candy to begin wave " << m_currentWave << ".\n";
        OutputDebugStringA(oss.str().c_str());
}

int EnemyWaveManager::GetCurrentWave() const
{
        return m_currentWave;
}

int EnemyWaveManager::GetCandyBank() const
{
        return m_candyBank;
}

int EnemyWaveManager::GetRequiredCandyForNextWave() const
{
        return RequiredCandyForWave(m_currentWave + 1);
}

bool EnemyWaveManager::IsAwaitingNextWave() const
{
        return m_waitingForNextWave;
}

void EnemyWaveManager::SpawnWave(int waveIndex)
{
        m_currentWave = waveIndex;
        m_enemies.clear();

        int enemyCount = 4 + waveIndex * 2;
        float spawnRadius = 12.0f + waveIndex * 1.5f;
        float baseSpeed = 1.2f + 0.3f * waveIndex;

        for (int i = 0; i < enemyCount; ++i)
        {
                Enemy enemy{};
                enemy.position = GenerateSpawnPosition(spawnRadius);
                enemy.velocity = { 0.0f, 0.0f, 0.0f };
                enemy.radius = 0.8f;
                enemy.health = 2.0f + 0.5f * waveIndex;
                enemy.speed = baseSpeed + 0.1f * (i % 3);
                enemy.candyValue = 3 + waveIndex;
                enemy.color = kEnemyPalette[i % (sizeof(kEnemyPalette) / sizeof(kEnemyPalette[0]))];
                m_enemies.emplace_back(enemy);
        }

        m_waitingForNextWave = false;
        m_waveCountdown = 0.0f;

        std::ostringstream oss;
        oss << "[EnemyWaveManager] Spawned wave " << waveIndex << " with " << enemyCount << " sweet-toothed foes.\n";
        OutputDebugStringA(oss.str().c_str());
}

DirectX::XMFLOAT3 EnemyWaveManager::GenerateSpawnPosition(float radius)
{
        std::uniform_real_distribution<float> angleDist(0.0f, DirectX::XM_2PI);
        float angle = angleDist(m_rng);
        DirectX::XMFLOAT3 position{
                std::cosf(angle) * radius,
                1.0f,
                std::sinf(angle) * radius
        };
        return position;
}
