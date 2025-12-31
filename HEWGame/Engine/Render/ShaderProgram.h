#ifndef __SHADERPROGRAM_H__
#define __SHADERPROGRAM_H__

#include <string>
#include <d3d11.h>
#include <wrl/client.h>

class ShaderProgram
{
public:
    bool LoadFromFiles(ID3D11Device* device,
        const std::string& vsPath,
        const std::string& psPath);

    void Bind(ID3D11DeviceContext* context);

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
};
#endif