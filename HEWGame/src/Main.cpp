#include "Main.h"

#include <cmath>
#include <memory>

#include "DirectX.h"
#include "Sprite.h"
#include "SceneGame.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include "ScenePowerUp.h"
#include "Defines.h"
#include "DebugConsole.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"

namespace
{
        Core::StepTimer g_stepTimer;
        Core::Input g_input;
        std::unique_ptr<SceneSpace::SceneManager> g_sceneManager;
}

SceneSpace::Scene* g_pScene;

HRESULT Init(HWND hWnd, UINT width, UINT height)
{
        HRESULT hr = InitDirectX(hWnd, width, height, false);
        if (FAILED(hr))
        {
                return hr;
        }

        hr = g_input.Initialize();
        if (FAILED(hr))
        {
                return hr;
        }

        g_stepTimer.Reset();

        // 他機能初期化
        Sprite::Init();

        // シーン
        g_sceneManager = std::make_unique<SceneSpace::SceneManager>(g_stepTimer, g_input);
        g_sceneManager->SetInitialScene(std::make_unique<TitleScene>(*g_sceneManager));
        g_pScene = new SceneSpace::ScenePowerUp;

        return hr;
}

void Uninit()
{
        g_input.Shutdown();

        g_sceneManager.reset();

        Sprite::Uninit();
        UninitDirectX();
}

void Update()
{
        if (g_sceneManager)
        {
                g_sceneManager->Update();
        }

#ifdef _DEBUG
        if (g_input.IsTrigger(Core::ActionId::DebugConsole)) DebugConsole::ToggleConsole();
#endif
}

void Draw()
{
        BeginDrawDirectX();

#ifdef _DEBUG
#endif

        if (g_sceneManager)
        {
                g_sceneManager->Draw();
        }
        EndDrawDirectX();
}

