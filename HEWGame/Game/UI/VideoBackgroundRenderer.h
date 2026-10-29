#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "MeshBuffer.h"
#include "Shader.h"

class VideoBackgroundRenderer
{
public:
    struct ShaderPaths
    {
        std::filesystem::path vertexShader;
        std::filesystem::path pixelShader;
    };

    VideoBackgroundRenderer();
    ~VideoBackgroundRenderer();

    bool Initialize(ID3D11Device* device,
        ID3D11DeviceContext* context,
        const ShaderPaths& shader,
        const std::string& mp4Path, float brightness = 1.0f);
    void Update(float dt);
    bool IsFinished() const;
    void SetAutoLoop(bool enabled);
    ID3D11ShaderResourceView* GetTexture() const;
    void Draw();
    void Shutdown();

private:
    struct DecoderState;

    bool CreateFullScreenQuad(float brightness);
    bool LoadShaders(const ShaderPaths& shader);
    bool CreateVideoTexture(uint32_t width, uint32_t height);
    bool UpdateTextureFromIYUV(const uint8_t* data, size_t dataSize);

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    std::unique_ptr<VertexShader> m_vertexShader;
    std::unique_ptr<PixelShader> m_pixelShader;
    std::unique_ptr<MeshBuffer> m_quad;
    DirectX::XMFLOAT4X4 m_matrices[3]{};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
    std::unique_ptr<DecoderState> m_decoder;
    uint32_t m_frameWidth = 0;
    uint32_t m_frameHeight = 0;
    bool m_ready = false;
    bool m_mfStarted = false;
};
