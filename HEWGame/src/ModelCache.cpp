//ModelCache.cpp
#include "ModelCache.h"
#include "Debug.h"
#include "TextureCache.h"
#include <iostream>

namespace Render
{
    //============================================================
    // VOgCX^X̎擾
    // ĂяoɗB̃CX^X𐶐Aȍ~
    // CX^Xւ̎QƂԂ܂
    //============================================================
    ModelCache& ModelCache::Instance()
    {
        static ModelCache instance;  
        return instance;             
    }

    //============================================================
    // f[_[̐ݒ
    // O烂fǂݍݗp̃[_[IuWFNgo^܂
    //============================================================
    void ModelCache::SetLoader(std::shared_ptr<IModelLoader> loader, ID3D11Device* device, TextureCache& textureCache)
    {
        m_loader = loader;  //  L|C^Ń[_[ێ
        m_device = device;
        m_textureCache = &textureCache;

        if (!m_loader)
        {
            DebugMessage("[ModelCache] SetLoader called with null loader.");
        }

        if (!m_device)
        {
            DebugMessage("[ModelCache] SetLoader missing device pointer.");
        }

        if (!m_textureCache)
        {
            DebugMessage("[ModelCache] SetLoader missing texture cache.");
        }
    }

    //============================================================
    // f̎擾iLbVΉj
    // w肳ꂽpX̃fLbV猟A΂Ԃ܂B
    // Ȃ΃[_[găfǂݍ݁ALbVɕۑĂԂ܂
    //============================================================
    std::shared_ptr<ModelDX> ModelCache::Get(const std::string& path)
    {
        // LbVɂ邩
        auto it = m_models.find(path);
        if (it != m_models.end())
        {
            DebugMessage("[ModelCache] LbVꂽf𔭌: ");
            return it->second;  //  LbV̂łԂ
        }

        // [_[ݒ肳Ă邩mF
        if (!m_loader)
        {
            DebugMessage("[ModelCache] G[: [_[ݒ肳Ă܂I");
            return nullptr;  //  [_[ȂƓǂݍ݂łȂ
        }

        if (!m_device)
        {
            DebugMessage("[ModelCache] Device pointer not configured; aborting load.");
            return nullptr;
        }

        if (!m_textureCache)
        {
            DebugMessage("[ModelCache] Texture cache not configured; aborting load.");
            return nullptr;
        }

        // LbVɂȂ̂ŐVKɃ[h
        DebugMessage("[ModelCache] f[h: ");
        std::shared_ptr<ModelDX> model = m_loader->Load(path, m_device, *m_textureCache);

        // [hʂ̊mF
        if (model)
        {
            // : LbVɕۑĂ
            m_models[path] = model;
        }
        else
        {
            // s: G[bZ[Wo
            DebugMessage("[ModelCache] [hɎs܂: ");
        }

        return model;  //  [hfi܂nullptrjԂ
    }

    //============================================================
    // LbV̑SNA
    // ݕێĂ邷ׂẴfLbV폜܂
    //============================================================
    void ModelCache::Clear()
    {
        m_models.clear();  //  }bv̓eSď
    }
}

//============================================================
// ModelCache gp
//============================================================
/*
// O: Render::IModelLoader ̎NXi: FBXLoaderj
//       ID3D11Device* device 쐬ς

// 1. fLbV̎擾iVOgj
Render::ModelCache& modelCache = Render::ModelCache::Instance();

// 2. [_[̐ݒiK{Â݁j
modelCache.SetLoader(std::make_shared<FBXLoader>(device), device, TextureCache::Instance());

// 3. f̃[h
try {
    // Ft@C烍[hăLbVɓo^
    std::shared_ptr<ModelDX> robotModel = modelCache.Get("models/robot.fbx");

    // 4. f̕`i擾`惋[vŁj
    if (robotModel) {
        robotModel->Draw(context); // fNX̕`惁\bhz
    }

    // 5. LbV̍ėp
    // pXŎ擾ƃLbVԂiă[hȂj
    std::shared_ptr<ModelDX> sameModel = modelCache.Get("models/robot.fbx"); // LbVqbg

    // 6. ʂ̃f̃[h
    std::shared_ptr<ModelDX> mapModel = modelCache.Get("models/map.obj");

} catch (...) {
    std::cerr << "f[hG[" << std::endl;
}

// 7. LbṼNAisvȃfj
modelCache.Clear();
*/