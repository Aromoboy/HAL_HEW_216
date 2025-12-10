#include "PowerUpRenderer.h"

#include "Defines.h"
#include "PowerUpTextures.h"

namespace
{
    constexpr float2 kBackFramePositionOffset = { 0.00f,  0.000f };
    constexpr float2 kFramePositionOffset = { -0.18f,  -0.008f };
    constexpr float2 kIconPositionOffset = { -0.18f,  -0.008f };
    constexpr float2 kCostIconPositionOffset = { 0.10f,  -0.050f };
    constexpr float2 kHudCostIconPositionOffset = { 0.40f,  0.30f };

    constexpr float kIconScaleOffset = 0.5f;
    constexpr float kFrameScaleOffset = 0.5f;
    constexpr float kBackFrameScaleOffset = 1.8f;
    constexpr float kCostIconScaleOffset = 0.3f;
    constexpr float kHudCostIconScaleOffset = 0.25f;

    Transform MakeTransform(const float2& position, const float2& scale)
    {
        Transform transform{};
        transform.position = float3{ position.x, position.y, 0.0f };
        transform.rotation = float3{ 0.0f, 0.0f, 0.0f };
        transform.scale = float3{ scale.x, scale.y, 1.0f };
        return transform;
    }
}

PowerUpStringTexture::PowerUpStringTexture()
    : m_texture(std::make_shared<Texture2D>())
    , m_stringTextureScale(float2{ 1.0f, 1.0f })
{
}

HRESULT PowerUpStringTexture::Create(DXGI_FORMAT format, UINT width, UINT height)
{
    (void)format;
    m_width = width;
    m_height = height;
    return S_OK;
}

void PowerUpStringTexture::UpdatePixels(const void* data, UINT pitch)
{
    (void)data;
    (void)pitch;
}

void PowerUpStringTexture::SetScale(float2 a_scale)
{
    m_stringTextureScale = a_scale;
}

float2 PowerUpStringTexture::GetScale() const
{
    return m_stringTextureScale;
}

UINT PowerUpStringTexture::GetWidth() const
{
    return m_width;
}

UINT PowerUpStringTexture::GetHeight() const
{
    return m_height;
}

ID3D11ShaderResourceView* PowerUpStringTexture::Get() const
{
    return m_texture ? m_texture->Get() : nullptr;
}

std::shared_ptr<Texture2D> PowerUpStringTexture::GetTexture() const
{
    return m_texture;
}

PowerUpRenderer::PowerUpRenderer(TextureCache& a_cache)
    : m_cache(a_cache)
    , m_modelRenderer(std::make_unique<ModelRender>())
{
    const std::string costKey = PowerUpTextures::BuildKey(PowerUpTextures::kCostPath);
    const std::string frameKey = PowerUpTextures::BuildKey(PowerUpTextures::kFramePath);
    const std::string backFrameKey = PowerUpTextures::BuildKey(PowerUpTextures::kBackFramePath);

    m_cost = m_cache.GetTexture(costKey);
    m_iconFrame = m_cache.GetTexture(frameKey);
    m_backFrame = m_cache.GetTexture(backFrameKey);
}

PowerUpRenderer::~PowerUpRenderer() = default;

bool PowerUpRenderer::Initialize(const PowerUpHudLayoutConfig& config)
{
    m_config = config;
    return true;
}

void PowerUpRenderer::Shutdown()
{
    m_modelRenderer.reset();
    m_quadMesh.reset();
    m_cost.reset();
    m_iconFrame.reset();
    m_backFrame.reset();
}

void PowerUpRenderer::DrawHud(const CameraSpace::ICamera& camera)
{
    if (!m_cost)
    {
        return;
    }

    DrawSprite(m_cost, UVRect{}, kHudCostIconPositionOffset, float2{ kHudCostIconScaleOffset, kHudCostIconScaleOffset },
        DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), camera);
}

void PowerUpRenderer::DrawCursor(std::shared_ptr<Texture2D> a_texture, float2 a_position, float2 a_scale, const CameraSpace::ICamera& camera)
{
    if (!a_texture)
    {
        return;
    }

    const float2 scaledPosition{ a_position.x + kBackFramePositionOffset.x * a_scale.x,
        a_position.y + kBackFramePositionOffset.y * a_scale.y };
    const float2 scaledScale{ a_scale.x * kBackFrameScaleOffset, a_scale.y * kBackFrameScaleOffset };

    DrawSprite(a_texture, UVRect{}, scaledPosition, scaledScale, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), camera);
}

