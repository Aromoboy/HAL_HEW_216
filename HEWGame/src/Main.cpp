#include "Main.h"
#include <memory>
#include <limits>
#include "DirectX.h"
#include "Geometory.h"
#include "Sprite.h"
#include "Input.h"
#include "Core/UpdateScheduler.h"
#include "SceneGame.h"
#include "Defines.h"
#include "ShaderList.h"

#include "DebugConsole.h"

namespace
{
        class InputUpdateSystem final : public Core::IUpdatable
        {
        public:
                void OnUpdate(const Core::UpdateContext& context) override
                {
                        UpdateInput();
#ifdef _DEBUG
                        auto trigger = context.input.isKeyTrigger ? context.input.isKeyTrigger : &IsKeyTrigger;
                        if (trigger && trigger(VK_F1))
                        {
                                DebugConsole::ToggleConsole();
                        }
#endif
                }
        };

        InputUpdateSystem g_inputUpdateSystem;
        DWORD g_prevTick = 0;
        double g_totalSeconds = 0.0;
        unsigned long long g_frameIndex = 0;
}

Scene* g_pScene = nullptr;

HRESULT Init(HWND hWnd, UINT width, UINT height)
{
        HRESULT hr;
        // DirectX
        hr = InitDirectX(hWnd, width, height, false);
        if (FAILED(hr)) { return hr; }

        // 各種初期化
        Geometory::Init();
        Sprite::Init();
        InitInput();
        ShaderList::Init();

        g_prevTick = timeGetTime();
        g_totalSeconds = 0.0;
        g_frameIndex = 0;

        // シーン生成
        g_pScene = new SceneGame();
        if (g_pScene)
        {
                g_pScene->RegisterUpdatable(&g_inputUpdateSystem, Core::UpdatePhase::Input, std::numeric_limits<int>::min());
        }

        return hr;
}

void Uninit()
{
        if (g_pScene)
        {
                g_pScene->UnregisterUpdatable(&g_inputUpdateSystem);
                delete g_pScene;
                g_pScene = nullptr;
        }

        ShaderList::Uninit();
        UninitInput();
        Sprite::Uninit();
        Geometory::Uninit();
        UninitDirectX();
}

void Update()
{
        Scene* activeScene = g_pScene;
        if (!activeScene)
        {
                return;
        }

        DWORD now = timeGetTime();
        if (g_prevTick == 0)
        {
                        g_prevTick = now;
        }

        float deltaSeconds = static_cast<float>(now - g_prevTick) * 0.001f;
        g_prevTick = now;
        if (deltaSeconds < 0.0f)
        {
                deltaSeconds = 0.0f;
        }

        g_totalSeconds += static_cast<double>(deltaSeconds);

        Core::UpdateContext context;
        context.deltaSeconds = deltaSeconds;
        context.timer.totalSeconds = g_totalSeconds;
        context.timer.frameIndex = g_frameIndex++;
        context.input.isKeyPress = &IsKeyPress;
        context.input.isKeyTrigger = &IsKeyTrigger;
        context.input.isKeyRelease = &IsKeyRelease;
        context.input.isKeyRepeat = &IsKeyRepeat;
        context.activeScene = activeScene;

        activeScene->GetScheduler().Tick(context);
}

void Draw()
{
        BeginDrawDirectX();

        // 3D 描画
#ifdef _DEBUG
        // グリッド
        DirectX::XMFLOAT4 lineColor(0.5f, 0.5f, 0.5f, 1.0f);
        float size = DEBUG_GRID_NUM * DEBUG_GRID_MARGIN;
        for (int i = 1; i <= DEBUG_GRID_NUM; ++i)
        {
                float grid = i * DEBUG_GRID_MARGIN;
                DirectX::XMFLOAT3 pos[2] = {
                        DirectX::XMFLOAT3(grid, 0.0f, size),
                        DirectX::XMFLOAT3(grid, 0.0f,-size),
                };
                Geometory::AddLine(pos[0], pos[1], lineColor);
                pos[0].x = pos[1].x = -grid;
                Geometory::AddLine(pos[0], pos[1], lineColor);
                pos[0].x = size;
                pos[1].x = -size;
                pos[0].z = pos[1].z = grid;
                Geometory::AddLine(pos[0], pos[1], lineColor);
                pos[0].z = pos[1].z = -grid;
                Geometory::AddLine(pos[0], pos[1], lineColor);
        }
        // 軸
        Geometory::AddLine(DirectX::XMFLOAT3(0,0,0), DirectX::XMFLOAT3(size,0,0), DirectX::XMFLOAT4(1,0,0,1));
        Geometory::AddLine(DirectX::XMFLOAT3(0,0,0), DirectX::XMFLOAT3(0,size,0), DirectX::XMFLOAT4(0,1,0,1));
        Geometory::AddLine(DirectX::XMFLOAT3(0,0,0), DirectX::XMFLOAT3(0,0,size), DirectX::XMFLOAT4(0,0,1,1));
        Geometory::AddLine(DirectX::XMFLOAT3(0,0,0), DirectX::XMFLOAT3(-size,0,0),  DirectX::XMFLOAT4(0,0,0,1));
        Geometory::AddLine(DirectX::XMFLOAT3(0,0,0), DirectX::XMFLOAT3(0,0,-size),  DirectX::XMFLOAT4(0,0,0,1));

        Geometory::DrawLines();

        // カメラ操作
        static bool camAutoSwitch = false;
        static bool camUpDownSwitch = true;
        static float camAutoRotate = 1.0f;
        if (IsKeyTrigger(VK_RETURN)) {
                camAutoSwitch ^= true;
        }
        if (IsKeyTrigger(VK_SPACE)) {
                camUpDownSwitch ^= true;
        }

        DirectX::XMVECTOR camPos;
        if (camAutoSwitch) {
                camAutoRotate += 0.01f;
        }
        camPos = DirectX::XMVectorSet(
                cosf(camAutoRotate) * 5.0f,
                3.5f * (camUpDownSwitch ? 1.0f : -1.0f),
                sinf(camAutoRotate) * 5.0f,
                0.0f);

        // ビュー・プロジェクション設定
        DirectX::XMFLOAT4X4 mat[2];
        DirectX::XMStoreFloat4x4(&mat[0], DirectX::XMMatrixTranspose(
                DirectX::XMMatrixLookAtLH(
                        camPos,
                        DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
                        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
                )));
        DirectX::XMStoreFloat4x4(&mat[1], DirectX::XMMatrixTranspose(
                DirectX::XMMatrixPerspectiveFovLH(
                        DirectX::XMConvertToRadians(60.0f), (float)SCREEN_WIDTH / SCREEN_HEIGHT, 0.1f, 1000.0f)
        ));
        Geometory::SetView(mat[0]);
        Geometory::SetProjection(mat[1]);
#endif

        g_pScene->RootDraw();
        EndDrawDirectX();
}

// EOF
