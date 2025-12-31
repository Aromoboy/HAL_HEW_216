// エンジンのアニメーション資源を共有する保管庫をまとめている
// モデル読み込みと描画から参照される

#include "AnimationCache.h"
#include "Debug.h"
#include "TextureCache.h"
#include "Diagnostics/Logger.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>

#pragma execution_character_set("utf-8")




namespace
{
    std::string CanonicalizeCacheKey(const std::string& rawPath)
    {
        std::filesystem::path fsPath(rawPath);
        std::filesystem::path normalized = fsPath.lexically_normal();

        std::string key = normalized.generic_string();
        if (key.empty())
        {
            key = rawPath;
        }

#if defined(_WIN32) || defined(_WIN64)
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
#endif

        return key;
    }
}

namespace Render
{
    //============================================================
    AnimationCache& AnimationCache::Instance()
    {
        static AnimationCache instance;
        return instance;
    }

    //============================================================
    // ローダーの設定
    // 実際のロード処理を担う IModelLoader と必要な依存を登録する
    //============================================================
    void AnimationCache::SetLoader(std::unique_ptr<IModelLoader> loader, ID3D11Device* device)
    {
        const bool loaderChanged = m_loader && m_loader.get() != loader.get();
        const bool deviceChanged = m_device && m_device != device;

        const bool reconfiguringWithCachedModels = (!m_animations.empty()) && (loaderChanged || deviceChanged);

        if (reconfiguringWithCachedModels)
        {
            //MODELCACHE_LOG(
            Clear();
        }

        m_loader = std::move(loader);   // シングルトン内にローダーを保持
        m_device = device;              // 描画デバイス

        if (!m_loader)
        {
        }

        if (!m_device)
        {
        }
    }

    bool AnimationCache::IsConfiguredFor(ID3D11Device* device) const noexcept
    {
        return m_loader && m_device == device;
    }

    //============================================================
    // モデルの取得（キャッシュ対応）
    // 既にロード済みであればキャッシュから取り出し、無ければロードする
    //============================================================
    std::shared_ptr<Animation> AnimationCache::Get(const std::string& path)
    {
        const std::string cacheKey = CanonicalizeCacheKey(path);

        Diagnostics::LogInfo(
            std::string("[ModelCache] リクエスト='") + path + "' -> key='" + cacheKey + "'\n");

        // キャッシュに存在するか確認
        auto it = m_animations.find(cacheKey);
        if (it != m_animations.end())
        {
            Diagnostics::LogInfo(std::string("[ModelCache] key='") + cacheKey + "' ヒット\n");
            return it->second;  // キャッシュ済みのモデルを返す
        }

        if (m_inFlightKeys.find(cacheKey) != m_inFlightKeys.end())
        {
            Diagnostics::LogInfo(std::string("[ModelCache] key='") + cacheKey + "' ブロック: ロードが進行中\n");
            return nullptr;
        }

        Diagnostics::LogInfo(std::string("[ModelCache] key='") + cacheKey + "' ミス -> ロード\n");

        // ローダーが有効かどうか確認
        if (!m_loader)
        {
            return nullptr;  // ローダーが無ければロードできない
        }

        if (!m_device)
        {
            return nullptr;
        }


        // キャッシュに無かったのでロードを実行

        m_inFlightKeys.insert(cacheKey);
        struct ScopedInFlightRemoval
        {
            ScopedInFlightRemoval(std::unordered_set<std::string>& keys, std::string key)
                : inFlightKeys(keys), trackedKey(std::move(key))
            {
            }

            ~ScopedInFlightRemoval()
            {
                inFlightKeys.erase(trackedKey);
            }

            std::unordered_set<std::string>& inFlightKeys;
            std::string trackedKey;
        } inFlightGuard(m_inFlightKeys, cacheKey);

        std::shared_ptr<Animation> animation = m_loader->LoadAnimation(path);

        // ロード結果の判定
        if (animation)
        {
            // 成功した場合はキャッシュに保存
            m_animations[cacheKey] = animation;
            Diagnostics::LogInfo(std::string("[ModelCache] key='") + cacheKey + "' 格納\n");
        }
        else
        {
            // 失敗した場合はエラーログ
        }

        return animation;  // ロード結果（失敗時は nullptr）
    }

    //============================================================
    // キャッシュの完全クリア
    // 登録済みのモデルをすべて破棄する
    //============================================================
    void AnimationCache::Clear()
    {
        Reset();
    }

    //============================================================
    // キャッシュ状態の完全リセット
    // モデル、ローダー、デバイス、テクスチャキャッシュへの参照を破棄する
    //============================================================
    void AnimationCache::Reset()
    {
        m_animations.clear();
        m_loader.reset();
        m_device = nullptr;
        m_inFlightKeys.clear();
    }
}
