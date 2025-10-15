#include "SpriteDrawer.h"

#include <d3dcompiler.h>


#pragma comment(lib, "d3dcompiler.lib")

#define MAX_VERTEX (4)

HRESULT ShaderCompile(ID3D11Device* pDevice, const char* vs, const char* ps);
HRESULT CreateVertexShader(ID3D11Device* pDevice, void* data, UINT size);
HRESULT CreatePixelShader(ID3D11Device* pDevice, void* data, UINT size);



SpriteParameter g_SpriteParam;
ID3D11DeviceContext* g_pSpriteContext;
ID3D11ShaderResourceView* g_pSpriteTexture;
ID3D11ShaderResourceView* g_pSpriteWhiteTex;
ID3D11InputLayout* g_pSpriteIL;
ID3D11VertexShader* g_pSpriteVS;
ID3D11PixelShader* g_pSpritePS;
ID3D11Buffer* g_pSpriteBuffer;
ID3D11RasterizerState* g_pSpriteRasterizer;




//void DrawSpriteParameter(ID3D11Buffer* pSprite, SpriteParameter parm, ID3D11ShaderResourceView* pTex)
//{
//	SetSpriteTexture(pTex);
//
//	D3D11_BUFFER_DESC desc;
//	pSprite->GetDesc(&desc);
//	UINT vtxSize = sizeof(Vertex);
//	UINT num = desc.ByteWidth / vtxSize;
//	UINT offset = 0;
//
//	g_SpriteParam = parm;
//	
//	
//	g_pSpriteContext->IASetInputLayout(g_pSpriteIL);
//	g_pSpriteContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
//	g_pSpriteContext->IASetVertexBuffers(0, 1, &pSprite, &vtxSize, &offset);
//	g_pSpriteContext->VSSetShader(g_pSpriteVS, NULL, 0);
//	g_pSpriteContext->PSSetShader(g_pSpritePS, NULL, 0);
//	g_pSpriteContext->UpdateSubresource(g_pSpriteBuffer, 0, nullptr, &g_SpriteParam, 0, 0);
//	g_pSpriteContext->VSSetConstantBuffers(0, 1, &g_pSpriteBuffer);
//	g_pSpriteContext->PSSetShaderResources(0, 1, &g_pSpriteTexture);
//
//	g_pSpriteContext->Draw(num, 0);
//}

void DrawSprite(ID3D11Buffer* pSprite)
{
	D3D11_BUFFER_DESC desc;
	pSprite->GetDesc(&desc);
	UINT vtxSize = sizeof(Vertex);
	UINT num = desc.ByteWidth / vtxSize;
	UINT offset = 0;



	g_pSpriteContext->IASetInputLayout(g_pSpriteIL);
	g_pSpriteContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	g_pSpriteContext->IASetVertexBuffers(0, 1, &pSprite, &vtxSize, &offset);
	g_pSpriteContext->VSSetShader(g_pSpriteVS, NULL, 0);
	g_pSpriteContext->PSSetShader(g_pSpritePS, NULL, 0);
	g_pSpriteContext->UpdateSubresource(g_pSpriteBuffer, 0, nullptr, &g_SpriteParam, 0, 0);
	g_pSpriteContext->VSSetConstantBuffers(0, 1, &g_pSpriteBuffer);
	g_pSpriteContext->PSSetShaderResources(0, 1, &g_pSpriteTexture);

	g_pSpriteContext->Draw(num, 0);
}


void SetSpritePos(float x, float y)
{
	g_SpriteParam.pos.x = x;
	g_SpriteParam.pos.y = y;
}
void SetSpriteAngle(float rad)
{
	g_SpriteParam.rad = rad;
}
void SetSpriteScale(float x, float y)
{
	g_SpriteParam.scale.x = x;
	g_SpriteParam.scale.y = y;
}
void SetSpriteUVPos(float u, float v)
{
	g_SpriteParam.uvPos.x = u;
	g_SpriteParam.uvPos.y = v;
}
void SetSpriteUVScale(float u, float v)
{
	g_SpriteParam.uvScale.x = u;
	g_SpriteParam.uvScale.y = v;
}
void SetSpriteColor(float r, float g, float b, float a)
{
	g_SpriteParam.color.x = r;
	g_SpriteParam.color.y = g;
	g_SpriteParam.color.z = b;
	g_SpriteParam.color.w = a;
}
void SetSpriteTexture(ID3D11ShaderResourceView* pTexture)
{
	g_pSpriteTexture = pTexture;
}


