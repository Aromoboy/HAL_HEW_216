#include "PowerUpRenderer.h"
#include "DirectX.h"
#include "DirectXMath.h"
#include "ShaderList.h"
#include "DrawMatrix.h"
#include "Sprite.h"


#include <sstream>
#include <iomanip>
#include <array>
#include <algorithm>

// 各オフセット
constexpr float2 kIconPositionOffset = { -0.18f, -0.008f };
constexpr float2 kFramePositionOffset = { -0.18f, -0.008f };
constexpr float2 kBackFramePositionOffset = { 0.00f,  0.000f };
constexpr float2 kCostIconPositionOffset = { 0.10f,  -0.050f };
constexpr float2 kNamePositionOffset = { 0.31f,  0.05f };
constexpr float2 kInfomationPositionOffset = { 0.170f,  0.000f };
constexpr float2 kLevelPositionOffset = { 0.50f,  0.045f };
constexpr float2 kCostPositionOffset = { 0.50f,  -0.060f }; 
constexpr float2 kCostNumberPositionOffset = { 0.68f,  0.290f };
constexpr float kIconScaleOffset = 0.5f;
constexpr float kFrameScaleOffset = 0.5f;
constexpr float kBackFrameScaleOffset = 1.8f;
constexpr float kCostIconScaleOffset = 0.3f;
constexpr float kNameScaleOffset = 0.08f;
constexpr float kInfomationScaleOffset = 0.05f;
constexpr float kLevelScaleOffset = 0.06f;
constexpr float kCostScaleOffset = 0.05f;
constexpr float kCostNumberScaleOffset = 0.08f;

constexpr float kNodeNameCharCount = 5.0f * 3.0f;
constexpr float kInfoCharCount = 15.0f * 3.0f;

constexpr float kCostNumberInterval = 0.06f;
// 背景のY移動に対してXの移動する割合
constexpr float kUVMoveRatioX = 2.0f / 2.0f;
// テクスチャ名
constexpr const char* kNumberTextureName = "Number";
constexpr const char* kCostTextureName = "Cost";
constexpr const char* kFrameTextureName = "Frame";
constexpr const char* kBackFrameTexturenName = "BackFrame";
constexpr const char* kBackgroundTextureName = "PowerUpBG";


constexpr float2 kHudCostIconPositionOffset = { 0.40f,  0.30f };
constexpr float kHudCostIconScaleOffset = 0.25f;



PowerUpRenderer::PowerUpRenderer(TextureCache& a_cache)
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
cbuffer Matrix : register(b0) {
	float4x4 world;
	float4x4 view;
	float4x4 proj;
};
cbuffer Param : register(b1) {
	float2 offset;
	float2 size;
	float2 uvPos;
	float2 uvScale;
	float4 color;
};
VS_OUT main(VS_IN vin) {
	VS_OUT vout;
	vout.pos = float4(vin.pos, 1.0f);
	vout.pos.xy *= size;
	vout.pos.xy += offset;
	vout.pos = mul(vout.pos, world);
	vout.pos = mul(vout.pos, view);
	vout.pos = mul(vout.pos, proj);
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


	struct Vertex
	{
		float pos[3];
		float uv[2];
	} vtx[] = {
		{{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}},
		{{ 0.5f, 0.5f, 0.0f}, {1.0f, 0.0f}},
		{{-0.5f,-0.5f, 0.0f}, {0.0f, 1.0f}},
		{{ 0.5f,-0.5f, 0.0f}, {1.0f, 1.0f}},
	};

	MeshDX::Description desc{};
	desc.pVtx = vtx;
	desc.vtxSize = sizeof(Vertex);
	desc.vtxCount = 4;
	desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

	m_spriteMesh = std::make_shared<MeshDX>(desc, -1);

	vs = std::make_shared<VertexShader>();
	vs->Compile(VS);

	ps = std::make_shared <PixelShader>();
	ps->Compile(PS);

	m_number = a_cache.Get(kNumberTextureName);
	m_cost = a_cache.Get(kCostTextureName);
	m_iconFrame = a_cache.Get(kFrameTextureName);
	m_backFrame = a_cache.Get(kBackFrameTexturenName);
	m_background = a_cache.Get(kBackgroundTextureName);
}

