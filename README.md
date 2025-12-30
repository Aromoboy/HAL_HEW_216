HAL_HEW_216

本リポジトリは、DirectX 11 ベースの 2.5D ゲーム用プロトタイプです。
更新は UpdateScheduler を中心にフェーズ駆動で行い、描画は Scene::RootDraw から別経路で実行します。

ビルド

Visual Studio: VS2022 推奨

PlatformToolset: v143

Windows SDK: 10.0

プロジェクト: HEWGame/DX22_Project.vcxproj

構成: Debug/Release × Win32/x64

NOTE: 現状、Box2D のリンクは HEWGame/ThirdParty/Box2D/lib/<Platform>/<Configuration> を参照する構成です。
ビルド済みの Box2D アーティファクトは BUILDING.md の手順に従い ThirdParty/Box2D 配下へ配置してください。

必須アセット / 必須バイナリ（削除禁止）
DirectXTex（プリビルド成果物）

本リポジトリでは、以下のプリビルド済み DirectXTex ライブラリを 常にバージョン管理下 に置きます。

HEWGame/DirectXTex/Win32/Debug

HEWGame/DirectXTex/Win32/Release

HEWGame/DirectXTex/x64/Debug

HEWGame/DirectXTex/x64/Release

これらの成果物（主に .lib、構成によっては .dll）はリンカ設定で直接参照されており、リポジトリから削除するとビルドが失敗します。
ツールチェーン更新などで再生成が必要になった場合は、DirectXTex のソリューションを 同じ構成 でビルドし直し、上記ディレクトリへ成果物を上書きしてください。差分確認のうえコミットし、常に最新のバイナリを共有できる状態を維持します。

Visual Studio ユーザー設定（.vcxproj.user）

HEWGame/DX22_Project.vcxproj.user は意図的にバージョン管理されています。
このファイルは Visual Studio 固有のユーザー設定（主にデバッグ実行設定など）を保持し、チームでの環境差分を抑える目的があります。

削除しない

gitignore しない

必要最小構成を維持する（環境依存の値が増えすぎないようにする）

ステージ設定（唯一の参照元）

ステージパラメータの 唯一の参照元（source of truth） は以下です。

HEWGame/Assets/Config/gameplay.cfg の [stage.checklist] セクション

Stage::StageData はこのセクションを読み込み、デフォルト適用・サニタイズを行ってステージ設定として保持します。

設定ローダー（概要）

ConfigLoader

ゲーム設定・コライダー設定の読み込み

主に SceneGameConfigLoader / ColliderConfigLoader / ConfigSectionReader

StageLayoutLoader

Assets/Config/StageLayout.csv からステージ配置を読み込み、ステージオブジェクト（transform 等）を生成するローダー

物理登録は Scene 側（または Binder 側）で行う方針

クラス図（Mermaid）

Mermaid 対応ビューアで表示してください。

classDiagram
direction TB

%% =========================
%% Core update contracts
%% =========================
class Core_UpdatePhase["Core::UpdatePhase"] {
  <<enumeration>>
  Input
  PrePhysics
  Physics
  PostPhysics
  Gameplay
  Camera
  UI
  Late
}

class Core_StepTimer["Core::StepTimer"] {
  +Tick()
  +DeltaTime() float
  +FrameIndex() uint64
}

class Core_UpdateContext["Core::UpdateContext"] {
  +dt: float
  +time: Core_StepTimer&
  +input: Core_Input&
}

class Core_IUpdatable["Core::IUpdatable"] {
  <<interface>>
  +OnUpdate(ctx: Core_UpdateContext)
  +GetPhase() Core_UpdatePhase
  +GetPriority() int
  +IsEnabled() bool
}

class Core_UpdateNode["Core::UpdateNode"] {
  -phase: Core_UpdatePhase
  -priority: int
  -enabled: bool
  +UpdateNode(phase: Core_UpdatePhase = Gameplay, priority: int = 0)
  +OnUpdate(ctx: Core_UpdateContext)
  +GetPhase() Core_UpdatePhase
  +GetPriority() int
  +Enable(v: bool)
  +SetPhase(ph: Core_UpdatePhase)
  +SetPriority(p: int)
}

class Core_UpdateScheduler["Core::UpdateScheduler"] {
  -nodes: Core_IUpdatable[*]
  +Register(node: Core_IUpdatable)
  +Unregister(node: Core_IUpdatable)
  +Tick(ctx: Core_UpdateContext)
}

class Core_Input["Core::Input"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +GetAxis(actionId) float
  +IsAction(actionId) bool
  +IsPress(vk:int) bool
  +IsTrigger(vk:int) bool
  +IsRelease(vk:int) bool
}