void InitSpriteDrawer(
	ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
	UINT width, UINT height, bool isYup)
{
	const char* VS = R"EOT(
struct VS_IN {
	float3 pos : POSITION0;
	float2 uv : TEXCOORD0;
};
struct VS_OUT {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
};
cbuffer Param : register(b0) {
	float2 pos;
	float2 scale;
	float2 uvPos;
	float2 uvScale;
	float4 color;
	float2 screen;
	float rad;
	float dummy;
};
VS_OUT main(VS_IN vin) {
	VS_OUT vout;
	vout.pos = float4(vin.pos, 1.0f);
	vout.pos.xy *= scale;

	float x = vout.pos.x * cos(rad) - vout.pos.y * sin(rad);
	float y = vout.pos.x * sin(rad) + vout.pos.y * cos(rad);
	vout.pos.x = x;
	vout.pos.y = y;
	vout.pos.xy += pos;
	vout.pos.xy /= screen;
	vout.uv = vin.uv;
	vout.uv *= uvScale;
	vout.uv += uvPos;
	vout.color = color;
	return vout;
})EOT";
	const char* PS = R"EOT(
struct PS_IN {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
};
Texture2D tex : register(t0);
SamplerState samp : register(s0);
float4 main(PS_IN pin) : SV_TARGET {
	return tex.Sample(samp, pin.uv) * pin.color;
})EOT";

	g_SpriteParam.pos = { 0.0f, 0.0f };
	g_SpriteParam.scale = { 1.0f, 1.0f };
	g_SpriteParam.uvPos = { 0.0f, 0.0f };
	g_SpriteParam.uvScale = { 1.0f, 1.0f };
	g_SpriteParam.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	g_SpriteParam.screen = { (float)width, height * (isYup ? 1.0f : -1.0f) };
	g_SpriteParam.rad = 0.0f;

	g_pSpriteContext = pContext;

	ShaderCompile(pDevice, VS, PS);


	HRESULT hr = E_FAIL;
	// カリング設定
	D3D11_RASTERIZER_DESC rasterizer = {};//負荷が大きかったときはここも見る
	rasterizer.CullMode = D3D11_CULL_NONE;
	rasterizer.FillMode = D3D11_FILL_SOLID;
	rasterizer.FrontCounterClockwise = true;
	hr = pDevice->CreateRasterizerState(&rasterizer, &g_pSpriteRasterizer);
	if (FAILED(hr)) { return; }
	g_pSpriteContext->RSSetState(g_pSpriteRasterizer);


}

void UninitSpriteDrawer()
{
	if(g_pSpriteWhiteTex) g_pSpriteWhiteTex->Release();
	if(g_pSpriteVS) g_pSpriteVS->Release();
	if(g_pSpritePS) g_pSpritePS->Release();
	if (g_pSpriteIL) g_pSpriteIL->Release();
	if (g_pSpriteBuffer) g_pSpriteBuffer->Release();
	if(g_pSpriteRasterizer) g_pSpriteRasterizer->Release();
}


HRESULT ShaderCompile(ID3D11Device* pDevice, const char* vs, const char* ps)
{
	const char* ppCode[] = { vs, ps };
	const char* pTargetList[] = { "vs_5_0", "ps_5_0" };
	HRESULT hr;
	ID3DBlob *pBlob;
	ID3DBlob *error;

	for (int i = 0; i < 2; ++i)
	{
		// シェーダー解析 = 最適化的な？
		UINT compileFlag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
		hr = D3DCompile(ppCode[i], strlen(ppCode[i]), nullptr, nullptr, nullptr,//GPUで使われる言語をコンパイルしてる？
			"main", pTargetList[i], compileFlag, 0, &pBlob, &error);
		if (FAILED(hr)) { return hr; }

		if (i == 0)
			hr = CreateVertexShader(pDevice, pBlob->GetBufferPointer(), (UINT)pBlob->GetBufferSize());
		else
			hr = CreatePixelShader(pDevice, pBlob->GetBufferPointer(), (UINT)pBlob->GetBufferSize());
		if (FAILED(hr)) { return hr; }

		if(pBlob) pBlob->Release();
		if(error) error->Release();
	}

	// 定数バッファ作成
	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.ByteWidth = sizeof(SpriteParameter);
	bufDesc.Usage = D3D11_USAGE_DEFAULT;
	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	// バッファの作成
	hr = pDevice->CreateBuffer(&bufDesc, nullptr, &g_pSpriteBuffer);
	if (FAILED(hr)) { return hr; }

	return hr;
}