PowerUpRenderer::~PowerUpRenderer()
{
}

bool PowerUpRenderer::Initialize(const PowerUpHudLayoutConfig& config)
{
	m_config = config;
	m_texture = std::make_shared<PowerUpStringTexture>();
	HRESULT hr = m_texture->Create(DXGI_FORMAT_R8G8B8A8_UNORM, m_config.textureWidth, m_config.textureHeight, nullptr, true);
	if (FAILED(hr))
	{
		return false;
	}

	const size_t pixelCount = static_cast<size_t>(m_config.textureWidth) * static_cast<size_t>(m_config.textureHeight);
	m_rgbaBuffer.resize(pixelCount * 4);

	if (!CreateTextResources())
	{
		Shutdown();
		return false;
	}

	m_ready = true;
	m_textureDirty = false;
	return true;

}

void PowerUpRenderer::Shutdown()
{
	DestroyTextResources();
	m_texture.reset();
	m_rgbaBuffer.clear();
	m_ready = false;
	m_textureDirty = false;
}


void PowerUpRenderer::DrawCost(int cost, const CameraSpace::ICamera& camera)
{
	const DirectX::XMFLOAT4X4& view = camera.GetView();
	const DirectX::XMFLOAT4X4& proj = camera.GetProjection();

	int count  = 0;
	while (cost)
	{
		float3 pos(kCostNumberPositionOffset.x - kCostNumberInterval * count, kCostNumberPositionOffset.y, 1.0f);
		float3 scale(kCostNumberScaleOffset, kCostNumberScaleOffset, 1.0f);
		DirectX::XMFLOAT4X4 world;
		DrawMatrix::CreateWorldMatrix(world, pos, scale, float3());

		int drawNum = cost % 10;
		DrawSprite(m_number, world, view, proj, DrawSpriteData(float2(0.1f * drawNum, 0.0f), float2(0.1f, 1.0f)));
		cost /= 10;
		count++;
	}
}

void PowerUpRenderer::DrawHud(const CameraSpace::ICamera& camera)
{
	const DirectX::XMFLOAT4X4& view = camera.GetView();
	const DirectX::XMFLOAT4X4& proj = camera.GetProjection();

	DirectX::XMFLOAT4X4 world;
	float3 position;
	float3 scale;
	float2 offset;


	// バックフレーム描画
	offset.x = kHudCostIconPositionOffset.x;
	offset.y = kHudCostIconPositionOffset.y;
	position.x = offset.x;
	position.y = offset.y;
	position.z = 1.0f;
	scale.x = kHudCostIconScaleOffset;
	scale.y = kHudCostIconScaleOffset;
	scale.z = 1.0f;

	DrawMatrix::CreateWorldMatrix(world, position, scale, float3{ 0.0f,0.0f,0.0f });

	DrawSprite(m_cost, world, view, proj);
}

void PowerUpRenderer::DrawCursor(std::shared_ptr<Texture2D> a_texture, float2 a_position, float2 a_scale, const CameraSpace::ICamera& camera, float a_alpha)
{
	const DirectX::XMFLOAT4X4& view = camera.GetView();
	const DirectX::XMFLOAT4X4& proj = camera.GetProjection();

	DirectX::XMFLOAT4X4 world;
	float3 position;
	float3 scale;
	float2 offset;

	offset.x = kBackFramePositionOffset.x * a_scale.x;
	offset.y = kBackFramePositionOffset.y * a_scale.y;
	position.x = a_position.x + offset.x;
	position.y = a_position.y + offset.y;
	position.z = 1.0f;
	scale.x = a_scale.x * kBackFrameScaleOffset;
	scale.y = a_scale.y * kBackFrameScaleOffset;
	scale.z = 1.0f;

	DrawMatrix::CreateWorldMatrix(world, position, scale, float3{ 0.0f,0.0f,0.0f });

	DrawSpriteData data;
	data.alpha = a_alpha;
	DrawSprite(a_texture, world, view, proj, data);
}





