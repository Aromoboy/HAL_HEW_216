// エンジン全体の共有資源への入り口をまとめている
// シーンや描画がここから必要な機能を取得する

#pragma once

#include "Core/DXContext.h"
#include "ModelRender.h"
#include "ShaderCache.h"
#include "TextureCache.h"

namespace App
{
    class Game
    {
    public:
        static Game& Instance();

        Core::DXContext& GetDXContext();
        const Core::DXContext& GetDXContext() const;
        ShaderCache& GetShaderCache();
        TextureCache& GetTextureCache();
        Render::ModelRenderer& GetModelRenderer();

    private:
        Game();

        Core::DXContext m_dxContext;
        ShaderCache& m_shaderCache;
        TextureCache& m_textureCache;
        Render::ModelRenderer m_modelRenderer;
    };
}