Core_UpdateNode ..|> Core_IUpdatable
Core_Input --|> Core_UpdateNode
Core_UpdateScheduler o-- Core_IUpdatable

%% =========================
%% App / Main loop
%% =========================
class App_Main["App::Main"] {
  +Init()
  +Update()
  +Draw()
  +Uninit()
}

%% =========================
%% Scene framework
%% =========================
class Scene_IScene["SceneSpace::IScene"] {
  <<interface>>
  +Load()
  +OnEnter()
  +OnExit()
  +Update()
  +Draw()
}

class Scene_Scene["SceneSpace::Scene"] {
  -scheduler: Core_UpdateScheduler
  -input: Core_Input&
  +RootUpdate()
  +RootDraw()
  +ConfigureUpdates()
  +Register(node: Core_IUpdatable)
  +Unregister(node: Core_IUpdatable)
  +RegisterSceneUpdateNode(phase: Core_UpdatePhase, priority:int)
  +RegisterCameraUpdateNode(phase: Core_UpdatePhase, priority:int)
}

class Scene_SceneUpdateNode["SceneSpace::SceneUpdateNode"] {
  +OnUpdate(ctx: Core_UpdateContext)
}

class Scene_CameraUpdateNode["SceneSpace::CameraUpdateNode"] {
  +OnUpdate(ctx: Core_UpdateContext)
}

class Scene_SceneManager["SceneSpace::SceneManager"] {
  -timer: Core_StepTimer
  -active: Scene_IScene
  -pending: Scene_IScene
  +SetInitialScene(scene: Scene_IScene)
  +RequestSceneChange(scene: Scene_IScene)
  +Update()
  +Draw()
}

Scene_Scene --|> Scene_IScene
Scene_Scene *-- Core_UpdateScheduler
Scene_Scene o-- Core_Input
Scene_SceneUpdateNode --|> Core_UpdateNode
Scene_CameraUpdateNode --|> Core_UpdateNode
Scene_Scene *-- Scene_SceneUpdateNode
Scene_Scene *-- Scene_CameraUpdateNode

Scene_SceneManager *-- Scene_IScene
Scene_SceneManager o-- Core_StepTimer

App_Main o-- Scene_SceneManager
App_Main o-- Core_Input

class Scene_TitleScene["SceneSpace::TitleScene"]
class Scene_SelectScene["SceneSpace::SelectScene"]
class Scene_SettingScene["SceneSpace::SettingScene"]
class Scene_ScenePowerUp["SceneSpace::ScenePowerUp"]
class Scene_SceneGame["SceneSpace::SceneGame"]

Scene_TitleScene --|> Scene_Scene
Scene_SelectScene --|> Scene_Scene
Scene_SettingScene --|> Scene_Scene
Scene_ScenePowerUp --|> Scene_Scene
Scene_SceneGame --|> Scene_Scene

%% =========================
%% Composition
%% =========================
class Build_SceneComposer["SceneBuild::SceneComposer"] {
  +ComposeSceneGame()
}

Build_SceneComposer ..> Scene_SceneGame : composes

%% =========================
%% Systems: physics + contacts + draw
%% =========================
class Sys_PhysicsMachine["Systems::PhysicsMachine"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +RegisterBody(entity)
  +UnregisterBody(entity)
  +CreateWeldJoint(a,b)
  +DestroyWeldJoint(jointId)
}

class Sys_DrawSystem["Systems::DrawSystem"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +SetRenderer(renderer)
}

class Sys_Box2DPhysicsSystem["Systems::Box2DPhysicsSystem"] {
  +Apply(dt: float)
}

class Sys_Box2DContactListener["Systems::Box2DContactListener"] {
  +ProcessContacts()
  +SetBeginContactCallback(cb)
  +SetEndContactCallback(cb)
}

class Sys_ContactHandler["Systems::ContactHandler"] {
  <<interface>>
  +OnBegin(a,b)
  +OnEnd(a,b)
}

class Sys_PhysicsRegistration["Systems::PhysicsRegistration"] {
  +DispatchBeginContact(a,b)
  +DispatchEndContact(a,b)
  +AddHandler(h: Sys_ContactHandler)
}

class Sys_IPhysicsBinder["Systems::IPhysicsBinder"] {
  <<interface>>
  +GetPhysicsMachine() Sys_PhysicsMachine
  +RegisterPhysicsBody(entity)
  +UnregisterPhysicsBody(entity)
  +AddContactHandler(h: Sys_ContactHandler)
}