void PowerUpRenderer::DrawNode(const NodeRenderData& data, const CameraSpace::ICamera& camera)
{

	if (data.dirty)
	{
		UpdateBitmap(data);
		UploadTexture(data);
	}

	const DirectX::XMFLOAT4X4& view = camera.GetView();
	const DirectX::XMFLOAT4X4& proj= camera.GetProjection();
	
	DirectX::XMFLOAT4X4 world;
	float3 position;
	float3 scale;
	float2 offset;
	
	
	// バックフレーム描画
	offset.x = kBackFramePositionOffset.x * data.drawScale.x;
	offset.y = kBackFramePositionOffset.y * data.drawScale.y;
	position.x = data.drawPosition.x + offset.x;
	position.y = data.drawPosition.y + offset.y;
	position.z = 1.0f;
	scale.x = data.drawScale.x * kBackFrameScaleOffset;
	scale.y = data.drawScale.y * kBackFrameScaleOffset;
	scale.z = 1.0f;

	DrawMatrix::CreateWorldMatrix(world, position, scale, float3{ 0.0f,0.0f,0.0f });

	DrawSprite(m_backFrame, world, view, proj);

	// アイコンフレーム描画
	offset.x = kFramePositionOffset.x * data.drawScale.x * kBackFrameScaleOffset;
	offset.y = kFramePositionOffset.y * data.drawScale.y * kBackFrameScaleOffset;
	position.x = data.drawPosition.x + offset.x;
	position.y = data.drawPosition.y + offset.y;
	position.z = 1.0f;
	scale.x = data.drawScale.x * kFrameScaleOffset;
	scale.y = data.drawScale.y * kFrameScaleOffset;
	scale.z = 1.0f;

	DrawMatrix::CreateWorldMatrix(world, position, scale, float3{ 0.0f,0.0f,0.0f });

	DrawSprite(m_iconFrame, world, view, proj);

	// アイコン描画
	offset.x = kIconPositionOffset.x * data.drawScale.x * kBackFrameScaleOffset;
	offset.y = kIconPositionOffset.y * data.drawScale.y * kBackFrameScaleOffset;
	position.x = data.drawPosition.x + offset.x;
	position.y = data.drawPosition.y + offset.y;
	position.z = 1.0f;
	scale.x = data.drawScale.x * kIconScaleOffset;
	scale.y = data.drawScale.y * kIconScaleOffset;
	scale.z = 1.0f;

	DrawMatrix::CreateWorldMatrix(world, position, scale, float3{ 0.0f,0.0f,0.0f });

	DrawSprite(data.texture, world, view, proj);

	// コストアイコン描画
	offset.x = kCostIconPositionOffset.x * data.drawScale.x * kBackFrameScaleOffset;
	offset.y = kCostIconPositionOffset.y * data.drawScale.y * kBackFrameScaleOffset;
	position.x = data.drawPosition.x + offset.x;
	position.y = data.drawPosition.y + offset.y;
	position.z = 1.0f;
	scale.x = data.drawScale.x * kCostIconScaleOffset;
	scale.y = data.drawScale.y * kCostIconScaleOffset;
	scale.z = 1.0f;

	DrawMatrix::CreateWorldMatrix(world, position, scale, float3{ 0.0f,0.0f,0.0f });

	DrawSprite(m_cost, world, view, proj);
	


	// 強化名描画
	offset.x = kNamePositionOffset.x * data.drawScale.x * kBackFrameScaleOffset;
	offset.y = kNamePositionOffset.y * data.drawScale.y * kBackFrameScaleOffset;
	position.x = data.drawPosition.x + offset.x;
	position.y = data.drawPosition.y + offset.y;
	position.z = 1.0f;
	scale.x = data.drawScale.x * kNameScaleOffset;
	scale.y = data.drawScale.y * kNameScaleOffset;
	scale.z = 1.0f;


	DrawMatrix::CreateWorldMatrix(world, position, scale, float3{ 0.0f,0.0f,0.0f });

	float nameSize = std::min(float(kNodeNameCharCount / (float)data.powerUpName.size()), 1.0f);
	RenderString(data, world, view, proj, NodeName, nameSize);

	// 説明描画
	offset.x = kInfomationPositionOffset.x * data.drawScale.x * kBackFrameScaleOffset;
	offset.y = kInfomationPositionOffset.y * data.drawScale.y * kBackFrameScaleOffset;
	position.x = data.drawPosition.x + offset.x;
	position.y = data.drawPosition.y + offset.y;
	position.z = 1.0f;
	scale.x = data.drawScale.x * kInfomationScaleOffset;
	scale.y = data.drawScale.y * kInfomationScaleOffset;
	scale.z = 1.0f;

	DrawMatrix::CreateWorldMatrix(world, position, scale, float3{ 0.0f,0.0f,0.0f });

	float infoSize = std::min(float(kInfoCharCount  / (float)data.powerUpInfomation.size()), 1.0f);
	RenderString(data, world, view, proj, NodeInfomation, infoSize);


	// コスト描画
	offset.x = kLevelPositionOffset.x * data.drawScale.x * kBackFrameScaleOffset;
	offset.y = kLevelPositionOffset.y * data.drawScale.y * kBackFrameScaleOffset;
	position.x = data.drawPosition.x + offset.x;
	position.y = data.drawPosition.y + offset.y;
	position.z = 1.0f;
	scale.x = data.drawScale.x * kLevelScaleOffset;
	scale.y = data.drawScale.y * kLevelScaleOffset;
	scale.z = 1.0f;

	DrawMatrix::CreateWorldMatrix(world, position, scale, float3{ 0.0f,0.0f,0.0f });

	RenderString(data, world, view, proj, NodeLevel, 1.0f);

	offset.x = kCostPositionOffset.x * data.drawScale.x * kBackFrameScaleOffset;
	offset.y = kCostPositionOffset.y * data.drawScale.y * kBackFrameScaleOffset;
	position.x = data.drawPosition.x + offset.x;
	position.y = data.drawPosition.y + offset.y;
	position.z = 1.0f;
	scale.x = data.drawScale.x * kCostScaleOffset;
	scale.y = data.drawScale.y * kCostScaleOffset;
	scale.z = 1.0f;

	DrawMatrix::CreateWorldMatrix(world, position, scale, float3{ 0.0f,0.0f,0.0f });

	RenderString(data, world, view, proj, NodeCost, 1.0f);
}

