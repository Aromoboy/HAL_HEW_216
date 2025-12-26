#include "AnimationCache.h"
#include "Debug.h"
#include "TextureCache.h"
#include "Diagnostics/Logger.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

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

    std::string BuildAnimationCacheKey(const std::string& rawPath, std::optional<bool> loopOverride)
    {
        std::string key = CanonicalizeCacheKey(rawPath);
        if (loopOverride.has_value())
        {
            key += loopOverride.value() ? "|loop" : "|oneshot";
        }
        return key;
    }
}

namespace Render
{
    //============================================================
    // モデルキャッシュの取得
    // シングルトンとして保持しているインスタンスを返す
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
    void AnimationCache::SetLoader(std::shared_ptr<IModelLoader> loader, ID3D11Device* device)
    {
        const bool loaderChanged = m_loader && m_loader != loader;
        const bool deviceChanged = m_device && m_device != device;
        
        const bool reconfiguringWithCachedModels = (!m_animations.empty()) && (loaderChanged || deviceChanged);

        if (reconfiguringWithCachedModels)
        {
            //MODELCACHE_LOG(
    std::shared_ptr<Animation> AnimationCache::Get(const std::string& path, std::optional<bool> loopOverride)
        const std::string cacheKey = BuildAnimationCacheKey(path, loopOverride);
        }

        m_loader = loader;              // シングルトン内にローダーを保持
        m_device = device;              // 描画デバイス
    

        if (!m_loader)
        {
            //MODELCACHE_LOG("ローダーが未設定 SetLoader に有効なローダーを渡すこと");
        }

        if (!m_device)
        {
            //MODELCACHE_LOG("デバイスが未設定のためモデルをロードできない ID3D11Device* を正しく渡すこと");
        }


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
            //MODELCACHE_VERBOSE(std::string("キャッシュを利用します [パス] ") + path + "\n");
            return it->second;  // キャッシュ済みのモデルを返す
        }

        if (m_inFlightKeys.find(cacheKey) != m_inFlightKeys.end())
        {
            Diagnostics::LogInfo(std::string("[ModelCache] key='") + cacheKey + "' ブロック: ロードが進行中\n");
            //MODELCACHE_LOG(std::string("ロードをスキップする（現在ロード中のキーへの再入禁止） [パス] ") + path + "\n");
            return nullptr;
        }

        Diagnostics::LogInfo(std::string("[ModelCache] key='") + cacheKey + "' ミス -> ロード\n");

        // ローダーが有効かどうか確認
        if (!m_loader)
        {
            //MODELCACHE_LOG(std::string("ローダーが未設定のためロードを中止 [パス] ") + path + "\n");
            return nullptr;  // ローダーが無ければロードできない
        }

        if (!m_device)
        {
        std::shared_ptr<Animation> animation = m_loader->LoadAnimation(path, loopOverride);
            return nullptr;
        }


        // キャッシュに無かったのでロードを実行
        //MODELCACHE_VERBOSE(std::string("モデルのロードを開始します [パス] ") + path + "\n");

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
            //MODELCACHE_LOG(std::string("モデルのロードに失敗 [パス] ") + path + "\n");
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
