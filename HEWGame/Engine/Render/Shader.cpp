#include "Shader.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "d3dcompiler.lib")

//----------
// 基本クラス
Shader::Shader(Kind kind)
	: m_kind(kind)
{
}
Shader::~Shader()
{
	std::vector<ID3D11Buffer*>::iterator it = m_pBuffers.begin();
	while (it != m_pBuffers.end())
	{
		SAFE_RELEASE((*it));
		++it;
	}
}
HRESULT Shader::Load(const char* pFileName)
{
	HRESULT hr = E_FAIL;

	// ファイルを読み込む
	FILE* fp;
	fopen_s(&fp, pFileName, "rb");
	if (!fp) { return hr; }

	// ファイルのサイズを調べる
	int fileSize = 0;
	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);

	// メモリに読み込み
	fseek(fp, 0, SEEK_SET);
	char* pData = new char[fileSize];
	fread(pData, fileSize, 1, fp);
	fclose(fp);

	// シェーダー作成
	hr = Make(pData, fileSize);
	
	// 終了処理
	if (pData) { delete[] pData; }
	return hr;
}
HRESULT Shader::Compile(const char *pCode)
{
        static const char *pTargetList[] =
        {
                "vs_5_0",
                "ps_5_0"
        };

        HRESULT hr = E_FAIL;
        ID3DBlob *pBlob = nullptr;
        ID3DBlob *error = nullptr;
        // ミカ：配布用シェーダーは最適化し、デバッグ情報はDebug構成だけに付ける
#if defined(_DEBUG)
        UINT compileFlag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlag = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
        hr = D3DCompile(pCode, strlen(pCode), nullptr, nullptr, nullptr,
                "main", pTargetList[m_kind], compileFlag, 0, &pBlob, &error);
        SAFE_RELEASE(error);
        if (FAILED(hr))
        {
                SAFE_RELEASE(pBlob);
                return hr;
        }

        hr = Make(pBlob->GetBufferPointer(), (UINT)pBlob->GetBufferSize());
        SAFE_RELEASE(pBlob);
        return hr;
}

void Shader::WriteBuffer(UINT slot, void* pData)
{
	// ミカ：slotは反射の列挙順ではなくHLSLのbレジスター番号として扱う
	if(slot < m_pBuffers.size() && m_pBuffers[slot])
		GetContext()->UpdateSubresource(m_pBuffers[slot], 0, nullptr, pData, 0, 0);
}
void Shader::SetTexture(UINT slot, Texture* tex)
{
	if (slot >= m_pTextures.size()) { return; }
	ID3D11ShaderResourceView* pTex = tex ? tex->GetResource() : nullptr;
	m_pTextures[slot] = pTex;
	switch (m_kind)
	{
	case Vertex:	GetContext()->VSSetShaderResources(slot, 1, &pTex); break;
	case Pixel:		GetContext()->PSSetShaderResources(slot, 1, &pTex); break;
	}
}

void Shader::SetTexture(UINT slot, ID3D11ShaderResourceView* tex)
{
	if (slot >= m_pTextures.size()) { return; }
	ID3D11ShaderResourceView* pTex = tex;
	m_pTextures[slot] = pTex;
	switch (m_kind)
	{
	case Vertex:	GetContext()->VSSetShaderResources(slot, 1, &pTex); break;
	case Pixel:		GetContext()->PSSetShaderResources(slot, 1, &pTex); break;
	}
}