void PowerUpRenderer::DrawBackground(const CameraSpace::ICamera& camera, float uvProgress)
{
	if (!m_background)
	{
		return;
	}

	SetSamplerState(SAMPLER_LINEAR);
	const DirectX::XMFLOAT4X4& view = camera.GetView();
	const DirectX::XMFLOAT4X4& proj = camera.GetProjection();

	DirectX::XMFLOAT4X4 world;
	float3 position;
	float3 scale;

	// 画面中央・最背面
	position = { 0.0f, 0.0f, 2.4f };
	scale = { 3.6f, 2.0f, 1.0f };

	DrawMatrix::CreateWorldMatrix(
		world,
		position,
		scale,
		float3{ 0.0f, 0.0f, 0.0f }
	);
	DrawSpriteData data;
	data.uvPos.x = uvProgress * kUVMoveRatioX;
	data.uvPos.y = uvProgress * -1.0f;
	DrawSprite(m_background, world, view, proj, data);
}

const std::wstring PowerUpRenderer::UTF8toWString(const std::string srcUTF8) const
{
	//Unicodeへ変換後の文字列長を得る
	int lenghtUnicode = MultiByteToWideChar(CP_UTF8, 0, srcUTF8.c_str(), srcUTF8.size() + 1, NULL, 0);

	//必要な分だけUnicode文字列のバッファを確保
	wchar_t* bufUnicode = new wchar_t[lenghtUnicode];

	//UTF8からUnicodeへ変換
	MultiByteToWideChar(CP_UTF8, 0, srcUTF8.c_str(), srcUTF8.size() + 1, bufUnicode, lenghtUnicode);

	std::wstring strSJis(bufUnicode);

	delete[] bufUnicode;

	return strSJis;
}

