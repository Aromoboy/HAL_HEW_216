#include "Game.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <sstream>

#include "Data.h"
#include "DirectX.h"

namespace
{
        DirectX::XMFLOAT4 SelectCandyColor(int wave)
        {
                static const DirectX::XMFLOAT4 palette[] = {
                        { 1.0f, 0.6f, 0.9f, 1.0f },
                        { 0.6f, 0.9f, 1.0f, 1.0f },
                        { 0.9f, 1.0f, 0.6f, 1.0f },
                        { 1.0f, 0.8f, 0.5f, 1.0f }
                };
                size_t index = static_cast<size_t>(wave) % (sizeof(palette) / sizeof(palette[0]));
                return palette[index];
        }
}

Game::Game()
        : m_debugLogTimer(0.0f)
        , m_totalCandy(0)
{
        if (!Renderer3D::Initialize(GetDevice(), GetContext()))
        {
                OutputDebugStringA("[Game] Failed to initialize 3D renderer.\n");
        }

        m_cubeMesh = Renderer3D::CreateCubeMesh(GetDevice());
        m_player.Initialize({ 0.0f, 1.5f, -10.0f });
        m_waveManager.Initialize();
        m_lastFrameTime = std::chrono::steady_clock::now();

        OutputDebugStringA("[Game] Candy defense initialization complete.\n");
}

Game::~Game()
{
        Renderer3D::ReleaseMesh(m_cubeMesh);
        Renderer3D::Shutdown();
}

void Game::UpdateGame()
{
        auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - m_lastFrameTime).count();
        m_lastFrameTime = now;
        deltaTime = std::min(deltaTime, 0.05f);

        m_player.Update(deltaTime);
        m_projectiles.Update(deltaTime);
        m_waveManager.Update(deltaTime, m_player.GetPosition());

        HandleShooting();
        ResolveCombat();
        UpdateEffects(deltaTime);
        m_waveManager.StartNextWaveIfReady();
        LogWaveStatus(deltaTime);
}

void Game::DrawGame()
{
        using namespace DirectX;

        float aspect = static_cast<float>(WINX) / static_cast<float>(WINY);
        XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.1f, 250.0f);
        Renderer3D::UpdateScene(m_player.GetViewMatrix(), projection, m_player.GetPosition());

        XMMATRIX groundScale = XMMatrixScaling(50.0f, 0.2f, 50.0f);
        XMMATRIX groundTranslate = XMMatrixTranslation(0.0f, -1.2f, 0.0f);
        Renderer3D::DrawMesh(m_cubeMesh, groundScale * groundTranslate, { 0.92f, 0.85f, 0.95f, 1.0f });

        Renderer3D::DrawMesh(m_cubeMesh, XMMatrixScaling(0.4f, 1.0f, 0.4f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f), { 0.8f, 0.9f, 1.0f, 1.0f });

        m_waveManager.Draw(m_cubeMesh);
        m_projectiles.DrawProjectiles(m_cubeMesh);
        DrawEffects();
}

void Game::HandleShooting()
{
        if (!m_player.ConsumeFireRequest())
        {
                return;
        }

        DirectX::XMFLOAT3 origin = m_player.GetMuzzlePosition();
        DirectX::XMFLOAT3 direction = m_player.GetForwardVector();
        int wave = std::max(1, m_waveManager.GetCurrentWave());
        DirectX::XMFLOAT4 candyColor = SelectCandyColor(wave);
        m_projectiles.SpawnProjectile(origin, direction, 25.0f, candyColor);
}

void Game::ResolveCombat()
{
        auto& projectiles = m_projectiles.AccessProjectiles();
        auto& enemies = m_waveManager.AccessEnemies();

        for (size_t p = 0; p < projectiles.size();)
        {
                bool projectileConsumed = false;
                for (auto enemyIt = enemies.begin(); enemyIt != enemies.end();)
                {
                        float dx = enemyIt->position.x - projectiles[p].position.x;
                        float dy = enemyIt->position.y - projectiles[p].position.y;
                        float dz = enemyIt->position.z - projectiles[p].position.z;
                        float distanceSq = dx * dx + dy * dy + dz * dz;
                        float radius = enemyIt->radius + projectiles[p].radius;
                        if (distanceSq <= radius * radius)
                        {
                                enemyIt->health -= 1.0f;

                                HitEffect effect;
                                effect.position = projectiles[p].position;
                                effect.color = { 1.0f, 0.95f, 0.75f, 1.0f };
                                effect.age = 0.0f;
                                effect.lifetime = 0.45f;
                                m_hitEffects.emplace_back(effect);

                                if (enemyIt->health <= 0.0f)
                                {
                                        m_totalCandy += enemyIt->candyValue;
                                        m_waveManager.DepositCandy(enemyIt->candyValue);

                                        std::ostringstream oss;
                                        oss << "[Game] Enemy defeated. Candy +" << enemyIt->candyValue << ". Total=" << m_totalCandy << "\n";
                                        OutputDebugStringA(oss.str().c_str());

                                        enemyIt = enemies.erase(enemyIt);
                                }
                                else
                                {
                                        ++enemyIt;
                                }

                                projectiles.erase(projectiles.begin() + p);
                                projectileConsumed = true;
                                break;
                        }
                        else
                        {
                                ++enemyIt;
                        }
                }

                if (!projectileConsumed)
                {
                        ++p;
                }
        }
}

void Game::UpdateEffects(float deltaTime)
{
        for (auto& effect : m_hitEffects)
        {
                effect.age += deltaTime;
        }

        m_hitEffects.erase(
                std::remove_if(m_hitEffects.begin(), m_hitEffects.end(), [](const HitEffect& effect)
                        {
                                return effect.age >= effect.lifetime;
                        }),
                m_hitEffects.end());
}

void Game::DrawEffects() const
{
        using namespace DirectX;
        for (const auto& effect : m_hitEffects)
        {
                float fade = 1.0f - (effect.age / effect.lifetime);
                XMMATRIX scale = XMMatrixScaling(0.6f * fade, 0.6f * fade, 0.6f * fade);
                XMMATRIX translation = XMMatrixTranslation(effect.position.x, effect.position.y, effect.position.z);
                DirectX::XMFLOAT4 tintedColor = effect.color;
                tintedColor.w *= fade;
                Renderer3D::DrawMesh(m_cubeMesh, scale * translation, tintedColor);
        }
}

void Game::LogWaveStatus(float deltaTime)
{
        m_debugLogTimer += deltaTime;
        if (m_debugLogTimer < 1.5f)
        {
                return;
        }
        m_debugLogTimer = 0.0f;

        std::ostringstream oss;
        oss << "[Game] Wave " << m_waveManager.GetCurrentWave()
                << " | Candy bank=" << m_waveManager.GetCandyBank()
                << " | Total candy=" << m_totalCandy
                << " | Required for next=" << m_waveManager.GetRequiredCandyForNextWave();
        if (m_waveManager.IsAwaitingNextWave())
        {
                oss << " | Awaiting next wave";
        }
        oss << "\n";
        OutputDebugStringA(oss.str().c_str());
}
