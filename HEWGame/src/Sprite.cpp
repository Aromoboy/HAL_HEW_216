#include "Sprite.h"

Sprite::Data Sprite::m_data;
std::shared_ptr<VertexShader> Sprite::m_defVS;
std::shared_ptr<PixelShader> Sprite::m_defPS;

void Sprite::Init()
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

        MeshDX::Vertex vtx[] = {
                {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}},
                {{ 0.5f, 0.5f, 0.0f}, {1.0f, 0.0f}},
                {{-0.5f,-0.5f, 0.0f}, {0.0f, 1.0f}},
                {{ 0.5f,-0.5f, 0.0f}, {1.0f, 1.0f}},
        };

        uint32_t idx[] = { 0, 1, 2, 2, 1, 3 };

        MeshDX::Description desc{};
        desc.pVtx = vtx;
        desc.vtxSize = sizeof(MeshDX::Vertex);
        desc.vtxCount = _countof(vtx);
        desc.pIdx = idx;
        desc.idxSize = sizeof(uint32_t);
        desc.idxCount = _countof(idx);
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        m_data.mesh = std::make_shared<MeshDX>(desc, -1);

        // p[^[
        m_data.param[0] = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
        m_data.param[1] = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
        m_data.param[2] = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        DirectX::XMStoreFloat4x4(&m_data.matrix[0], DirectX::XMMatrixIdentity());
        DirectX::XMStoreFloat4x4(&m_data.matrix[1], DirectX::XMMatrixIdentity());
        DirectX::XMStoreFloat4x4(&m_data.matrix[2], DirectX::XMMatrixIdentity());

        // VF[_[
        m_defVS = std::make_shared<VertexShader>();
        m_defVS->Compile(VS);
        m_data.vs = m_defVS.get();
        m_defPS = std::make_shared <PixelShader>();
        m_defPS->Compile(PS);
        m_data.ps = m_defPS.get();
}
void Sprite::Uninit()
{
}
void Sprite::Draw()
{
        m_data.vs->WriteBuffer(0, m_data.matrix);
        m_data.vs->WriteBuffer(1, m_data.param);
        m_data.vs->Bind();
        m_data.ps->SetTexture(0, m_data.texture);
        m_data.ps->Bind();

        ID3D11DeviceContext* pContext = GetContext();
        UINT offset = 0;
        const Core::VertexBuffer& vertexBuffer = m_data.mesh->GetVertexBuffer();
        vertexBuffer.Bind(0, offset);

        if (m_data.mesh->HasIndexBuffer())
        {
                const Core::IndexBuffer* indexBuffer = m_data.mesh->GetIndexBuffer();
                indexBuffer->Bind();
                GetContext()->IASetPrimitiveTopology(m_data.mesh->GetTopology());
                pContext->DrawIndexed(m_data.mesh->GetIndexCount(), 0, 0);
        }
        else
        {
                GetContext()->IASetPrimitiveTopology(m_data.mesh->GetTopology());
                pContext->Draw(vertexBuffer.GetCount(), 0);
        }
}

void Sprite::SetOffset(DirectX::XMFLOAT2 offset)
{
        m_data.param[0].x = offset.x;
        m_data.param[0].y = offset.y;
}
void Sprite::SetSize(DirectX::XMFLOAT2 size)
{
        m_data.param[0].z = size.x;
        m_data.param[0].w = size.y;
}

void Sprite::SetUVPos(DirectX::XMFLOAT2 pos)
{
        m_data.param[1].x = pos.x;
        m_data.param[1].y = pos.y;
}
void Sprite::SetUVScale(DirectX::XMFLOAT2 scale)
{
        m_data.param[1].z = scale.x;
        m_data.param[1].w = scale.y;
}
void Sprite::SetColor(DirectX::XMFLOAT4 color)
{
        m_data.param[2] = color;
}
void Sprite::SetTexture(const std::shared_ptr<Texture2D>& tex)
{
        m_data.texture = tex;
}
void Sprite::SetWorld(DirectX::XMFLOAT4X4 world)
{
        m_data.matrix[0] = world;
}
void Sprite::SetView(DirectX::XMFLOAT4X4 view)
{
        m_data.matrix[1] = view;
}
void Sprite::SetProjection(DirectX::XMFLOAT4X4 proj)
{
        m_data.matrix[2] = proj;
}
void Sprite::SetVertexShader(Shader* vs)
{
        if (vs && typeid(VertexShader) == typeid(*vs))
                m_data.vs = vs;
        else
                m_data.vs = m_defVS.get();
}
void Sprite::SetPixelShader(Shader* ps)
{
        if (ps && typeid(PixelShader) == typeid(*ps))
                m_data.ps = ps;
        else
                m_data.ps = m_defPS.get();
}
