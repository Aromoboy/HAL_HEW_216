#include "ModelCache.h"
#include "Debug.h"
#include "TextureCache.h"
#include "Diagnostics/Logger.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#pragma execution_character_set("utf-8")

#ifndef MODELCACHE_ENABLE_LOG
#define MODELCACHE_ENABLE_LOG 1
#endif

//============================================================
// MODELCACHE_VERBOSE_LOG を定義すると、キャッシュヒットなどの
// 頻出ログが DebugMessage へ出力される。通常ビルドでは未定義のままにし、
// デバッグで詳細を追跡したいときだけプロジェクト設定やプリプロセッサ
// 定義で MODELCACHE_VERBOSE_LOG を追加して有効化する。
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
    void ModelCache::SetLoader(std::shared_ptr<IModelLoader> loader, ID3D11Device* device, TextureCache& textureCache)
    {
        const bool loaderChanged = m_loader && m_loader != loader;
        const bool deviceChanged = m_device && m_device != device;
        const bool textureCacheChanged = m_textureCache && m_textureCache != &textureCache;
        const bool reconfiguringWithCachedModels = (!m_models.empty()) && (loaderChanged || deviceChanged || textureCacheChanged);

        if (reconfiguringWithCachedModels)
        {
            MODELCACHE_LOG("ローダー/デバイスの切り替え前にキャッシュをクリアします (Clear が呼ばれていません)。");
            Clear();
        }

        m_loader = loader;              // シングルトン内にローダーを保持
        m_device = device;              // 描画デバイス
        m_textureCache = &textureCache; // テクスチャキャッシュ

        if (!m_loader)
        {
            MODELCACHE_LOG("ローダーが設定されていません。SetLoader に有効なローダーを渡してください");
        }

        if (!m_device)
        {
            MODELCACHE_LOG("デバイスが未設定のため、モデルをロードできません。ID3D11Device* を正しく渡してください");
        }

        if (!m_textureCache)
        {
            MODELCACHE_LOG("テクスチャキャッシュが未設定です。TextureCache のインスタンスを渡してください");
        }
    }

    //============================================================
    // モデルの取得（キャッシュ対応）
    // 既にロード済みであればキャッシュから取り出し、無ければロードする
    //============================================================
    std::shared_ptr<Render::Model> ModelCache::Get(const std::string& path)
    {
        const std::string cacheKey = CanonicalizeCacheKey(path);

        Diagnostics::LogInfo(std::string("[ModelCache] request='") + path + "' -> key='" + cacheKey + "'");

        // キャッシュに存在するか確認
        auto it = m_models.find(cacheKey);
        if (it != m_models.end())
        {
            Diagnostics::LogInfo(std::string("[ModelCache] key='") + cacheKey + "' hit");
            MODELCACHE_VERBOSE(std::string("キャッシュを利用します [パス] ") + path);
            return it->second;  // キャッシュ済みのモデルを返す
        }

        Diagnostics::LogInfo(std::string("[ModelCache] key='") + cacheKey + "' miss -> load");

        // ローダーが有効かどうか確認
        if (!m_loader)
        {
            MODELCACHE_LOG(std::string("ローダーが設定されていないためロードを中止しました [パス] ") + path);
            return nullptr;  // ローダーが無ければロードできない
        }

        if (!m_device)
        {
            MODELCACHE_LOG(std::string("デバイスが設定されていないためロードを中止しました [パス] ") + path);
            return nullptr;
        }

        if (!m_textureCache)
        {
            MODELCACHE_LOG(std::string("テクスチャキャッシュが設定されていないためロードを中止しました [パス] ") + path);
            return nullptr;
        }

        // キャッシュに無かったのでロードを実行
        MODELCACHE_VERBOSE(std::string("モデルのロードを開始します [パス] ") + path);
        std::shared_ptr<Render::Model> model = m_loader->Load(path, m_device, *m_textureCache);

        // ロード結果の判定
        if (model)
        {
            // 成功した場合はキャッシュに保存
            m_models[cacheKey] = model;
            Diagnostics::LogInfo(std::string("[ModelCache] key='") + cacheKey + "' stored");
        }
        else
        {
            // 失敗した場合はエラーログ
            MODELCACHE_LOG(std::string("モデルのロードに失敗しました [パス] ") + path);
        }

        return model;  // ロード結果（失敗時は nullptr）
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
    }
}
