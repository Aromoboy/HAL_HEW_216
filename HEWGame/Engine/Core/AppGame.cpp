// エンジン全体の共有資源への入り口をまとめている
// シーンや描画がここから必要な機能を取得する

#include "AppGame.h"

namespace App
{
    Game& Game::Instance()
    {
        static Game instance;
        return instance;
    }

    Game::Game()
        : m_dxContext()
        , m_shaderCache(ShaderCache::Instance())
        , m_textureCache(TextureCache::Instance())
        , m_modelRenderer()
    {
    }

    Core::DXContext& Game::GetDXContext()
    {
        return m_dxContext;
    }

    const Core::DXContext& Game::GetDXContext() const
    {
        return m_dxContext;
    }

    ShaderCache& Game::GetShaderCache()
    {
        return m_shaderCache;
    }

    TextureCache& Game::GetTextureCache()
    {
        return m_textureCache;
    }

    Render::ModelRenderer& Game::GetModelRenderer()
    {
        return m_modelRenderer;
    }
}
