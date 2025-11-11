#include "SceneGame.h"
#include "Geometory.h"
#include "DrawMatrix.h"
#include "Vector.h"
#include "ShaderList.h"
#include "Defines.h"
#include "CameraDebug.h"
#include "DebugConsole.h"
#include "Input.h"
#include "AssimpModelLorder.h"

Model::AnimeNo no;
float3 fRads = { 0.0f, 0.0f, 0.0f };

SceneGame::SceneGame()
{
        RenderTarget* pRTV = GetDefaultRTV();
        DepthStencil* pDSV = GetDefaultDSV();
        SetRenderTargets(1, &pRTV, pDSV);
        SetDepthTest(true);
        m_pModel = std::make_unique<Model>();
        if (!m_pModel->Load("Assets/Model/LowPolyNature/Models/Flowers_01.fbx", 1.0f, Model::Flip::None))
        {
                no = m_pModel->AddAnimation("Assets/Model/LowPolyNature/Models/BoxsLam.fbx");
        }

        no = m_pModel->AddAnimation("Assets/Model/LowPolyNature/BoxsLam.fbx");

        m_pModel->Play(no, 1, 1.0f);

        m_pCamera = std::make_unique<CameraDebug>(float3{ 20.0f, 0.0f, 0.0f }, 60.0f);

        for (int i = 0; i < 9; i++)
        {
                m_pObjects.push_back(std::make_shared<TestObject>("OiiaioooooiaiFin", float3{ -2.0f ,0.0f ,-5.0f }, float3{ 1.0f,1.0f,1.0f }, float3{ 0.02f * i, 0.0f, 0.0f }));
        }

        m_pManager = std::make_unique<SoundManagerEX>();

        m_loader = std::make_unique<AssimpModelLorder>();

        // 描画テスト用
        m_model = m_loader->Load("Assets/Model/LowPolyNature/Models/OiiaioooooiaiFin.fbx");

        m_render = std::make_unique<ModelRender>();
}

SceneGame::~SceneGame()
{
        if (m_pCamera)
        {
                GetUpdateScheduler().Unregister(*m_pCamera);
        }
}

void SceneGame::ConfigureUpdates()
{
        if (m_pCamera)
        {
                GetUpdateScheduler().Register(*m_pCamera);
        }
}

void SceneGame::Update(const Core::UpdateContext& ctx)
{
        (void)ctx;
        fRads.z += 0.001f * RAD;
}

void SceneGame::Draw()
{
        // 描画確認用の数値

        // カメラの設定
        Geometory::SetView(m_pCamera->GetViewMatrix());
        Geometory::SetProjection(m_pCamera->GetProjectionMatrix());

        m_render->Draw(*m_model, *m_pCamera);
}
