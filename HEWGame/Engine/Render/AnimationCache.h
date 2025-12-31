// エンジンのアニメーション資源を共有する保管庫をまとめている
// モデル読み込みと描画から参照される

#pragma once



#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include "Animation.h"
#include "IModelLoader.h"

struct ID3D11Device;
class TextureCache;

namespace Render
{
    class AnimationCache
    {
    public:
        static AnimationCache& Instance();

        std::shared_ptr<Animation>  Get(const std::string& path);
        void SetLoader(std::unique_ptr<IModelLoader> loader, ID3D11Device* device);
        bool HasLoader() const noexcept { return static_cast<bool>(m_loader); }
        bool IsConfiguredFor(ID3D11Device* device) const noexcept;
        void Clear();
        void Reset();
       

    private:
        AnimationCache() = default;
        ~AnimationCache() = default;
        AnimationCache(const AnimationCache&) = delete;
        AnimationCache& operator=(const AnimationCache&) = delete;

    private:
        std::unique_ptr<IModelLoader> m_loader;
        ID3D11Device* m_device = nullptr;
        std::map<std::string, std::shared_ptr<Animation>> m_animations;
        std::unordered_set<std::string> m_inFlightKeys;
       

    };
}
