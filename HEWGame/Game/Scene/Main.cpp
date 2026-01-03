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
#include "AppGame.h"
#include "SceneGame.h"
#include "DebugConsole.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"
#include "Diagnostics/Logger.h"
#include "FullscreenManager.h"
#include "ShaderCache.h"

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
        ShaderCache::Instance().Load("postprocess", GetDevice(), "shaders/postprocess_vs.cso", "shaders/postprocess_ps.cso");

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

        auto& dxContext = App::Game::Instance().GetDXContext();
        ID3D11DeviceContext* context = dxContext.GetContext();
        ID3D11RenderTargetView* sceneRTV = dxContext.GetSceneColorRTV();
        ID3D11DepthStencilView* sceneDSV = dxContext.GetSceneDepthDSV();

        if (context && sceneRTV)
        {
                context->OMSetRenderTargets(1, &sceneRTV, sceneDSV);

                D3D11_VIEWPORT viewport = {};
                viewport.TopLeftX = 0.0f;
                viewport.TopLeftY = 0.0f;
                viewport.Width = static_cast<float>(dxContext.GetWidth());
                viewport.Height = static_cast<float>(dxContext.GetHeight());
                viewport.MinDepth = 0.0f;
                viewport.MaxDepth = 1.0f;
                context->RSSetViewports(1, &viewport);

                const float clearColor[4] = { 0.8f, 0.9f, 1.0f, 1.0f };
                context->ClearRenderTargetView(sceneRTV, clearColor);
                if (sceneDSV)
                {
                        context->ClearDepthStencilView(sceneDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
                }
        }

        if (g_sceneManager)
        {
                g_sceneManager->Draw();
        }

        bool sceneHandlesPostprocess = false;
        if (g_sceneManager)
        {
                if (auto* activeScene = g_sceneManager->GetActiveScene())
                {
                        sceneHandlesPostprocess = dynamic_cast<SceneGame*>(activeScene) != nullptr;
                }
        }

        if (context && !sceneHandlesPostprocess)
        {
                RenderTarget* defaultRTV = GetDefaultRTV();
                SetRenderTargets(1, &defaultRTV, nullptr);

                auto postprocess = ShaderCache::Instance().Get("postprocess");
                postprocess->Bind(context);

                ID3D11ShaderResourceView* sceneSRV = dxContext.GetSceneColorSRV();
                context->PSSetShaderResources(0, 1, &sceneSRV);
                SetSamplerState(SAMPLER_LINEAR);
                BindPostProcessParams();

                context->IASetInputLayout(nullptr);
                context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                context->Draw(3, 0);

                ID3D11ShaderResourceView* nullSRV = nullptr;
                context->PSSetShaderResources(0, 1, &nullSRV);
        }

        EndDrawDirectX();
}
