#ifndef __SHADER_LIST_H__
#define __SHADER_LIST_H__

#include "Shader.h"
#include "Render/Legacy/Model.h"

#include "MaterialDX.h"


class ShaderList
{
	// `
public:
	enum VSKind
	{
		VS_WORLD, // WVP 設定
		VS_ANIME, // WVP/ボーン設定
		VS_KIND_MAX
	};
	enum PSKind
	{
		PS_LAMBERT, // マテリアル/ライト設定
		PS_SPECULAR, // マテリアル/ライト/カメラ位置設定
		PS_TOON, // マテリアル/ライト設定
		PS_FOG, // マテリアル/ライト/フォグ設定
		PS_KIND_MAX
	};


public:
	ShaderList();
	~ShaderList();

	static void Init();
	static void Uninit();

	// VF[_[ݒ
	static VertexShader* GetVS(VSKind vs);
	static PixelShader* GetPS(PSKind ps);

	// 萔obt@ւ̐ݒ
	static void SetWVP(DirectX::XMFLOAT4X4* wvp);
	static void SetBones(DirectX::XMFLOAT4X4* bones200);
	static void SetMaterial(const Model::Material& material);
	static void SetMaterial(const MaterialDX& material);

	static void SetLight(DirectX::XMFLOAT4 color, DirectX::XMFLOAT3 dir);
	static void SetCameraPos(const DirectX::XMFLOAT3 pos);
	static void SetFog(DirectX::XMFLOAT4 color, float start, float range);
	
private:
	static void MakeWorldVS();
	static void MakeAnimeVS();
	static void MakeLambertPS();
	static void MakeSpecularPS();
	static void MakeToonPS();
	static void MakeFogPS();

private:
	static VertexShader* m_pVS[VS_KIND_MAX];
	static PixelShader* m_pPS[PS_KIND_MAX];
	
};

#endif
