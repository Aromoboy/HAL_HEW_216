#include "Main.h"

#include <memory>

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
#include <Effekseer.h>
#include <EffekseerRendererDX11.h>

namespace
{
        Core::StepTimer g_stepTimer;
        Core::Input g_input;
        std::unique_ptr<SceneSpace::SceneManager> g_sceneManager;
        ::Effekseer::Backend::GraphicsDeviceRef g_effectDevice;
        ::EffekseerRendererDX11::RendererRef g_effectRenderer;
        ::Effekseer::ManagerRef g_effectManager;

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

        g_effectDevice = ::EffekseerRendererDX11::CreateGraphicsDevice(GetDevice(), GetContext());
        g_effectRenderer = ::EffekseerRendererDX11::Renderer::Create(g_effectDevice, 8000);
        g_effectManager = Effekseer::Manager::Create(8000);

        // 描画モジュールの設定
        g_effectManager->SetSpriteRenderer(g_effectRenderer->CreateSpriteRenderer());
        g_effectManager->SetRibbonRenderer(g_effectRenderer->CreateRibbonRenderer());
        g_effectManager->SetRingRenderer(g_effectRenderer->CreateRingRenderer());
        g_effectManager->SetTrackRenderer(g_effectRenderer->CreateTrackRenderer());
        g_effectManager->SetModelRenderer(g_effectRenderer->CreateModelRenderer());

        // テクスチャ、モデル、カーブ、マテリアルローダーの設定する
        // ユーザーが独自で拡張できる。現在はファイルから読み込んでいる
        g_effectManager->SetTextureLoader(g_effectRenderer->CreateTextureLoader());
        g_effectManager->SetModelLoader(g_effectRenderer->CreateModelLoader());
        g_effectManager->SetMaterialLoader(g_effectRenderer->CreateMaterialLoader());
        g_effectManager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>());

        // シーン
        g_sceneManager = std::make_unique<SceneSpace::SceneManager>(g_stepTimer, g_input);
        g_sceneManager->SetInitialScene(std::make_unique<TitleScene>(*g_sceneManager));

        LogBootScenario();

        return hr;
}

void Uninit()
{
        g_input.Shutdown();

        g_sceneManager.reset();

        g_effectManager.Reset();
        g_effectRenderer.Reset();
        g_effectDevice.Reset();

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

        if (g_sceneManager)
        {
                g_sceneManager->Draw();
        }
        EndDrawDirectX();
}

