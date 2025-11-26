//debugのメッセージのせいでなぜかビルドが通らないのでコメントアウトされた。後で治す必要ある

// ModelCache.cpp
#include "ModelCache.h"
#include "Debug.h"
#include "TextureCache.h"
#include <iostream>

#pragma execution_character_set("utf-8")

//============================================================
// MODELCACHE_VERBOSE_LOG を定義すると、キャッシュヒットなどの
// 頻出ログが DebugMessage へ出力される。通常ビルドでは未定義のままにし、
// デバッグで詳細を追跡したいときだけプロジェクト設定やプリプロセッサ
// 定義で MODELCACHE_VERBOSE_LOG を追加して有効化する。
//============================================================
#if defined(MODELCACHE_VERBOSE_LOG)
#define MODELCACHE_VERBOSE(message) DebugMessage(std::string(u8"[モデルキャッシュ] ") + message)
#else
#define MODELCACHE_VERBOSE(message) ((void)0)
#endif

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
        m_loader = loader;            // シングルトン内にローダーを保持
        m_device = device;            // 描画デバイス
        m_textureCache = &textureCache; // テクスチャキャッシュ

        if (!m_loader)
        {
            //DebugMessage(u8"[モデルキャッシュ] ローダーが設定されていません。SetLoader に有効なローダーを渡してください。");
        }

        if (!m_device)
        {
            //DebugMessage(u8"[モデルキャッシュ] デバイスが未設定のため、モデルをロードできません。ID3D11Device* を正しく渡してください。");
        }

        if (!m_textureCache)
        {
            //DebugMessage(u8"[モデルキャッシュ] テクスチャキャッシュが未設定です。TextureCache のインスタンスを渡してください。");
        }
    }

    //============================================================
    // モデルの取得（キャッシュ対応）
    // 既にロード済みであればキャッシュから取り出し、無ければロードする
    //============================================================
    std::shared_ptr<Render::Model> ModelCache::Get(const std::string& path)
    {
        // キャッシュに存在するか確認
        auto it = m_models.find(path);
        if (it != m_models.end())
        {
            //MODELCACHE_VERBOSE(std::string(u8"キャッシュを利用します [パス] ") + path);
            return it->second;  // キャッシュ済みのモデルを返す
        }

        // ローダーが有効かどうか確認
        if (!m_loader)
        {
            //DebugMessage(std::string(u8"[モデルキャッシュ] ローダーが設定されていないためロードを中止しました [パス] ") + path);
            return nullptr;  // ローダーが無ければロードできない
        }

        if (!m_device)
        {
            //ebugMessage(std::string(u8"[モデルキャッシュ] デバイスが設定されていないためロードを中止しました [パス] ") + path);
            return nullptr;
        }

        if (!m_textureCache)
        {
            //DebugMessage(std::string(u8"[モデルキャッシュ] テクスチャキャッシュが設定されていないためロードを中止しました [パス] ") + path);
            return nullptr;
        }

        // キャッシュに無かったのでロードを実行
        //MODELCACHE_VERBOSE(std::string(u8"モデルのロードを開始します [パス] ") + path);
        std::shared_ptr<Render::Model> model = m_loader->Load(path, m_device, *m_textureCache);

        // ロード結果の判定
        if (model)
        {
            // 成功した場合はキャッシュに保存
            m_models[path] = model;
        }
        else
        {
            // 失敗した場合はエラーログ
            //DebugMessage(std::string(u8"[モデルキャッシュ] モデルのロードに失敗しました [パス] ") + path);
        }

        return model;  // ロード結果（失敗時は nullptr）
    }

    //============================================================
    // キャッシュの完全クリア
    // 登録済みのモデルをすべて破棄する
    //============================================================
    void ModelCache::Clear()
    {
        m_models.clear();  // キャッシュテーブルを初期化
    }
}

//============================================================
// ModelCache 使用例
//============================================================
/*
// 前提: Render::IModelLoader の派生（例: FBXLoader）と ID3D11Device* device がある

// 1. モデルキャッシュの取得（シングルトン）
Render::ModelCache& modelCache = Render::ModelCache::Instance();

// 2. ローダーの設定（必要な依存をまとめて登録）
modelCache.SetLoader(std::make_shared<FBXLoader>(device), device, TextureCache::Instance());

// 3. モデルをロード
try {
    // 初回アクセスでロードされ、キャッシュへ登録される
    std::shared_ptr<Render::Model> robotModel = modelCache.Get("models/robot.fbx");

    // 4. モデルの描画（取得に成功した場合）
    if (robotModel) {
        robotModel->Draw(context); // モデルを描画
    }

    // 5. キャッシュ利用
    // 同じパスで呼び出すとキャッシュされたインスタンスが返る
    std::shared_ptr<Render::Model> sameModel = modelCache.Get("models/robot.fbx");

    // 6. 別のモデルをロード
    std::shared_ptr<Render::Model> mapModel = modelCache.Get("models/map.obj");

} catch (...) {
    std::cerr << "モデルのロードに失敗" << std::endl;
}

// 7. キャッシュのクリア（アプリ終了時など）
modelCache.Clear();
*/
