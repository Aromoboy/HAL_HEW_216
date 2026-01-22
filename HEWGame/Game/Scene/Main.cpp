#include "Main.h"

#include <array>
#include <filesystem>
#include <memory>
#include <objbase.h>
#include <sstream>
#include <Windows.h>
#include <Effekseer.h>
#include <EffekseerRendererDX11.h>

#include "DirectX.h"
#include "Geometory.h"
#include "Sprite.h"
#include "SceneManager.h"
#include "Scene.h"
#include "SoundManagerEX.h" 
#include "TitleScene.h"
#include "Defines.h"
#include "ShaderList.h"
#include "AppGame.h"
#include "SceneGame.h"
#include "PerspectiveCamera.h"
#include "DebugConsole.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"
#include "Diagnostics/Logger.h"
#include "FullscreenManager.h"
#include "ShaderCache.h"
#include "SceneGameConfigLoader.h"
#include "Texture.h"

namespace
{
        Core::StepTimer g_stepTimer;
        Core::Input g_input;
        std::unique_ptr<SceneSpace::SceneManager> g_sceneManager;
        HRESULT g_comInitResult = E_FAIL;
        SceneGameConfig::RenderPostProcessSettings g_postProcessSettings{};
        SceneGameConfig::RenderFadeSettings g_renderFadeSettings{};
        std::unique_ptr<Texture> g_fadeTexture;
        Effekseer::Backend::GraphicsDeviceRef g_effectDevice;
        EffekseerRendererDX11::RendererRef g_effectRenderer;
        Effekseer::ManagerRef g_effectManager;
        std::unique_ptr<SoundManagerEX> g_soundManager;

        std::filesystem::path GetExecutableDirectory()
        {
                std::array<char, MAX_PATH> buffer{};
                const DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
                if (length == 0 || length == buffer.size())
                {
                        return std::filesystem::current_path();
                }

                return std::filesystem::path(buffer.data()).parent_path();
        }

        void LogBootScenario()
        {
                Diagnostics::LogInfo(
                        "[Scenario] startup -> load TitleScene -> spawn 5-10 sweets -> single gameplay loop -> exit request");
                Diagnostics::LogInfo(
                        "[Scenario] success: no hangs across 3 runs, platform visible, >=3 sweets types readable, player stable");
        }

        void DrawSceneFadeOverlay(float alpha)
        {
                if (!g_renderFadeSettings.enable || alpha <= 0.0f || !g_fadeTexture)
                {
                        return;
                }

                RenderTarget* defaultRTV = GetDefaultRTV();
                if (defaultRTV)
                {
                        SetRenderTargets(1, &defaultRTV, nullptr);
                }

                SetDepthTest(false);
                SetBlendMode(BLEND_ALPHA);

                Sprite::ScopedPassDefaults guard;
                Sprite::SetWorld(DirectX::XMFLOAT4X4(
                        1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f));
                Sprite::SetView(DirectX::XMFLOAT4X4(
                        1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f));
                Sprite::SetProjection(DirectX::XMFLOAT4X4(
                        1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f));
                Sprite::SetTexture(g_fadeTexture.get());
                Sprite::SetUVPos(DirectX::XMFLOAT2(0.0f, 0.0f));
                Sprite::SetUVScale(DirectX::XMFLOAT2(1.0f, 1.0f));
                Sprite::SetColor(DirectX::XMFLOAT4(
                        g_renderFadeSettings.color.x,
                        g_renderFadeSettings.color.y,
                        g_renderFadeSettings.color.z,
                        alpha));
                Sprite::SetSize(DirectX::XMFLOAT2(2.0f, 2.0f));
                Sprite::SetOffset(DirectX::XMFLOAT2(0.0f, 0.0f));
                Sprite::Draw();

                SetDepthTest(true);
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

        g_soundManager = std::make_unique<SoundManagerEX>();
        Diagnostics::LogInfo("[Sound] SoundManagerEX initialized");

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
        g_fadeTexture = std::make_unique<Texture>();
        const uint32_t solidWhite = 0xFFFFFFFF;
        if (FAILED(g_fadeTexture->Create(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &solidWhite)))
        {
                g_fadeTexture.reset();
        }
        const auto shaderDir = GetExecutableDirectory() / "assets" / "shaders";
        ShaderCache::Instance().Load(
                "postprocess",
                GetDevice(),
                shaderDir / "postprocess_vs.cso",
                shaderDir / "postprocess_ps.cso");
        g_postProcessSettings = SceneGameConfig::LoadRenderPostProcessSettings();
        g_renderFadeSettings = SceneGameConfig::LoadRenderFadeSettings();

        g_effectDevice = ::EffekseerRendererDX11::CreateGraphicsDevice(GetDevice(), GetContext());
        g_effectRenderer = ::EffekseerRendererDX11::Renderer::Create(g_effectDevice, 8000);
        g_effectManager = ::Effekseer::Manager::Create(8000);

        g_effectManager->SetSpriteRenderer(g_effectRenderer->CreateSpriteRenderer());
        g_effectManager->SetRibbonRenderer(g_effectRenderer->CreateRibbonRenderer());
        g_effectManager->SetRingRenderer(g_effectRenderer->CreateRingRenderer());
        g_effectManager->SetTrackRenderer(g_effectRenderer->CreateTrackRenderer());
        g_effectManager->SetModelRenderer(g_effectRenderer->CreateModelRenderer());

        g_effectManager->SetTextureLoader(g_effectRenderer->CreateTextureLoader());
        g_effectManager->SetModelLoader(g_effectRenderer->CreateModelLoader());
        g_effectManager->SetMaterialLoader(g_effectRenderer->CreateMaterialLoader());
        g_effectManager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>());