class Sys_PhysicsBinder["Systems::PhysicsBinder"] {
  +GetPhysicsMachine() Sys_PhysicsMachine
  +RegisterPhysicsBody(entity)
  +UnregisterPhysicsBody(entity)
  +AddContactHandler(h: Sys_ContactHandler)
}

Sys_PhysicsMachine --|> Core_UpdateNode
Sys_DrawSystem --|> Core_UpdateNode
Sys_PhysicsBinder ..|> Sys_IPhysicsBinder

Sys_PhysicsBinder *-- Sys_PhysicsMachine
Sys_PhysicsBinder *-- Sys_PhysicsRegistration
Sys_PhysicsRegistration o-- Sys_ContactHandler

Sys_PhysicsMachine ..> Sys_Box2DPhysicsSystem : steps world
Sys_Box2DPhysicsSystem ..> Sys_Box2DContactListener : calls ProcessContacts
Sys_Box2DContactListener ..> Sys_PhysicsRegistration : dispatch callbacks

%% =========================
%% Camera
%% =========================
class Cam_ICamera["CameraSpace::ICamera"] {
  <<interface>>
  +GetView()
  +GetProjection()
}

class Cam_Camera["CameraSpace::Camera"] {
  +GetView()
  +GetProjection()
}

class Cam_CameraRig["CameraSpace::CameraRig"] {
  +Attach(target)
  +OnUpdate(ctx: Core_UpdateContext)
}

Cam_Camera ..|> Cam_ICamera
Cam_CameraRig --|> Core_UpdateNode

%% =========================
%% Entities
%% =========================
class Ent_Entity["Entities::Entity"] {
  +OnUpdate(ctx: Core_UpdateContext)
}

class Ent_PhysicalEntity["Entities::PhysicalEntity"] {
  +GetTransform()
  +GetPhysical()
}

class Ent_Player["Entities::Player"] {
  +HandleInput(input: Core_Input)
  +OnUpdate(ctx: Core_UpdateContext)
}

Ent_Entity --|> Core_UpdateNode
Ent_PhysicalEntity --|> Ent_Entity
Ent_Player --|> Ent_PhysicalEntity

%% =========================
%% Sweets (post-fix: no manual bypass)
%% =========================
class Sweets_State["SweetsSpace::SweetsState"] {
  <<enumeration>>
  Active
  Stacked
  Ground
  Carried
}

class Sweets_Base["SweetsSpace::BaseSweets"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +DrawAll()
}

class Sweets_LateNode["SweetsSpace::LateUpdateNode"] {
  +OnUpdate(ctx: Core_UpdateContext)
}

class Sweets_Manager["SweetsSpace::SweetsManager"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +OnLateUpdate(ctx: Core_UpdateContext)
  +DrawAll()
  +Spawn(kind, transform)
  +LoadPhaseSpawnConfig(path)
  +OnContactBegin(a,b)
  +OnContactEnd(a,b)
  +GetLateUpdateNode() Sweets_LateNode
}

Sweets_Base --|> Core_UpdateNode
Sweets_LateNode --|> Core_UpdateNode
Sweets_Manager --|> Core_UpdateNode

Sweets_Manager *-- Sweets_LateNode
Sweets_Manager o-- Sweets_Base : owns instances
Sweets_Manager ..> Core_UpdateScheduler : registers BaseSweets
Sweets_Manager ..> Sys_PhysicsMachine : joints and physics ops

%% =========================
%% Stage + Config
%% =========================
class Cfg_SectionReader["Config::ConfigSectionReader"] {
  +ReadSectionLines(path, section)
}

class Cfg_SceneGameConfigLoader["Config::SceneGameConfigLoader"] {
  +LoadPhysicsTuningSettings(path)
  +LoadPlayerTransformConfig(path)
  +LoadPlayerColliderTuning(path)
  +LoadStageColliderConfig(path)
  +LoadModelTextureLoadingSetting(path)
}

class Stage_StageData["Stage::StageData"] {
  +LoadFromFile(path, section)
  +GetGuidelineHeight() float
  +GetBackgroundTexture() string
}

class Stage_LayoutLoader["Stage::StageLayoutLoader"] {
  +LoadFromCsv(path)
}

Stage_StageData ..> Cfg_SectionReader : reads stage checklist
Cfg_SceneGameConfigLoader ..> Cfg_SectionReader
Stage_LayoutLoader ..> Cfg_SectionReader

Sweets_Manager ..> Cfg_SectionReader : reads spawn phases
Sweets_Manager ..> Stage_StageData : uses stage params

%% =========================
%% UI
%% =========================
class UI_UIManager["UI::UIManager"] {
  +Initialize()
  +Shutdown()
  +UpdateHUD()
  +Draw()
}

