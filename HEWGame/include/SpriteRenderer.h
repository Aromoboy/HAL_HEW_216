#pragma once


#include "ICamera.h"
#include "TextureDX.h"
#include "Transform.h"
#include <memory>
#include "MeshDX.h"

#include "Texture.h"
#include "Shader.h"

class SpriteRenderer
{
public:
	SpriteRenderer();
	~SpriteRenderer();

	void Draw(Texture2D& texture, const Transform& transform, const CameraSpace::ICamera* camera);
	

private:
	// 板として描画するためのメッシュ、コンストラクタで自作する。
	std::shared_ptr<MeshDX> m_spriteMesh;
	std::shared_ptr<Shader> vs;
	std::shared_ptr<Shader> ps;


	void MeshDraw();



};