HRESULT CreateVertexShader(ID3D11Device* pDevice, void* pData, UINT size)
{
	// シェーダー作成
	HRESULT hr;
	hr = pDevice->CreateVertexShader(pData, size, NULL, &g_pSpriteVS);
	if (FAILED(hr)) { return hr; }

	ID3D11ShaderReflection *pReflection; // 他のインターフェースのポインタを取得し存在を管理する
	D3D11_SHADER_DESC shaderDesc;
	D3D11_INPUT_ELEMENT_DESC* pInputDesc;
	D3D11_SIGNATURE_PARAMETER_DESC sigDesc;

	DXGI_FORMAT formats[][4] =
	{
		{
			DXGI_FORMAT_R32_UINT,
			DXGI_FORMAT_R32G32_UINT,
			DXGI_FORMAT_R32G32B32_UINT,
			DXGI_FORMAT_R32G32B32A32_UINT,
		}, {
			DXGI_FORMAT_R32_SINT,
			DXGI_FORMAT_R32G32_SINT,
			DXGI_FORMAT_R32G32B32_SINT,
			DXGI_FORMAT_R32G32B32A32_SINT,
		}, {
			DXGI_FORMAT_R32_FLOAT,
			DXGI_FORMAT_R32G32_FLOAT,
			DXGI_FORMAT_R32G32B32_FLOAT,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
		},
	};

	hr = D3DReflect(pData, size, IID_PPV_ARGS(&pReflection));
	if (FAILED(hr)) { return hr; }

	pReflection->GetDesc(&shaderDesc);
	pInputDesc = new D3D11_INPUT_ELEMENT_DESC[shaderDesc.InputParameters];
	for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
	{
		pReflection->GetInputParameterDesc(i, &sigDesc);
		pInputDesc[i].SemanticName = sigDesc.SemanticName;
		pInputDesc[i].SemanticIndex = sigDesc.SemanticIndex;

		// http://marupeke296.com/TIPS_No17_Bit.html
		BYTE elementCount = sigDesc.Mask;
		elementCount = (elementCount & 0x05) + ((elementCount >> 1) & 0x05);
		elementCount = (elementCount & 0x03) + ((elementCount >> 2) & 0x03);

		switch (sigDesc.ComponentType)
		{
		case D3D_REGISTER_COMPONENT_UINT32:
			pInputDesc[i].Format = formats[0][elementCount - 1];
			break;
		case D3D_REGISTER_COMPONENT_SINT32:
			pInputDesc[i].Format = formats[1][elementCount - 1];
			break;
		case D3D_REGISTER_COMPONENT_FLOAT32:
			pInputDesc[i].Format = formats[2][elementCount - 1];
			break;
		}
		pInputDesc[i].InputSlot = 0;
		pInputDesc[i].AlignedByteOffset = i == 0 ? 0 : D3D11_APPEND_ALIGNED_ELEMENT;
		pInputDesc[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		pInputDesc[i].InstanceDataStepRate = 0;
	}

	hr = pDevice->CreateInputLayout(
		pInputDesc, shaderDesc.InputParameters,
		pData, size, &g_pSpriteIL
	);

	delete[] pInputDesc;
	return hr;
}
HRESULT CreatePixelShader(ID3D11Device* pDevice, void* data, UINT size)
{
	return pDevice->CreatePixelShader(data, size, NULL, &g_pSpritePS);
}