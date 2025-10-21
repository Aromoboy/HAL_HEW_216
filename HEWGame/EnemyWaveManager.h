#pragma once

#include <DirectXMath.h>
#include <random>
#include <vector>

namespace Renderer3D
{
        struct Mesh;
}

struct Enemy
{
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 velocity;
        DirectX::XMFLOAT4 color;
        float radius;
        float health;
        float speed;
        int candyValue;
};

class EnemyWaveManager
{
public:
        EnemyWaveManager();

        void Initialize();
        void Reset();
        void Update(float deltaTime, const DirectX::XMFLOAT3& playerPosition);
        void Draw(const Renderer3D::Mesh& mesh) const;

        const std::vector<Enemy>& GetEnemies() const;
        std::vector<Enemy>& AccessEnemies();

        bool IsWaveCleared() const;
        void DepositCandy(int amount);
        void StartNextWaveIfReady();
        int GetCurrentWave() const;
        int GetCandyBank() const;
        int GetRequiredCandyForNextWave() const;
        bool IsAwaitingNextWave() const;

private:
        void SpawnWave(int waveIndex);
        DirectX::XMFLOAT3 GenerateSpawnPosition(float radius);

        std::vector<Enemy> m_enemies;
        int m_currentWave;
        int m_candyBank;
        bool m_waitingForNextWave;
        float m_waveCountdown;
        std::mt19937 m_rng;
};
