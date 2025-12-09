#ifndef __SHADER_LIST_H__
#define __SHADER_LIST_H__

#include "Shader.h"
#include "Model.h"

#include "MaterialDX.h"


class ShaderList
{
public:
        enum VSKind
        {
                VS_WORLD, // SetWVP
                VS_KIND_MAX
        };
        enum PSKind
        {
                PS_LAMBERT, //SetMaterial, SetLight
                PS_KIND_MAX
        };


public:
	ShaderList();
	~ShaderList();

        static void Init();
        static void Uninit();

        struct WVPMatrix
        {
                DirectX::XMFLOAT4X4 world;
                DirectX::XMFLOAT4X4 view;
                DirectX::XMFLOAT4X4 projection;
        };

        // シェーダー設定
        static VertexShader* GetVS(VSKind vs);
        static PixelShader* GetPS(PSKind ps);

        // 定数バッファへの設定
        static void SetWVP(const WVPMatrix& wvp);
        static void SetMaterial(const Model::Material& material);
        static void SetMaterial(const MaterialDX& material);

        static void SetLight(DirectX::XMFLOAT4 color, DirectX::XMFLOAT3 dir);

private:
        static void MakeWorldVS();
        static void MakeLambertPS();

private:
	static VertexShader* m_pVS[VS_KIND_MAX];
	static PixelShader* m_pPS[PS_KIND_MAX];
	
};

#endif // __SHADER_LIST_H__