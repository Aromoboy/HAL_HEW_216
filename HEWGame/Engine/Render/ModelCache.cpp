#include "ModelCache.h"
#include "Debug.h"
#include "TextureCache.h"
#include "Diagnostics/Logger.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>

#pragma execution_character_set("utf-8")

#ifndef MODELCACHE_ENABLE_LOG
#define MODELCACHE_ENABLE_LOG 1
#endif

//============================================================
// MODELCACHE_VERBOSE_LOG を定義するとキャッシュヒットなどの
// 頻出ログを DebugMessage へ出力する 通常ビルドでは未定義のままにし
// デバッグで詳細を追跡したいときだけプロジェクト設定やプリプロセッサ
// 定義で MODELCACHE_VERBOSE_LOG を追加して有効化する
//============================================================
#if MODELCACHE_ENABLE_LOG && defined(MODELCACHE_VERBOSE_LOG)
#define MODELCACHE_VERBOSE(message) DebugLog::DebugMessage(std::string(u8"[モデルキャッシュ] ") + message)
#else
#define MODELCACHE_VERBOSE(message) ((void)0)
#endif

#if MODELCACHE_ENABLE_LOG
#define MODELCACHE_LOG(message) DebugLog::DebugMessage(std::string("[モデルキャッシュ] ") + message)
#else
#define MODELCACHE_LOG(message) ((void)0)
#endif

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
    // モデルキャッシュの取得
    // シングルトンとして保持しているインスタンスを返す
    //============================================================
    ModelCache& ModelCache::Instance()
    {
        static ModelCache instance;
        return instance;
    }

    //============================================================
    // ローダーの設定
    // 実際のロード処理を担う IModelLoader と必要な依存を登録する
    //============================================================
    void ModelCache::SetLoader(std::unique_ptr<IModelLoader> loader, ID3D11Device* device, TextureCache& textureCache)
    {
        const bool loaderChanged = m_loader && m_loader.get() != loader.get();
        const bool deviceChanged = m_device && m_device != device;
        const bool textureCacheChanged = m_textureCache && m_textureCache != &textureCache;
        const bool reconfiguringWithCachedModels = (!m_models.empty()) && (loaderChanged || deviceChanged || textureCacheChanged);

        if (reconfiguringWithCachedModels)
        {
            MODELCACHE_LOG(
                "ローダー/デバイスの切り替え前にキャッシュをクリアする (Clear が呼ばれていない)");
            Clear();
        }

        m_loader = std::move(loader);   // シングルトン内にローダーを保持
        m_device = device;              // 描画デバイス
        m_textureCache = &textureCache; // テクスチャキャッシュ

        if (!m_loader)
        {
            MODELCACHE_LOG("ローダーが未設定 SetLoader に有効なローダーを渡すこと");
        }

        if (!m_device)
        {
            MODELCACHE_LOG("デバイスが未設定のためモデルをロードできない ID3D11Device* を正しく渡すこと");
        }

        if (!m_textureCache)
        {
            MODELCACHE_LOG("テクスチャキャッシュが未設定 TextureCache のインスタンスを渡すこと\n");
        }
    }

    bool ModelCache::IsConfiguredFor(ID3D11Device* device, const TextureCache& textureCache) const noexcept
    {
        return m_loader && m_device == device && m_textureCache == &textureCache;
    }

    //============================================================
    // モデルの取得（キャッシュ対応）
    // 既にロード済みであればキャッシュから取り出し、無ければロードする
    //============================================================
    std::shared_ptr<Render::Model> ModelCache::Get(const std::string& path)
    {
        const std::string cacheKey = CanonicalizeCacheKey(path);

        Diagnostics::LogInfo(
            std::string("[ModelCache] リクエスト='") + path + "' -> key='" + cacheKey + "'\n");

        // キャッシュに存在するか確認
        auto it = m_models.find(cacheKey);
        if (it != m_models.end())
        {
            Diagnostics::LogInfo(std::string("[ModelCache] key='") + cacheKey + "' ヒット\n");
            MODELCACHE_VERBOSE(std::string("キャッシュを利用します [パス] ") + path + "\n");
            return it->second;  // キャッシュ済みのモデルを返す
        }

        if (m_inFlightKeys.find(cacheKey) != m_inFlightKeys.end())
        {
            Diagnostics::LogInfo(std::string("[ModelCache] key='") + cacheKey + "' ブロック: ロードが進行中\n");
            MODELCACHE_LOG(std::string("ロードをスキップする（現在ロード中のキーへの再入禁止） [パス] ") + path + "\n");
            return nullptr;
        }

        Diagnostics::LogInfo(std::string("[ModelCache] key='") + cacheKey + "' ミス -> ロード\n");

        // ローダーが有効かどうか確認
        if (!m_loader)
        {
            MODELCACHE_LOG(std::string("ローダーが未設定のためロードを中止 [パス] ") + path + "\n");
            return nullptr;  // ローダーが無ければロードできない
        }

        if (!m_device)
        {
            MODELCACHE_LOG(std::string("デバイスが未設定のためロードを中止 [パス] ") + path + "\n");
            return nullptr;
        }

        if (!m_textureCache)
        {
            MODELCACHE_LOG(std::string("テクスチャキャッシュが未設定のためロードを中止 [パス] ") + path + "\n");
            return nullptr;
        }

        // キャッシュに無かったのでロードを実行
        MODELCACHE_VERBOSE(std::string("モデルのロードを開始します [パス] ") + path + "\n");

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

        std::shared_ptr<Render::Model> model = m_loader->Load(path, m_device, *m_textureCache);

        // ロード結果の判定
        if (model)
        {
            // 成功した場合はキャッシュに保存
            m_models[cacheKey] = model;
            Diagnostics::LogInfo(std::string("[ModelCache] key='") + cacheKey + "' 格納\n");
        }
        else
        {
            // 失敗した場合はエラーログ
            MODELCACHE_LOG(std::string("モデルのロードに失敗 [パス] ") + path + "\n");
        }

        return model;  // ロード結果（失敗時は nullptr）
    }

    //============================================================
    // キャッシュの完全クリア
    // 登録済みのモデルをすべて破棄する
    //============================================================
    void ModelCache::ClearModels()
    {
        m_models.clear();
        m_inFlightKeys.clear();
    }

    //============================================================
    // キャッシュの完全クリア
    // 登録済みのモデルをすべて破棄する
    //============================================================
    void ModelCache::Clear()
    {
        Reset();
    }

    //============================================================
    // キャッシュ状態の完全リセット
    // モデル、ローダー、デバイス、テクスチャキャッシュへの参照を破棄する
    //============================================================
    void ModelCache::Reset()
    {
        m_models.clear();
        m_loader.reset();
        m_device = nullptr;
        m_textureCache = nullptr;
        m_inFlightKeys.clear();
        m_textureLoadingEnabled = true;
    }
}