class UI_HudRenderer["UI::HudRenderer"] {
  +Initialize()
  +Shutdown()
  +Draw()
}

class UI_ResultRenderer["UI::ResultRenderer"] {
  +Initialize()
  +Shutdown()
  +Draw()
}

class UI_Overlay["UI::SceneGameUIOverlay"] {
  +Initialize()
  +Shutdown()
  +Update()
  +Draw()
}

UI_UIManager *-- UI_HudRenderer
UI_UIManager *-- UI_ResultRenderer
UI_Overlay *-- UI_UIManager

%% =========================
%% PowerUp scene
%% =========================
class PU_Manager["PowerUp::PowerUpManager"] {
  +OnUpdate(ctx: Core_UpdateContext)
}

class PU_Cursor["PowerUp::PowerUpCursor"] {
  +OnUpdate(ctx: Core_UpdateContext)
}

class PU_Node["PowerUp::PowerUpNode"] {
  +OnUpdate(ctx: Core_UpdateContext)
}

PU_Manager --|> Core_UpdateNode
PU_Cursor --|> Core_UpdateNode
PU_Node --|> Core_UpdateNode

Scene_ScenePowerUp *-- PU_Manager
Scene_ScenePowerUp *-- PU_Cursor
PU_Manager *-- PU_Node

%% =========================
%% SceneGame wiring (ownership + registration + contacts)
%% =========================
Scene_SceneGame *-- Sys_PhysicsBinder
Scene_SceneGame *-- Sys_DrawSystem
Scene_SceneGame *-- Sweets_Manager
Scene_SceneGame *-- UI_UIManager
Scene_SceneGame *-- Cam_CameraRig
Scene_SceneGame *-- Ent_Player
Scene_SceneGame *-- Stage_StageData

Scene_SceneGame ..> Core_UpdateScheduler : registers nodes
Scene_SceneGame ..> Sys_PhysicsBinder : registers contact handler
Scene_SceneGame ..> Sweets_Manager : forwards contacts

クラス図ざっくりまとめ
Core

DirectX まわり。GitHub リポジトリに入ってる初期化コードをそのままラップしてるだけ。
DXContext・Input・Shader/TextureCache あたりはいじらなくて OK（必要が出るまで触らない）。

Components

オブジェクトの基礎データ。
Transform（位置/回転/スケール）、Physical（速度/力/減衰）、2D/Box2D 系のコライダー（当たり判定）だけで十分。

CameraSpace

カメラは基本 1 個。priority とかは今は使わない。
3D の奥行きはあるけど、プレイ自体は横方向（2.5D）固定。

Render

モデル描画まわり。描画は “一発で描ける” 流儀（Renderer/DrawSystem 経由）。
ローダーは仮で OK（立方体などの代用でも成立）。

Systems

物理系。基本は重力 + 減衰。
接地感は余裕があったら別システムで追加する。
PhysicsBinder は PhysicsMachine と登録処理・コンタクト配信を束ねて Scene から扱いやすくする薄い仲介。

GameFlow

フェーズ管理と強化メニュー。
一定の高さを超えたらフェーズ進行 → 終わったら強化選択（まだ確定していないので例）。
ここが今回の新要素。

ConfigLoader

ゲーム設定・コライダー設定の読み込み。
SceneGameConfigLoader / ColliderConfigLoader / ConfigSectionReader で実装。

StageLayoutLoader

CSV からステージ配置を読み込み、ステージオブジェクト（Transform 等）を生成するローダー。

UIManager

HUD とリザルト UI のレイアウト初期化・更新・描画を一元管理。

Scene / Entities / SweetsSpace

SceneGame に統合されている領域。
プレイヤー（横移動＋回転＋落下 など）とスイーツの管理。
フェーズごとに難易度（出現間隔など）を調整。

App

全部まとめて動かすメイン。
Initialize → Update → Draw で完結。

UpdateNode

更新は 単一路線で進む。

SceneManager::Update → 各シーンの Update → Scene::RootUpdate → UpdateScheduler::Tick → 登録済み IUpdatable::OnUpdate

更新が必要なクラスは Core::UpdateNode を継承し、フェーズと priority を設定して Scene に登録する。
順序を変えたい時はフェーズ/priority を調整するか Enable(false) で無効化するだけ。

標準フェーズ順:
Input → PrePhysics → Physics → PostPhysics → Gameplay → Camera → UI → Late

描画は Scene::RootDraw から別経路で行う。
SceneGame::ConfigureUpdates の登録と実行順の説明は docs/scene-lifecycle.md にまとめてある。
