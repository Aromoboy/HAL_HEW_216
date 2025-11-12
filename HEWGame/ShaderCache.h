#ifndef __SHADERCACHE_H__
#define __SHADERCACHE_H__

#include <string>
#include <map>
#include <memory>
#include <d3d11.h>
#include <wrl/client.h>

namespace Core
{
    using Microsoft::WRL::ComPtr;

    class ShaderProgram
    {
    public:
        ShaderProgram() = default;
        ~ShaderProgram() = default;

        bool LoadFromFiles(ID3D11Device* device, const std::string& vsPath, const std::string& psPath);
        void Bind(ID3D11DeviceContext* context);

    private:
        ComPtr<ID3D11VertexShader> vertexShader;
        ComPtr<ID3D11PixelShader> pixelShader;
        ComPtr<ID3D11InputLayout> inputLayout;
    };

    class ShaderCache
    {
    public:
        ShaderProgram& Load(const std::string& name, ID3D11Device* device,
            const std::string& vsPath, const std::string& psPath);

        ShaderProgram& Get(const std::string& name);

    private:
        std::map<std::string, std::unique_ptr<ShaderProgram>> programs;
    };
}
#endif //__SHADERCACHE_H__