#include "Main.h"

#include <cmath>
#include <memory>

#include "DirectX.h"
#include "Geometory.h"
#include "Sprite.h"
#include "SceneGame.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include "ScenePowerUp.h"
#include "Defines.h"
#include "ShaderList.h"
#include "DebugConsole.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"
#include "Diagnostics/Logger.h"
#include "FullscreenManager.h"

namespace
{
        Core::StepTimer g_stepTimer;
        Core::Input g_input;
        std::unique_ptr<SceneSpace::SceneManager> g_sceneManager;

        void LogBootScenario()
        {
                Diagnostics::LogInfo(
                        "[Scenario] startup -> load SceneGame -> spawn 5-10 sweets -> single gameplay loop -> exit request");
                Diagnostics::LogInfo(
                        "[Scenario] success: no hangs across 3 runs, platform visible, >=3 sweets types readable, player stable");
        }
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
        Geometory::Init();
        Sprite::Init();
        ShaderList::Init();

        // シーン
        g_sceneManager = std::make_unique<SceneSpace::SceneManager>(g_stepTimer, g_input);
        g_sceneManager->SetInitialScene(std::make_unique<TitleScene>(*g_sceneManager));
        g_pScene = new SceneSpace::ScenePowerUp;

        LogBootScenario();

        return hr;
}

void Uninit()
{
        g_input.Shutdown();

        g_sceneManager.reset();

        ShaderList::Uninit();
        Sprite::Uninit();
        Geometory::Uninit();
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

        if (g_input.IsTrigger(Core::ActionId::ToggleFullscreen))
        {
                FullscreenManager::Toggle();
        }
}

void Draw()
{
        BeginDrawDirectX();

        // 軸線の表示
#ifdef _DEBUG
        // グリッド
        DirectX::XMFLOAT4 lineColor(0.5f, 0.5f, 0.5f, 1.0f);
        float size = DEBUG_GRID_NUM * DEBUG_GRID_MARGIN;
        for (int i = 1; i <= DEBUG_GRID_NUM; ++i)
        {
                //float grid = i * DEBUG_GRID_MARGIN;
                //DirectX::XMFLOAT3 pos[2] = {
                //        DirectX::XMFLOAT3(grid, 0.0f, size),
                //        DirectX::XMFLOAT3(grid, 0.0f, -size),
                //};
                //Geometory::AddLine(pos[0], pos[1], lineColor);
                //pos[0].x = pos[1].x = -grid;
                //Geometory::AddLine(pos[0], pos[1], lineColor);
                //pos[0].x = size;
                //pos[1].x = -size;
                //pos[0].z = pos[1].z = grid;
                //Geometory::AddLine(pos[0], pos[1], lineColor);
                //pos[0].z = pos[1].z = -grid;
                //Geometory::AddLine(pos[0], pos[1], lineColor);
        }
        //// 軸
        //Geometory::AddLine(DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT3(size, 0, 0), DirectX::XMFLOAT4(1, 0, 0, 1));
        //Geometory::AddLine(DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT3(0, size, 0), DirectX::XMFLOAT4(0, 1, 0, 1));
        //Geometory::AddLine(DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT3(0, 0, size), DirectX::XMFLOAT4(0, 0, 1, 1));
        //Geometory::AddLine(DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT3(-size, 0, 0), DirectX::XMFLOAT4(0, 0, 0, 1));
        //Geometory::AddLine(DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT3(0, 0, -size), DirectX::XMFLOAT4(0, 0, 0, 1));

        Geometory::DrawLines();

        // カメラの値
        static bool camAutoSwitch = false;
        static bool camUpDownSwitch = true;
        static float camAutoRotate = 1.0f;
        if (g_input.IsTrigger(Core::ActionId::CameraToggle))
        {
                camAutoSwitch ^= true;
        }
        if (g_input.IsTrigger(Core::ActionId::CameraHeightToggle))
        {
                camUpDownSwitch ^= true;
        }

        DirectX::XMVECTOR camPos;
        if (camAutoSwitch)
        {
                camAutoRotate += 0.01f;
        }
        camPos = DirectX::XMVectorSet(
                cosf(camAutoRotate) * 5.0f,
                3.5f * (camUpDownSwitch ? 1.0f : -1.0f),
                sinf(camAutoRotate) * 5.0f,
                0.0f);

        // ジオメトリ用カメラ初期化
        DirectX::XMFLOAT4X4 mat[2];
        DirectX::XMStoreFloat4x4(&mat[0], DirectX::XMMatrixTranspose(
                DirectX::XMMatrixLookAtLH(
                        camPos,
                        DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
                        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
                )));
        DirectX::XMStoreFloat4x4(&mat[1], DirectX::XMMatrixTranspose(
                DirectX::XMMatrixPerspectiveFovLH(
                        DirectX::XMConvertToRadians(60.0f), static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, 0.1f, 1000.0f)
        ));
        Geometory::SetView(mat[0]);
        Geometory::SetProjection(mat[1]);
#endif

        if (g_sceneManager)
        {
                g_sceneManager->Draw();
        }
        EndDrawDirectX();
}