void PowerUpRenderer::DrawSprite(std::shared_ptr<Texture2D> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix, const DrawSpriteData& drawData)
{
	// 各マトリクスと描画用のパラメーター
	DirectX::XMFLOAT4X4 wvp[3];
	DirectX::XMFLOAT4 param[3];

	// 各マトリクスを設定
	wvp[0] = worldMatrix;
	wvp[1] = viewMatrix;
	wvp[2] = projMatrix;

	
	// ステータスを設定
	param[0] = DirectX::XMFLOAT4(drawData.projOffset.x, drawData.projOffset.y, drawData.projScale.x, drawData.projScale.y);
	param[1] = DirectX::XMFLOAT4(drawData.uvPos.x, drawData.uvPos.y, drawData.uvScale.x, drawData.uvScale.y);
	param[2] = DirectX::XMFLOAT4(drawData.r, drawData.g, drawData.b, drawData.alpha);


	// シェーダーをバインド
	ShaderList::SetWVP(wvp);

	vs->WriteBuffer(0, wvp);
	vs->WriteBuffer(1, param);
	vs->Bind();

	ps->SetTexture(0, texture->Get());
	ps->Bind();

	// 描画
	MeshDraw();


}

void PowerUpRenderer::DrawSprite(std::shared_ptr<Texture2D> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix)
{
	// 各マトリクスと描画用のパラメーター
	DirectX::XMFLOAT4X4 wvp[3];
	DirectX::XMFLOAT4 param[3];

	// 各マトリクスを設定
	wvp[0] = worldMatrix;
	wvp[1] = viewMatrix;
	wvp[2] = projMatrix;


	// ステータスを設定
	param[0] = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	param[1] = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	param[2] = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);


	// シェーダーをバインド
	ShaderList::SetWVP(wvp);

	vs->WriteBuffer(0, wvp);
	vs->WriteBuffer(1, param);
	vs->Bind();

	ps->SetTexture(0, texture->Get());
	ps->Bind();

	// 描画
	MeshDraw();

}


void PowerUpRenderer::DrawSprite(std::shared_ptr<Texture> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix, const DrawSpriteData& drawData)
{
	// 各マトリクスと描画用のパラメーター
	DirectX::XMFLOAT4X4 wvp[3];
	DirectX::XMFLOAT4 param[3];

	// 各マトリクスを設定
	wvp[0] = worldMatrix;
	wvp[1] = viewMatrix;
	wvp[2] = projMatrix;


	// ステータスを設定
	param[0] = DirectX::XMFLOAT4(drawData.projOffset.x, drawData.projOffset.y, drawData.projScale.x, drawData.projScale.y);
	param[1] = DirectX::XMFLOAT4(drawData.uvPos.x, drawData.uvPos.y, drawData.uvScale.x, drawData.uvScale.y);
	param[2] = DirectX::XMFLOAT4(drawData.r, drawData.g, drawData.b, drawData.alpha);



	// シェーダーをバインド
	ShaderList::SetWVP(wvp);

	vs->WriteBuffer(0, wvp);
	vs->WriteBuffer(1, param);
	vs->Bind();
	ps->SetTexture(0, texture->GetResource());
	ps->Bind();

	// 描画
	MeshDraw();
}






void PowerUpRenderer::MeshDraw()
{
	// コンテキストを取得
	ID3D11DeviceContext* pContext = GetContext();
	UINT offset = 0;

	// 頂点をバインド
	const Core::VertexBuffer& vertexBuffer = m_spriteMesh->GetVertexBuffer();
	vertexBuffer.Bind(0, offset);

	// 頂点バッファのみで描画
	GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->Draw(vertexBuffer.GetCount(), 0);

}


bool PowerUpRenderer::CreateTextResources()
{
#if !defined(_WIN32)
	return false;
#else
	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = static_cast<LONG>(m_config.textureWidth);
	info.bmiHeader.biHeight = -static_cast<LONG>(m_config.textureHeight);
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;

	m_memoryDC = CreateCompatibleDC(nullptr);
	if (!m_memoryDC)
	{
		return false;
	}

	m_bitmap = CreateDIBSection(m_memoryDC, &info, DIB_RGB_COLORS, &m_rawBits, nullptr, 0);
	if (!m_bitmap)
	{
		return false;
	}
	m_prevBitmap = SelectObject(m_memoryDC, m_bitmap);

	LOGFONTA font = {};
	font.lfHeight = -static_cast<LONG>(m_config.fontSizePixels);
	font.lfWeight = FW_SEMIBOLD;
	strcpy_s(font.lfFaceName, "Segoe UI");
	m_font = CreateFontIndirectA(&font);
	if (!m_font)
	{
		return false;
	}
	m_prevFont = SelectObject(m_memoryDC, m_font);

	SetBkMode(m_memoryDC, TRANSPARENT);
	SetTextColor(m_memoryDC, RGB(255, 255, 255));

	return true;
#endif
}


