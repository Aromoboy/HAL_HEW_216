// 描画エフェクトの適用や切り替えを行う
// 描画更新やモデル描画から呼ばれる

#include "EffectSystem.h"



EffectSystem::EffectSystem(::EffekseerRendererDX11::RendererRef a_render, ::Effekseer::ManagerRef a_manager, const std::unordered_map<std::string, EffectCreateData>& a_effectPathMap)
	: m_renderer(a_render)
	, m_manager(a_manager)
	, m_time(0.0f)
{
	for (auto it : a_effectPathMap)
	{
		Effekseer::EffectRef effect = Effekseer::Effect::Create(m_manager, it.second.path.c_str(),it.second.magnification);
		if(effect == nullptr)
		{
			continue;
		}

		m_effectCache[it.first] = effect;

	}
}

EffectSystem::~EffectSystem()
{
}

void EffectSystem::SpawnEffect(const EffectCreateRequest& a_request)
{
	std::string nameKey = a_request.effectName;
	auto pathIt = m_effectCache.find(nameKey);
	if (pathIt == m_effectCache.end()) return;

	// Effectを取得
	Effekseer::EffectRef effect = m_effectCache.at(nameKey);
	if (effect == nullptr) return;
	// 座標をEffekseer用に変換
	Effekseer::Vector3D spawnPos(a_request.spawnPosition.x, a_request.spawnPosition.y, a_request.spawnPosition.z);

	// Effectの読み込み
	Effekseer::Handle handle = m_manager->Play(effect, spawnPos, a_request.startFrame);

	// ハンドルと情報をセットで保存
	m_playingEffects.push_back(EffectSystem::PlayingEffect(handle, a_request.moveSpeed));
	
}

void EffectSystem::OnUpdate(const Core::UpdateContext& ctx)
{
	m_time += ctx.dt / 1000;
	m_renderer->SetTime(m_time);
	// 削除予定のエフェクト
	std::vector<int> pendingEffectDelete;

	for (int i = 0; i < m_playingEffects.size(); i++)
	{
		auto& it = m_playingEffects[i];
		if (!m_manager->Exists(it.handle))
		{
			// 削除予定のエフェクトの要素番号を保持
			pendingEffectDelete.push_back(i);
			continue;
		}
		
		
		// EffectのTransform更新
		UpdateEffectTransform(it,ctx.dt);
	}

	// 後ろ側からイテレータを回して削除
	for (std::vector<int>::reverse_iterator it = pendingEffectDelete.rbegin(),  end = pendingEffectDelete.rend(); it != end; it++)
	{
		// イテレータを入れなきゃ動かないので一番前のイテレータを出して加算
		m_playingEffects.erase(m_playingEffects.begin() + *it);
	}

	m_manager->Update();
}

void EffectSystem::Draw(const CameraSpace::ICamera* a_camera)
{
	// Specify a camera matrix
	// カメラ行列を設定
	::Effekseer::Matrix44 cameraMatrix;
	DirectX::XMFLOAT4X4 view = a_camera->GetView();
	DirectX::XMMATRIX restored = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&view));
	std::memcpy(&cameraMatrix, &restored, sizeof(cameraMatrix));

	// Specify a projection matrix
	// 投影行列を設定
	::Effekseer::Matrix44 projectionMatrix;
	//projectionMatrix.PerspectiveFovRH(90.0f / 180.0f * 3.14f, (float)SCREEN_WIDTH /  (float)SCREEN_HEIGHT, 1.0f, 500.0f);
	DirectX::XMFLOAT4X4 proj = a_camera->GetProjection();
	DirectX::XMMATRIX restoredProj = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&proj));
	std::memcpy(&projectionMatrix, &restoredProj, sizeof(projectionMatrix));


	// カメラ行列を設定
	m_renderer->SetCameraMatrix(cameraMatrix);	

	// 投影行列を設定
	m_renderer->SetProjectionMatrix(projectionMatrix);


	m_renderer->BeginRendering();

	Effekseer::Manager::DrawParameter param;
	param.ZNear = 0.0f;
	param.ZFar = 1.0f;
	param.ViewProjectionMatrix = m_renderer->GetCameraProjectionMatrix();
	m_manager->Draw(param);

	m_renderer->EndRendering();
}

void EffectSystem::UpdateEffectTransform(const PlayingEffect& effect, float dt)
{
	m_manager->AddLocation(effect.handle, effect.move * dt);
}
