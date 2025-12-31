// 更新対象が満たすべき共通口を定義する
// 更新スケジューラがこの口を呼び出す

#pragma once



namespace Core
{
        class StepTimer;
}


namespace Core
{
        //
        // UpdatePhase はフレーム内での実行順を段階的に区切るための列挙
        // 入力処理 → 物理 → ゲームロジック → カメラ → UI のように、
        // 相互依存関係が少ない順番にフェーズを揃えることで、
        // 「どこで何を呼ぶべきか」を迷わず設計できる
        //
        enum class UpdatePhase
        {
                Input,       // プレイヤー入力など、他の処理が参照する生データの取得
                PrePhysics,  // 物理演算の前処理（力の蓄積など）
                Physics,     // 実際の物理計算、コリジョン解決
                PostPhysics, // 物理結果を用いた後処理（Transformの正規化など）
                Gameplay,    // ゲームロジック全般（スコア計算、演出トリガー）
                Camera,      // カメラ追従、ビュー行列の更新
                UI,          // UI の状態更新 描画は別フェーズでOK
                Late,        // 最終調整 依存関係が複雑なものはここに逃がす
        };

        inline const char* ToString(UpdatePhase phase)
        {
                switch (phase)
                {
                case UpdatePhase::Input:
                        return "Input";
                case UpdatePhase::PrePhysics:
                        return "PrePhysics";
                case UpdatePhase::Physics:
                        return "Physics";
                case UpdatePhase::PostPhysics:
                        return "PostPhysics";
                case UpdatePhase::Gameplay:
                        return "Gameplay";
                case UpdatePhase::Camera:
                        return "Camera";
                case UpdatePhase::UI:
                        return "UI";
                case UpdatePhase::Late:
                        return "Late";
                default:
                        return "<unknown>";
                }
        }

        struct UpdateContext
        {
                float dt;
                StepTimer& timer;

                UpdateContext(float deltaTime, StepTimer& timerRef)
                        : dt(deltaTime)
                        , timer(timerRef)
                {
                }
        };

        class IUpdatable
        {
        public:
                // IUpdatable は「毎フレーム更新される」ことを表す最小インターフェースである
                // UpdateSchedulerから呼び出されることを前提に設計されている
                virtual ~IUpdatable() = default;

                // OnUpdateは各派生クラスで実際の処理を書く場所だ
                // Contextを受け取り、必要なシステム情報にアクセスできる
                virtual void OnUpdate(const UpdateContext& ctx) = 0;

                // GetPhaseまたはGetPriorityはスケジューラが実行順を決めるために参照する
                virtual UpdatePhase GetPhase() const = 0;
                virtual int GetPriority() const = 0;

                //有効か無効を切り替えるためのフラグ 重たい処理でもEnable(false)で即停止可能
                virtual bool IsEnabled() const = 0;
        };
}