void PowerUpRenderer::DestroyTextResources()
{
#if defined(_WIN32)
	if (m_memoryDC && m_prevFont)
	{
		SelectObject(m_memoryDC, m_prevFont);
	}
	if (m_font)
	{
		DeleteObject(m_font);
	}
	m_prevFont = nullptr;
	m_font = nullptr;

	if (m_memoryDC && m_prevBitmap)
	{
		SelectObject(m_memoryDC, m_prevBitmap);
	}
	if (m_bitmap)
	{
		DeleteObject(m_bitmap);
	}
	m_prevBitmap = nullptr;
	m_bitmap = nullptr;
	m_rawBits = nullptr;

	if (m_memoryDC)
	{
		DeleteDC(m_memoryDC);
	}
	m_memoryDC = nullptr;
#endif
}

void PowerUpRenderer::UploadTexture(const NodeRenderData& data)
{
	if (!m_textureDirty || !m_rawBits || m_rgbaBuffer.empty() || !m_texture)
	{
		return;
	}

	const size_t pixelCount = static_cast<size_t>(data.stringTexture->GetWidth()) * static_cast<size_t>(data.stringTexture->GetHeight());
	uint8_t* src = static_cast<uint8_t*>(m_rawBits);
	uint8_t* dst = m_rgbaBuffer.data();
	for (size_t i = 0; i < pixelCount; ++i)
	{
		const uint8_t b = src[0];
		const uint8_t g = src[1];
		const uint8_t r = src[2];
		const uint8_t intensity = std::max({ r, g, b });
		dst[0] = 255;
		dst[1] = 255;
		dst[2] = 255;
		dst[3] = intensity;
		src += 4;
		dst += 4;
	}

	data.stringTexture->UpdatePixels(m_rgbaBuffer.data(), data.stringTexture->GetWidth() * 4);
	m_textureDirty = false;

	// 文字の書かれている範囲を格納
	DetectTextBounds(data.stringTexture->GetBound(), m_rgbaBuffer.data(), data.stringTexture->GetWidth(), data.stringTexture->GetHeight());
	
	TEXTMETRIC tm;
	GetTextMetrics(m_memoryDC, &tm);

	

	float quadWidth = static_cast<float>(tm.tmWeight);
	float quadHeight = static_cast<float>(tm.tmHeight);

	data.stringTexture->SetScale(float2{ quadWidth, quadHeight });
}


void PowerUpRenderer::UpdateBitmap(const NodeRenderData& data)
{
#if defined(_WIN32)
	if (!m_memoryDC)
	{
		return;
	}

	if(m_config.textureWidth != data.stringTexture->GetWidth() || m_config.textureHeight != data.stringTexture->GetHeight())
	{
		return;
	}

	PatBlt(m_memoryDC, 0, 0, static_cast<int>(data.stringTexture->GetWidth()), static_cast<int>(data.stringTexture->GetHeight()), BLACKNESS);

	const std::wstring hudText = BuildHudString(data);

	RECT textRect = {};
	textRect.left = static_cast<LONG>(m_config.innerPaddingPixels);
	textRect.top = static_cast<LONG>(m_config.innerPaddingPixels);
	textRect.right = static_cast<LONG>(data.stringTexture->GetWidth()) - static_cast<LONG>(m_config.innerPaddingPixels);
	textRect.bottom = static_cast<LONG>(data.stringTexture->GetHeight()) - static_cast<LONG>(m_config.innerPaddingPixels);

	RECT shadowRect = textRect;
	shadowRect.left += static_cast<LONG>(m_config.shadowOffsetX);
	shadowRect.top += static_cast<LONG>(m_config.shadowOffsetY);
	shadowRect.right += static_cast<LONG>(m_config.shadowOffsetX);
	shadowRect.bottom += static_cast<LONG>(m_config.shadowOffsetY);

	SetTextColor(m_memoryDC, RGB(0, 0, 0));
	DrawText(m_memoryDC, hudText.c_str(), -1, &shadowRect, DT_LEFT | DT_TOP | DT_NOPREFIX);

	SetTextColor(m_memoryDC, RGB(255, 255, 255));
	DrawText(m_memoryDC, hudText.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_NOPREFIX);

	GdiFlush();
	m_textureDirty = true;


	TEXTMETRIC tm;
	GetTextMetrics(m_memoryDC, &tm);
	
	textPixelWidth = 300;
	textPixelHeight = tm.tmHeight;
	
	
#else
	(void)data;
#endif
}

