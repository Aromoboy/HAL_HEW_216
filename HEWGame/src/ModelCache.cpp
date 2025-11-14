//ModelCache.cpp
#include "ModelCache.h"
#include "Debug.h"
#include <iostream>

namespace Render
{
    //============================================================
    // シングルトンインスタンスの取得
    // 初回呼び出し時に唯一のインスタンスを生成し、それ以降は
    // 同じインスタンスへの参照を返します
    //============================================================
    ModelCache& ModelCache::Instance()
    {
        static ModelCache instance;  
        return instance;             
    }

    //============================================================
    // モデルローダーの設定
    // 外部からモデル読み込み用のローダーオブジェクトを登録します
    //============================================================
    void ModelCache::SetLoader(std::shared_ptr<IModelLoader> loader)
    {
        m_loader = loader;  //  共有ポインタでローダーを保持
    }

    //============================================================
    // モデルの取得（キャッシュ対応）
    // 指定されたパスのモデルをキャッシュから検索し、あればそれを返します。
    // なければローダーを使ってモデルを読み込み、キャッシュに保存してから返します
    //============================================================
    std::shared_ptr<ModelDX> ModelCache::Get(const std::string& path)
    {
        // キャッシュにあるか検索
        auto it = m_models.find(path);
        if (it != m_models.end())
        {
            DebugMessage("[ModelCache] キャッシュされたモデルを発見: ");
            return it->second;  //  キャッシュが見つかったのでそれを返す
        }

        // ローダーが設定されているか確認
        if (!m_loader)
        {
            DebugMessage("[ModelCache] エラー: ローダーが設定されていません！");
            return nullptr;  //  ローダーがないと読み込みできない
        }

        // キャッシュにないので新規にロード
        DebugMessage("[ModelCache] モデルをロード中: ");
        std::shared_ptr<ModelDX> model = m_loader->Load(path);

        // ロード結果の確認
        if (model)
        {
            // 成功: キャッシュに保存しておく
            m_models[path] = model;
        }
        else
        {
            // 失敗: エラーメッセージを出力
            DebugMessage("[ModelCache] ロードに失敗しました: ");
        }

        return model;  //  ロードしたモデル（またはnullptr）を返す
    }

    //============================================================
    // キャッシュの全クリア
    // 現在保持しているすべてのモデルをキャッシュから削除します
    //============================================================
    void ModelCache::Clear()
    {
        m_models.clear();  //  マップの内容を全て消去
    }
}

//============================================================
// ModelCache 使用例
//============================================================
/*
// 前提: Render::IModelLoader の実装クラス（例: FBXLoader）が存在
//       ID3D11Device* device が作成済み

// 1. モデルキャッシュの取得（シングルトン）
Render::ModelCache& modelCache = Render::ModelCache::Instance();

// 2. ローダーの設定（必須、初回のみ）
modelCache.SetLoader(std::make_shared<FBXLoader>(device));

// 3. モデルのロード
try {
    // 初回：ファイルからロードしてキャッシュに登録
    std::shared_ptr<ModelDX> robotModel = modelCache.Get("models/robot.fbx");

    // 4. モデルの描画（取得直後や描画ループ内で）
    if (robotModel) {
        robotModel->Draw(context); // モデルクラスの描画メソッドを想定
    }

    // 5. キャッシュからの再利用
    // 同じパスで取得するとキャッシュが返される（再ロードしない）
    std::shared_ptr<ModelDX> sameModel = modelCache.Get("models/robot.fbx"); // キャッシュヒット

    // 6. 別のモデルのロード
    std::shared_ptr<ModelDX> mapModel = modelCache.Get("models/map.obj");

} catch (...) {
    std::cerr << "モデルロードエラー" << std::endl;
}

// 7. キャッシュのクリア（不要なモデル解放時）
modelCache.Clear();
*/