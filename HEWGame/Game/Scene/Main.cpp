#include "Main.h"

#include <memory>
#include <objbase.h>
#include <sstream>

#include "DirectX.h"
#include "Geometory.h"
#include "Sprite.h"
#include "SceneManager.h"
#include "TitleScene.h"
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
        HRESULT g_comInitResult = E_FAIL;

        void LogBootScenario()
        {
                Diagnostics::LogInfo(
                        "[Scenario] startup -> load TitleScene -> spawn 5-10 sweets -> single gameplay loop -> exit request");
                Diagnostics::LogInfo(
                        "[Scenario] success: no hangs across 3 runs, platform visible, >=3 sweets types readable, player stable");
        }

}

HRESULT Init(HWND hWnd, UINT width, UINT height)
{
        g_comInitResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(g_comInitResult))
        {
                std::ostringstream oss;
                oss << "[COM] CoInitializeEx failed. hr=0x" << std::hex << g_comInitResult;
                Diagnostics::LogInfo(oss.str());
                return g_comInitResult;
        }

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

        LogBootScenario();

        return hr;
}

HRESULT GetComInitResult()
{
        return g_comInitResult;
}

void Uninit()
{
        g_input.Shutdown();

        g_sceneManager.reset();

        ShaderList::Uninit();
        Sprite::Uninit();
        Geometory::Uninit();
        UninitDirectX();

        if (SUCCEEDED(g_comInitResult))
        {
                CoUninitialize();
                g_comInitResult = E_FAIL;
        }
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

        if (g_input.IsTrigger(VK_ESCAPE) || g_input.IsTrigger(Core::ActionId::Cancel))
        {
                std::ostringstream oss;
                oss << "[Input] Exit hotkey triggered at frame " << g_stepTimer.FrameIndex();
                Diagnostics::LogInfo(oss.str());
        }
}

void Draw()
{
        BeginDrawDirectX();

        if (g_sceneManager)
        {
                g_sceneManager->Draw();
        }
        EndDrawDirectX();
}