void PowerUpRenderer::RenderString(const NodeRenderData& data, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix, int uvNum, float size)
{
	float viewportWidth = 0.0f;
	float viewportHeight = 0.0f;
	if (!TryGetViewportSize(viewportWidth, viewportHeight))
	{
		return;
	}

	float2 scale = data.stringTexture->GetScale();
	UVRegion region = ComputeUV(data.stringTexture->GetBound(), data.stringTexture->GetHeight());


	uvNum++;

	float uvStartY = (scale.y * uvNum / data.stringTexture->GetHeight() + region.startY);
	float height = (m_config.fontSizePixels) / data.stringTexture->GetHeight();
	
	float2 uvStart = float2{ 0.0f, uvStartY};
	float2 uvSize = float2{ 1.0f, height};

	float width = static_cast<float>(scale.x / scale.y);

	float2 pos(0.0f, 0.0f);
	pos.x -= (width - width * size) / 2;
	
	SetBlendMode(BLEND_ALPHA);

	DrawSprite(data.stringTexture, worldMatrix, viewMatrix, projMatrix, DrawSpriteData(uvStart, uvSize, pos, float2{ width * size, size }, float3{0.0f, 0.0f, 0.0f}));

}

void PowerUpRenderer::DetectTextBounds(TextBounds& bounds, const uint8_t* rgba, int width, int height)
{
	TextBounds result{};
	result.top = height;     // 初期値は最大
	result.bottom = 0;       // 初期値は最小

	const int stride = width * 4;

	// 色が塗られている最初のピクセルと最後のピクセルの高さを取得
	for (int y = 0; y < height; ++y)
	{
		const uint8_t* row = rgba + y * stride;

		for (int x = 0; x < width; ++x)
		{
			uint8_t alpha = row[x * 4 + 3]; // ← 強度が alpha に入っている前提

			if (alpha > 30)
			{
				if (y < result.top) result.top = y;
				if (y > result.bottom) result.bottom = y;
			}
		}
	}

	// 一文字も描かれてなかった場合の保険
	if (result.top > result.bottom)
	{
		result.top = 0;
		result.bottom = 0;
	}

	result.height = (result.bottom - result.top) + 1;
	bounds = result;
}



bool PowerUpRenderer::TryGetViewportSize(float& width, float& height) const
{
	RenderTarget* rtv = GetDefaultRTV();
	if (!rtv)
	{
		return false;
	}

	width = static_cast<float>(rtv->GetWidth());
	height = static_cast<float>(rtv->GetHeight());
	return width > 0.0f && height > 0.0f;
}
std::wstring PowerUpRenderer::BuildHudString(const NodeRenderData& data) const
{

	std::wostringstream stream;
	// ダミーとして出来るだけ開始位置の高い文字を置いておく
	stream << L"ピポ弩" << '\n';

	stream << UTF8toWString(data.powerUpName) << '\n';

	stream << UTF8toWString(data.powerUpInfomation) << '\n';


	if (data.level == data.maxLevel)
	{
		std::wstring level = L"Max";
		stream << "Lv." << level << '\n';
		stream << '\n';
	}
	else if(data.level == 0)
	{
		stream << L"未開放" << '\n';
		stream << data.cost << '\n';
	}
	else
	{
		std::wstring level = std::to_wstring((int)data.level);
		stream << "Lv." << level << '\n';
		stream << data.cost << '\n';
	}
	

	return stream.str();
}

PowerUpRenderer::UVRegion PowerUpRenderer::ComputeUV(const TextBounds& bounds, int textureHeight)
{
	UVRegion uv{};
	uv.startY = float(bounds.top) / float(textureHeight);
	uv.height = float(bounds.height) / float(textureHeight);
	return uv;
}
