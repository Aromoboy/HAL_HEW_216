#include "TimerDisplay.h"
#include "DirectX.h"
#include "Defines.h"
#include "MaterialDX.h"
#include "Transform.h"

#include <algorithm>
#include <array>
#include <vector>

TimerDisplay::TimerDisplay()
    : m_numberTexture(nullptr)
    , m_posX(700.0f)
    , m_posY(50.0f)
    , m_digitWidth(50.0f)
    , m_digitHeight(70.0f)
    , m_digitSpacing(55.0f)
{
}

TimerDisplay::~TimerDisplay()
{
}

void TimerDisplay::Init(std::shared_ptr<Texture2D> numberTexture)
{
    m_numberTexture = std::move(numberTexture);
    m_modelRenderer = std::make_unique<ModelRender>();
    m_digitMeshes.fill(nullptr);
    m_ready = static_cast<bool>(m_numberTexture);
}

void TimerDisplay::SetPosition(float x, float y)
{
    m_posX = x;
    m_posY = y;
}

void TimerDisplay::SetDigitSize(float width, float height)
{
    m_digitWidth = width;
    m_digitHeight = height;
    m_digitSpacing = width + 5.0f;
}

void TimerDisplay::Draw(float remainingSeconds)
{
    if (!m_ready || !m_modelRenderer || !m_numberTexture)
    {
        return;
    }

    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    if (!TryGetViewportSize(viewportWidth, viewportHeight))
    {
        return;
    }

    SetDepthTest(false);
    SetBlendMode(BLEND_ALPHA);

    int totalSeconds = static_cast<int>(remainingSeconds);
    if (totalSeconds < 0) totalSeconds = 0;

    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    int min10 = (minutes / 10) % 10;
    int min1 = minutes % 10;
    int sec10 = (seconds / 10) % 10;
    int sec1 = seconds % 10;

    float drawX = m_posX;
    const float drawY = m_posY;

    DrawDigit(min10, drawX, drawY);
    drawX += m_digitSpacing;
    DrawDigit(min1, drawX, drawY);
    drawX += m_digitSpacing * 0.6f; // small gap for ':'
    DrawDigit(sec10, drawX, drawY);
    drawX += m_digitSpacing;
    DrawDigit(sec1, drawX, drawY);

    SetDepthTest(true);
}

void TimerDisplay::DrawDigit(int digit, float x, float y)
{
    if (digit < 0 || digit > 9 || !m_modelRenderer || !m_numberTexture)
    {
        return;
    }

    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    if (!TryGetViewportSize(viewportWidth, viewportHeight))
    {
        return;
    }

    const float widthClip = (m_digitWidth / viewportWidth) * 2.0f;
    const float heightClip = (m_digitHeight / viewportHeight) * 2.0f;
    const float centerXClip = -1.0f + (x / viewportWidth) * 2.0f + (widthClip * 0.5f);
    const float centerYClip = 1.0f - (y / viewportHeight) * 2.0f - (heightClip * 0.5f);

    auto mesh = GetDigitMesh(digit);
    if (!mesh)
    {
        return;
    }

    auto material = std::make_shared<MaterialDX>(
        DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
        DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
        m_numberTexture);

    Render::Model model(
        std::vector<std::shared_ptr<MeshDX>>{ mesh },
        std::vector<std::shared_ptr<MaterialDX>>{ material });

    Transform transform{};
    transform.position = float3{ centerXClip, centerYClip, 0.0f };
    transform.rotation = float3{ 0.0f, 0.0f, 0.0f };
    transform.scale = float3{ widthClip, heightClip, 1.0f };

    struct HudCamera : public CameraSpace::ICamera
    {
        HudCamera()
        {
            DirectX::XMStoreFloat4x4(&view, DirectX::XMMatrixIdentity());
            DirectX::XMStoreFloat4x4(&proj, DirectX::XMMatrixOrthographicLH(2.0f, 2.0f, 0.0f, 1.0f));
        }
        const DirectX::XMFLOAT4X4& GetView() const override { return view; }
        const DirectX::XMFLOAT4X4& GetProjection() const override { return proj; }
    protected:
        void RecalculateView() override {}
    private:
        DirectX::XMFLOAT4X4 view{};
        DirectX::XMFLOAT4X4 proj{};
    };

    static HudCamera hudCamera{};
    m_modelRenderer->Draw(model, transform, &hudCamera);
}

bool TimerDisplay::TryGetViewportSize(float& width, float& height) const
{
    RenderTargetDX* rtv = GetDefaultRTV();
    if (!rtv)
    {
        return false;
    }

    width = static_cast<float>(rtv->GetWidth());
    height = static_cast<float>(rtv->GetHeight());
    return width > 0.0f && height > 0.0f;
}

std::shared_ptr<MeshDX> TimerDisplay::GetDigitMesh(int digit)
{
    digit = std::clamp(digit, 0, 9);
    auto& cached = m_digitMeshes[static_cast<size_t>(digit)];
    if (cached)
    {
        return cached;
    }

    const float uvWidth = 0.1f;
    const float uMin = uvWidth * static_cast<float>(digit);
    const float uMax = uMin + uvWidth;

    const std::array<MeshDX::Vertex, 4> vertices{
        MeshDX::Vertex{ { -0.5f,  0.5f, 0.0f }, { uMin, 0.0f } },
        MeshDX::Vertex{ {  0.5f,  0.5f, 0.0f }, { uMax, 0.0f } },
        MeshDX::Vertex{ { -0.5f, -0.5f, 0.0f }, { uMin, 1.0f } },
        MeshDX::Vertex{ {  0.5f, -0.5f, 0.0f }, { uMax, 1.0f } },
    };

    MeshDX::Description desc{};
    desc.pVtx = vertices.data();
    desc.vtxSize = sizeof(MeshDX::Vertex);
    desc.vtxCount = static_cast<UINT>(vertices.size());
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

    cached = std::make_shared<MeshDX>(desc, 0);
    return cached;
}