        // シーン
        g_sceneManager = std::make_unique<SceneSpace::SceneManager>(g_stepTimer, g_input, g_effectRenderer, g_effectManager);
        g_sceneManager->ConfigureRenderFade(g_renderFadeSettings);
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
        g_fadeTexture.reset();

        g_effectManager.Reset();
        g_effectRenderer.Reset();
        g_effectDevice.Reset();
        g_soundManager.reset();
        Diagnostics::LogInfo("[Sound] SoundManagerEX shutdown");

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
                float cameraNear = 0.1f;
                float cameraFar = 1000.0f;
                if (g_sceneManager)
                {
                        if (auto* activeScene = dynamic_cast<SceneSpace::Scene*>(g_sceneManager->GetActiveScene()))
                        {
                                auto activeCamera = activeScene->GetActiveCamera();
                                if (auto perspectiveCamera = std::dynamic_pointer_cast<CameraSpace::PerspectiveCamera>(activeCamera))
                                {
                                        cameraNear = perspectiveCamera->GetNearPlane();
                                        cameraFar = perspectiveCamera->GetFarPlane();
                                }
                        }
                }

                RenderTarget* defaultRTV = GetDefaultRTV();
                SetRenderTargets(1, &defaultRTV, nullptr);

                auto postprocess = ShaderCache::Instance().Get("postprocess");
                postprocess->Bind(context);

                ID3D11ShaderResourceView* sceneSRV = dxContext.GetSceneColorSRV();
                ID3D11ShaderResourceView* sceneDepthSRV = dxContext.GetSceneDepthSRV();
                context->PSSetShaderResources(0, 1, &sceneSRV);
                context->PSSetShaderResources(1, 1, &sceneDepthSRV);
                SetSamplerState(SAMPLER_POSTPROCESS);
                const auto& postSettings = g_postProcessSettings;
                DirectX::XMFLOAT4 fogColor = DirectX::XMFLOAT4(
                        postSettings.fogColor.x,
                        postSettings.fogColor.y,
                        postSettings.fogColor.z,
                        1.0f);
                float fogStart = postSettings.fogStart;
                float fogRange = postSettings.fogRange;
                float vignetteStrength = postSettings.vignetteStrength;
                float bloomIntensity = postSettings.bloomIntensity;
                float invWidth = 0.0f;
                float invHeight = 0.0f;
                if (auto defaultRTV = GetDefaultRTV())
                {
                        const UINT screenWidth = defaultRTV->GetWidth();
                        const UINT screenHeight = defaultRTV->GetHeight();
                        invWidth = screenWidth > 0 ? 1.0f / static_cast<float>(screenWidth) : 0.0f;
                        invHeight = screenHeight > 0 ? 1.0f / static_cast<float>(screenHeight) : 0.0f;
                }
                const DirectX::XMFLOAT2 invScreenSize(invWidth, invHeight);
                float postTime = static_cast<float>(g_stepTimer.FrameIndex()) * g_stepTimer.DeltaTime();

                if (!postSettings.enableFog)
                {
                        fogStart = cameraFar + 1.0f;
                        fogRange = 1.0f;
                }
                if (!postSettings.enableVignette)
                {
                        vignetteStrength = 0.0f;
                }
                if (!postSettings.enableBloom)
                {
                        bloomIntensity = 0.0f;
                }

                BindPostProcessParams(
                        postSettings.gamma,
                        postSettings.saturation,
                        postSettings.contrast,
                        fogColor,
                        fogStart,
                        fogRange,
                        cameraNear,
                        cameraFar,
                        vignetteStrength,
                        bloomIntensity,
                        postSettings.bloomThreshold,
                        invScreenSize,
                        postSettings.fxaaEdgeThreshold,
                        postSettings.fxaaEdgeThresholdMin,
                        postSettings.fxaaSubpixelQuality,
                        postSettings.enableFxaa ? 1 : 0,
                        postTime,
                        postSettings.grainAmount,
                        postSettings.grainEnable ? 1 : 0,
                        postSettings.grainAnimate ? 1 : 0);

                context->IASetInputLayout(nullptr);
                context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                context->Draw(3, 0);

                ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
                context->PSSetShaderResources(0, 2, nullSRVs);
        }

        if (g_sceneManager)
        {
                DrawSceneFadeOverlay(g_sceneManager->GetFadeAlpha());
        }

        EndDrawDirectX();
}
