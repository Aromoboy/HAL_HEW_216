// スプライトを描く処理をまとめている
// 画面表示や演出の描画から呼ばれる

#pragma once


#include "ICamera.h"
#include "TextureDX.h"
#include "Transform.h"
#include <memory>
#include "MeshDX.h"

#include "Texture.h"
#include "Shader.h"
#include <DirectXMath.h>

class SpriteRenderer
{
public:
	SpriteRenderer();
	~SpriteRenderer();

	void Draw(Texture2D& texture, const Transform& transform, const CameraSpace::ICamera* camera);
	
	void SetUVPos(DirectX::XMFLOAT2 pos);
	void SetUVScale(DirectX::XMFLOAT2 scale);

private:
	// Ƃĕ`悷邽߂̃bVARXgN^Ŏ삷 
	std::shared_ptr<MeshDX> m_spriteMesh;
	std::shared_ptr<Shader> vs;
	std::shared_ptr<Shader> ps;

	DirectX::XMFLOAT2 m_uvPos;
	DirectX::XMFLOAT2 m_uvScale;

	void MeshDraw();



};