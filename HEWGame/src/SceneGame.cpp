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

SceneGame::~SceneGame() {}

void SceneGame::Update(const Core::UpdateContext& context)
{
	fRads.z += context.deltaSeconds * RAD;

	m_pCamera->Update();
}

void SceneGame::Draw()
{
	// 描画確認用の数値

	// カメラの設定
	Geometory::SetView(m_pCamera->GetViewMatrix());
	Geometory::SetProjection(m_pCamera->GetProjectionMatrix());
	
	m_render->Draw(*m_model, *m_pCamera);
	
	// アニメーションの更新
	m_pModel->Step(context.deltaSeconds);

	// 変換行列の転置
	DrawMatrix::CreateWorldMatrix(wvp[0], float3{ 0.0f, 0.0f, 0.0f }, float3{ 1.0f, 1.0f, 1.0f }, float3{ 0.0f, 170.0f * RAD, 0.0f }, m_pModel->GetBone(1));
	
	
	// シェーダーの登録
	ShaderList::SetWVP(wvp);
	m_pModel->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
	m_pModel->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));


	DebugConsole::SetDrawPos(1, 5);
	std::cout << "メッシュ数" << m_pModel->GetMeshNum();

	DebugConsole::SetDrawPos(1, 6);
	std::cout << "マテリアル数" << m_pModel->GetMaterialNum();

	// 描画処理
	for (int i = 0; i < m_pModel->GetMeshNum(); i++)
	{
		
		Model::Mesh mesh = *m_pModel->GetMesh(i);
		
		if (i < m_pModel->GetMaterialNum())
		{
			Model::Material material = *m_pModel->GetMaterial(i);

			ShaderList::SetMaterial(material);
		}


		if (m_pModel) m_pModel->Draw();
	}


	for (auto it : m_pObjects)
	{
		DrawMatrix::CreateWorldMatrix(wvp[0], it->GetPosition(), it->GetScale(), it->GetfRads());

		// シェーダーの登録
		ShaderList::SetWVP(wvp);

		Model &model = it->GetModel();

		model.SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
		model.SetPixelShader(ShaderList::GetPS(ShaderList::PS_FOG));


		// 描画処理
		for (int i = 0; i < model.GetMeshNum(); i++)
		{

			Model::Mesh mesh = *model.GetMesh(i);
			Model::Material material = *model.GetMaterial(i);

			ShaderList::SetMaterial(material);

			//model.Draw();
		}
	}


	

	
	
 }