void PowerUpRenderer::DrawNode(const NodeRenderData& data, const CameraSpace::ICamera& camera)
{
    if (!m_backFrame || !m_iconFrame || !m_cost)
    {
        return;
    }

    const float2 backPos{ data.drawPosition.x + kBackFramePositionOffset.x * data.drawScale.x,
        data.drawPosition.y + kBackFramePositionOffset.y * data.drawScale.y };
    const float2 backScale{ data.drawScale.x * kBackFrameScaleOffset, data.drawScale.y * kBackFrameScaleOffset };
    DrawSprite(m_backFrame, UVRect{}, backPos, backScale, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), camera);

    const float2 framePos{ data.drawPosition.x + kFramePositionOffset.x * data.drawScale.x * kBackFrameScaleOffset,
        data.drawPosition.y + kFramePositionOffset.y * data.drawScale.y * kBackFrameScaleOffset };
    const float2 frameScale{ data.drawScale.x * kFrameScaleOffset, data.drawScale.y * kFrameScaleOffset };
    DrawSprite(m_iconFrame, UVRect{}, framePos, frameScale, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), camera);

    if (data.texture)
    {
        const float2 iconPos{ data.drawPosition.x + kIconPositionOffset.x * data.drawScale.x * kBackFrameScaleOffset,
            data.drawPosition.y + kIconPositionOffset.y * data.drawScale.y * kBackFrameScaleOffset };
        const float2 iconScale{ data.drawScale.x * kIconScaleOffset, data.drawScale.y * kIconScaleOffset };
        DrawSprite(data.texture, UVRect{}, iconPos, iconScale, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), camera);
    }

    const float2 costIconPos{ data.drawPosition.x + kCostIconPositionOffset.x * data.drawScale.x * kBackFrameScaleOffset,
        data.drawPosition.y + kCostIconPositionOffset.y * data.drawScale.y * kBackFrameScaleOffset };
    const float2 costIconScale{ data.drawScale.x * kCostIconScaleOffset, data.drawScale.y * kCostIconScaleOffset };
    DrawSprite(m_cost, UVRect{}, costIconPos, costIconScale, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), camera);
}

void PowerUpRenderer::DrawSprite(
    const std::shared_ptr<Texture2D>& texture,
    const UVRect& uvRect,
    const float2& position,
    const float2& scale,
    const DirectX::XMFLOAT4& tint,
    const CameraSpace::ICamera& camera)
{
    if (!texture || !m_modelRenderer)
    {
        return;
    }

    std::shared_ptr<MeshDX> mesh = (uvRect.start.x == 0.0f && uvRect.start.y == 0.0f &&
        uvRect.size.x == 1.0f && uvRect.size.y == 1.0f)
        ? GetQuadMesh()
        : CreateQuadMesh(uvRect);
    if (!mesh)
    {
        return;
    }

    auto material = std::make_shared<MaterialDX>(
        tint,
        DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
        DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
        texture);

    Render::Model model = BuildModel(material, mesh);
    const Transform transform = MakeTransform(position, scale);
    m_modelRenderer->Draw(model, transform, &camera);
}

std::shared_ptr<MeshDX> PowerUpRenderer::GetQuadMesh()
{
    auto mesh = m_quadMesh.lock();
    if (mesh)
    {
        return mesh;
    }

    mesh = CreateQuadMesh(UVRect{});
    m_quadMesh = mesh;
    return mesh;
}

std::shared_ptr<MeshDX> PowerUpRenderer::CreateQuadMesh(const UVRect& uvRect) const
{
    const std::vector<MeshDX::Vertex> vertices{
        MeshDX::Vertex{ DirectX::XMFLOAT3{ -0.5f,  0.5f, 0.0f }, DirectX::XMFLOAT2{ uvRect.start.x, uvRect.start.y } },
        MeshDX::Vertex{ DirectX::XMFLOAT3{  0.5f,  0.5f, 0.0f }, DirectX::XMFLOAT2{ uvRect.start.x + uvRect.size.x, uvRect.start.y } },
        MeshDX::Vertex{ DirectX::XMFLOAT3{ -0.5f, -0.5f, 0.0f }, DirectX::XMFLOAT2{ uvRect.start.x, uvRect.start.y + uvRect.size.y } },
        MeshDX::Vertex{ DirectX::XMFLOAT3{  0.5f, -0.5f, 0.0f }, DirectX::XMFLOAT2{ uvRect.start.x + uvRect.size.x, uvRect.start.y + uvRect.size.y } },
    };

    MeshDX::Description desc{};
    desc.pVtx = vertices.data();
    desc.vtxSize = sizeof(MeshDX::Vertex);
    desc.vtxCount = static_cast<UINT>(vertices.size());
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

    return std::make_shared<MeshDX>(desc, 0);
}

Render::Model PowerUpRenderer::BuildModel(const std::shared_ptr<MaterialDX>& material, const std::shared_ptr<MeshDX>& mesh) const
{
    std::vector<std::shared_ptr<MeshDX>> meshes{ mesh };
    std::vector<std::shared_ptr<MaterialDX>> materials{ material };
    return Render::Model(std::move(meshes), std::move(materials));
}

Transform PowerUpRenderer::MakeTransform(const float2& position, const float2& scale) const
{
    return ::MakeTransform(position, scale);
}