HRESULT Shader::Make(void* pData, UINT size)
{
	HRESULT hr;
	ID3D11Device* pDevice = GetDevice();

	// 解析用のリフレクション作成
	Microsoft::WRL::ComPtr<ID3D11ShaderReflection> pReflection;
	hr = D3DReflect(pData, size, IID_PPV_ARGS(pReflection.GetAddressOf()));
	if (FAILED(hr)) { return hr; }

	D3D11_SHADER_DESC shaderDesc{};
	hr = pReflection->GetDesc(&shaderDesc);
	if (FAILED(hr)) { return hr; }
	std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> buffers;
	std::vector<ID3D11ShaderResourceView*> textures;
	// ミカ：未使用レジスターの穴と配列を保ち、実際のバインド先から領域を作る
	for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
	{
		D3D11_SHADER_INPUT_BIND_DESC binding{};
		hr = pReflection->GetResourceBindingDesc(i, &binding);
		if (FAILED(hr)) { return hr; }
		if (binding.Type == D3D_SIT_CBUFFER)
		{
			if (binding.BindCount != 1 || binding.BindPoint >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
				return E_INVALIDARG;
			D3D11_SHADER_BUFFER_DESC shaderBufDesc{};
			hr = pReflection->GetConstantBufferByName(binding.Name)->GetDesc(&shaderBufDesc);
			if (FAILED(hr)) { return hr; }
			if (buffers.size() <= binding.BindPoint) buffers.resize(binding.BindPoint + 1);
			D3D11_BUFFER_DESC bufDesc{};
			bufDesc.ByteWidth = shaderBufDesc.Size;
			bufDesc.Usage = D3D11_USAGE_DEFAULT;
			bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			hr = pDevice->CreateBuffer(&bufDesc, nullptr, buffers[binding.BindPoint].GetAddressOf());
			if (FAILED(hr)) { return hr; }
		}
		else if (binding.Type == D3D_SIT_TEXTURE || binding.Type == D3D_SIT_TBUFFER
			|| binding.Type == D3D_SIT_STRUCTURED || binding.Type == D3D_SIT_BYTEADDRESS)
		{
			if (binding.BindPoint >= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT || binding.BindCount == 0
				|| binding.BindCount > D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - binding.BindPoint)
				return E_INVALIDARG;
			const UINT endSlot = binding.BindPoint + binding.BindCount;
			if (textures.size() < endSlot) textures.resize(endSlot, nullptr);
		}
	}

	hr = MakeShader(pData, size);
	if (FAILED(hr)) { return hr; }
	// ミカ：作成完了後に置き換え、途中失敗では既存の描画資源を維持する
	for (auto*& buffer : m_pBuffers) SAFE_RELEASE(buffer);
	m_pBuffers.assign(buffers.size(), nullptr);
	for (size_t slot = 0; slot < buffers.size(); ++slot) m_pBuffers[slot] = buffers[slot].Detach();
	m_pTextures = std::move(textures);
	return S_OK;
}

//----------
// 頂点シェーダ
VertexShader::VertexShader()
	: Shader(Shader::Vertex)
	, m_pVS(nullptr)
	, m_pInputLayout(nullptr)
{
}

VertexShader::~VertexShader()
{
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pVS);
}

void VertexShader::Bind(void)
{
	ID3D11DeviceContext* pContext =	GetContext();
	pContext->VSSetShader(m_pVS, NULL, 0);
	pContext->IASetInputLayout(m_pInputLayout);
	for (int i = 0; i < m_pBuffers.size(); ++i)
		pContext->VSSetConstantBuffers(i, 1, &m_pBuffers[i]);
	for (int i = 0; i < m_pTextures.size(); ++i)
		pContext->VSSetShaderResources(i, 1, &m_pTextures[i]);
}

HRESULT VertexShader::MakeShader(void* pData, UINT size)
{
	HRESULT hr;
	ID3D11Device* pDevice = GetDevice();
	
	// シェーダー作成
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
	hr = pDevice->CreateVertexShader(pData, size, NULL, vertexShader.GetAddressOf());
	if(FAILED(hr)) { return hr; }

	/*
	シェーダ作成時にシェーダリフレクションを通してインプットレイアウトを取得
	セマンティクスの配置などから識別子を作成
	識別子が登録済→再利用、なければ新規作成
	*/

	Microsoft::WRL::ComPtr<ID3D11ShaderReflection> pReflection;
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

	hr = D3DReflect(pData, size, IID_PPV_ARGS(pReflection.GetAddressOf()));
	if (FAILED(hr)) { return hr; }

	pReflection->GetDesc(&shaderDesc);
	pInputDesc = new D3D11_INPUT_ELEMENT_DESC[shaderDesc.InputParameters];
	for(UINT i = 0; i < shaderDesc.InputParameters; ++ i)
	{
		pReflection->GetInputParameterDesc(i, &sigDesc);
		pInputDesc[i].SemanticName = sigDesc.SemanticName;
		pInputDesc[i].SemanticIndex = sigDesc.SemanticIndex;

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
		pData, size, inputLayout.GetAddressOf()
	);
	
	delete[] pInputDesc;
	if (SUCCEEDED(hr))
	{
		SAFE_RELEASE(m_pVS);
		SAFE_RELEASE(m_pInputLayout);
		m_pVS = vertexShader.Detach();
		m_pInputLayout = inputLayout.Detach();
	}
	return hr;
}

//----------
// ピクセルシェーダ
PixelShader::PixelShader()
	: Shader(Shader::Pixel)
	, m_pPS(nullptr)
{
}
PixelShader::~PixelShader()
{
	SAFE_RELEASE(m_pPS);
}
void PixelShader::Bind(void)
{
	ID3D11DeviceContext* pContext = GetContext();
	pContext->PSSetShader(m_pPS, nullptr, 0);
	for (int i = 0; i < m_pBuffers.size(); ++i)
		pContext->PSSetConstantBuffers(i, 1, &m_pBuffers[i]);
	for (int i = 0; i < m_pTextures.size(); ++i)
		pContext->PSSetShaderResources(i, 1, &m_pTextures[i]);
}
HRESULT PixelShader::MakeShader(void* pData, UINT size)
{
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
	const HRESULT hr = GetDevice()->CreatePixelShader(pData, size, NULL, pixelShader.GetAddressOf());
	if (SUCCEEDED(hr))
	{
		SAFE_RELEASE(m_pPS);
		m_pPS = pixelShader.Detach();
	}
	return hr;
}
