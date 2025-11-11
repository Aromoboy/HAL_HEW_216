#pragma once

#include <cstdint>

class Scene;

namespace Core
{
        //
        // UpdatePhase はフレーム内での実行順を段階的に区切るための列挙です。
        // 入力処理 → 物理 → ゲームロジック → カメラ → UI のように、
        // 相互依存関係が少ない順番にフェーズを揃えることで、若手でも
        // 「どこで何を呼ぶべきか」を迷わず設計できます。
        //
        enum class UpdatePhase
        {
                Input,       // プレイヤー入力など、他の処理が参照する生データの取得
                PrePhysics,  // 物理演算の前処理（力の蓄積など）
                Physics,     // 実際の物理計算、コリジョン解決
                PostPhysics, // 物理結果を用いた後処理（Transform の正規化など）
                Gameplay,    // ゲームロジック全般（AI、スコア計算、演出トリガー）
                Camera,      // カメラ追従、ビュー行列の更新
                UI,          // UI の状態更新。描画は別フェーズで OK
                Late,        // 最終調整。依存関係が複雑なものはここに逃がす
        };

        struct UpdateContext
        {
                // フレーム間の経過時間（秒）。FixedUpdate のような固定間隔でも
                // 値を保持しておくと、補間やエフェクト調整に利用できます。
                float deltaTime = 0.0f;

                // ゲーム開始から何フレーム目か。リプレイ検証や乱数シードに便利です。
                std::uint64_t frameIndex = 0;

                // 呼び出し元のシーンポインタ。Scene 経由で必要なシステムにアクセスします。
                Scene* scene = nullptr;
        };

        class IUpdatable
        {
        public:
                // IUpdatable は「毎フレーム更新される」ことを表す最小インターフェースです。
                // UpdateScheduler から呼び出されることを前提に設計されています。
                virtual ~IUpdatable() = default;

                // OnUpdate は各派生クラスで実際の処理を書く場所です。
                // Context を受け取り、必要なシステム情報にアクセスできます。
                virtual void OnUpdate(const UpdateContext& ctx) = 0;

                // GetPhase / GetPriority はスケジューラが実行順を決めるために参照します。
                virtual UpdatePhase GetPhase() const = 0;
                virtual int GetPriority() const = 0;

                // 有効/無効を切り替えるためのフラグ。重たい処理でも Enable(false) で即停止可能。
                virtual bool IsEnabled() const = 0;
        };
}

