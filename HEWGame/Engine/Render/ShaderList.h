#ifndef __SHADER_LIST_H__
#define __SHADER_LIST_H__

#include "Shader.h"
#include "Render/Legacy/Model.h"

#include "MaterialDX.h"

struct LightingSettings
{
	struct DirectionalLight
	{
		DirectX::XMFLOAT4 diffuse;
		DirectX::XMFLOAT3 dir;
		float wrap = 0.0f;
	};
	struct HemisphereLight
	{
		DirectX::XMFLOAT4 sky;
		DirectX::XMFLOAT4 ground;
	};
	struct RimLight
	{
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT4 params;
		float enable = 0.0f;
	};

	DirectionalLight key;
	DirectionalLight fill;
	HemisphereLight hemi;
	RimLight rim;

	void NormalizeDirections()
	{
		key.dir = NormalizeDirection(key.dir);
		fill.dir = NormalizeDirection(fill.dir);
	}

	void BuildParams(DirectX::XMFLOAT4(&p)[8]) const
	{
		p[0] = key.diffuse;
		p[1] = DirectX::XMFLOAT4(key.dir.x, key.dir.y, key.dir.z, key.wrap);
		p[2] = fill.diffuse;
		p[3] = DirectX::XMFLOAT4(fill.dir.x, fill.dir.y, fill.dir.z, fill.wrap);
		p[4] = hemi.sky;
		p[5] = hemi.ground;
		p[6] = rim.color;
		p[7] = rim.params;
		p[7].w = rim.enable;
	}

private:
	static DirectX::XMFLOAT3 NormalizeDirection(const DirectX::XMFLOAT3& dir)
	{
		const float lengthSq = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
		if (lengthSq <= 0.0f)
		{
			return dir;
		}
		DirectX::XMFLOAT3 normalized;
		DirectX::XMStoreFloat3(&normalized, DirectX::XMVector3Normalize(
			DirectX::XMVectorSet(dir.x, dir.y, dir.z, 0.0f)));
		return normalized;
	}
};


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
		PS_LAMBERT_LEGACY, // 旧ランバート
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
	static void SetMaterial(const MaterialDX& material, PixelShader* target = nullptr);

	static void SetLight(DirectX::XMFLOAT4 color, DirectX::XMFLOAT3 dir);
	static void SetLighting(const LightingSettings& settings);
	static void SetLightingMode(int mode);
	static void SetCameraPos(const DirectX::XMFLOAT3 pos);
	static void SetFog(DirectX::XMFLOAT4 color, float start, float range);
	
private:
	static void MakeWorldVS();
	static void MakeAnimeVS();
	static void MakeLambertPS();
	static void MakeLambertLegacyPS();
	static void MakeSpecularPS();
	static void MakeToonPS();
	static void MakeFogPS();

private:
	static VertexShader* m_pVS[VS_KIND_MAX];
	static PixelShader* m_pPS[PS_KIND_MAX];
	static int m_lightingMode;
	
};

#endif
