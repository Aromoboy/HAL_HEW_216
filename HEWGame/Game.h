#ifndef ___GAME_H___
#define ___GAME_H___

#include <DirectXMath.h>
#include <chrono>
#include <vector>

#include "EnemyWaveManager.h"
#include "Player.h"
#include "ProjectileManager.h"
#include "Renderer3D.h"

class Game
{
public:
        Game();
        ~Game();

        void UpdateGame();
        void DrawGame();

private:
        struct HitEffect
        {
                DirectX::XMFLOAT3 position;
                DirectX::XMFLOAT4 color;
                float age;
                float lifetime;
        };

        void HandleShooting();
        void ResolveCombat();
        void UpdateEffects(float deltaTime);
        void DrawEffects() const;
        void LogWaveStatus(float deltaTime);

        Renderer3D::Mesh m_cubeMesh;
        Player m_player;
        ProjectileManager m_projectiles;
        EnemyWaveManager m_waveManager;
        std::vector<HitEffect> m_hitEffects;

        std::chrono::steady_clock::time_point m_lastFrameTime;
        float m_debugLogTimer;
        int m_totalCandy;
};

#endif // !___GAME_H___
