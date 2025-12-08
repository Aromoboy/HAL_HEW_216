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
	// 板として描画するためのメッシュ、コンストラクタで自作する。
	std::shared_ptr<MeshDX> m_spriteMesh;
	std::shared_ptr<Shader> vs;
	std::shared_ptr<Shader> ps;

	DirectX::XMFLOAT2 m_uvPos;
	DirectX::XMFLOAT2 m_uvScale;

	void MeshDraw();



};