HAL_HEW_216

本リポジトリは、DirectX 11 ベースの 2.5D ゲーム用プロトタイプです。
更新は UpdateScheduler を中心にフェーズ駆動で行い、描画は Scene::RootDraw から別経路で実行します。

ビルド

Visual Studio: VS2022 推奨

PlatformToolset: v143

Windows SDK: 10.0

プロジェクト: HEWGame/DX22_Project.vcxproj

構成: Debug/Release × Win32/x64

NOTE: 現状、Box2D のリンクは HEWGame/box2d/build/... をライブラリ探索パスとして参照している構成があります。
box2d/build を削除する場合は、先に依存の “正規パッケージ化（repack）” を行ってからにしてください。

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

ステージパラメータの 唯一の参照元（単一の基準） は以下です。

HEWGame/Assets/Config/gameplay.cfg の [stage.checklist] セクション

Stage::StageData はこのセクションを読み込み、デフォルト適用・サニタイズを行ってステージ設定として保持します。

設定ローダー（概要）

ConfigLoader

ゲーム設定・コライダー設定の読み込み

基本コライダーは gameplay.cfg の [colliders.registry] を ColliderRegistry が読み込み、ステージ配置やスイーツ生成などで共通利用する
（ColliderConfigLoader はコライダー定義のパース用ユーティリティ）

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
  +Tick() float
  +Tick(overrideDt: float) float
  +DeltaTime() float
  +FrameIndex() uint64
  +SetFixedStep(dt: float, enable: bool)
}

class Core_UpdateContext["Core::UpdateContext"] {
  +dt: float
  +timer: Core_StepTimer&
}

class Core_IUpdatable["Core::IUpdatable"] {
  <<interface>>
  +OnUpdate(ctx: Core_UpdateContext)
  +GetPhase() Core_UpdatePhase
  +GetPriority() int
  +IsEnabled() bool
}

class Core_RegistrationHandle["Core::RegistrationHandle"] {
  +IsValid() bool
  +Reset()
  +Release()
}

class Core_UpdateNode["Core::UpdateNode"] {
  +UpdateNode(phase: Core_UpdatePhase = Gameplay, priority: int = 0)
  +SetPhase(ph: Core_UpdatePhase)
  +SetPriority(p: int)
  +Enable(v: bool)
  +SetRegistrationHandle(handle: Core_RegistrationHandle)
  +ReleaseRegistrationHandle()
  +GetPhase() Core_UpdatePhase
  +GetPriority() int
  +IsEnabled() bool
}

class Core_UpdateScheduler["Core::UpdateScheduler"] {
  +Register(node: Core_IUpdatable) Core_RegistrationHandle
  +Unregister(node: Core_IUpdatable)
  +SetOrder(node: Core_IUpdatable, phase: Core_UpdatePhase, priority: int)
  +AddDependency(before: Core_IUpdatable, after: Core_IUpdatable)
  +ClearDependencies()
  +Tick(ctx: Core_UpdateContext)
  +SetExecutionLogListener(listener)
}

class Core_Input["Core::Input"] {
  +Initialize()
  +Shutdown()
  +OnUpdate(ctx: Core_UpdateContext)
  +GetAxis(actionId) float
  +IsAction(actionId) bool
  +IsPress(vk:int) bool
  +IsTrigger(vk:int) bool
  +IsRelease(vk:int) bool
}

Core_UpdateNode ..|> Core_IUpdatable
Core_Input --|> Core_UpdateNode
Core_UpdateNode *-- Core_RegistrationHandle
Core_UpdateScheduler o-- Core_IUpdatable
Core_UpdateScheduler ..> Core_RegistrationHandle

%% =========================
%% App / Entry
%% =========================
class App_Game["App::Game"] {
  +Instance() App::Game&
  +GetDXContext() Core::DXContext&
  +GetShaderCache() ShaderCache&
  +GetTextureCache() TextureCache&
  +GetModelRenderer() Render::ModelRenderer&
}

class App_Main["Main (Main.cpp)"] {
  +Init(hWnd, width, height)
  +Update()
  +Draw()
  +Uninit()
}

App_Main o-- Core_Input
App_Main o-- Core_StepTimer
App_Main o-- Scene_SceneManager

%% =========================
%% Scene framework
%% =========================
class Scene_IScene["SceneSpace::IScene"] {
  <<interface>>
  +Load(input: Core_Input) bool
  +Update(timer: Core_StepTimer, input: Core_Input)
  +Draw()
  +OnEnter()
  +OnExit()
}

class Scene_ISceneServices["SceneSpace::ISceneServices"] {
  <<interface>>
  +GetPhysicsBinder() Systems::IPhysicsBinder*
  +GetRenderer() Render::ModelRenderer*
  +GetScheduler() Core_UpdateScheduler&
  +GetInput() Core_Input*
  +GetTimer() Core_StepTimer*
}

class Scene_Scene["SceneSpace::Scene"] {
  +RootUpdate(timer: Core_StepTimer, input: Core_Input)
  +RootDraw()
  +ConfigureUpdates()
  +Register(node: Core_IUpdatable)
  +Unregister(node: Core_IUpdatable)
  +RegisterSceneUpdateNode(phase: Core_UpdatePhase, priority: int)
  +RegisterCameraUpdateNode(phase: Core_UpdatePhase, priority: int)
  +GetUpdateScheduler() Core_UpdateScheduler&
  +GetInput() Core_Input*
  +GetTimer() Core_StepTimer*
  +GetCameraManager() Camera_CameraManager&
  +AddCamera(camera)
  +SetActiveCamera(camera)
  +GetActiveCamera()
}

class Scene_SceneUpdateNode["SceneSpace::Scene::SceneUpdateNode"] {
  +OnUpdate(ctx: Core_UpdateContext)
}

class Scene_CameraUpdateNode["SceneSpace::Scene::CameraUpdateNode"] {
  +OnUpdate(ctx: Core_UpdateContext)
}

class Scene_SceneManager["SceneSpace::SceneManager"] {
  +SetInitialScene(scene: Scene_IScene)
  +RequestSceneChange(scene: Scene_IScene)
  +Update()
  +Draw()
  +GetActiveScene()
}

Scene_SceneUpdateNode --|> Core_UpdateNode
Scene_CameraUpdateNode --|> Core_UpdateNode
Scene_Scene *-- Core_UpdateScheduler
Scene_Scene o-- Core_Input
Scene_Scene o-- Core_StepTimer
Scene_Scene *-- Scene_SceneUpdateNode
Scene_Scene *-- Scene_CameraUpdateNode
Scene_Scene *-- Camera_CameraManager

Scene_SceneManager *-- Scene_IScene
Scene_SceneManager o-- Core_StepTimer
Scene_SceneManager o-- Core_Input

class Scene_TitleScene["TitleScene"]
class Scene_SelectScene["SelectScene"]
class Scene_SettingScene["SceneSpace::SettingScene"]
class Scene_ScenePowerUp["SceneSpace::ScenePowerUp"]
class Scene_SceneGame["SceneGame"]

Scene_TitleScene --|> Scene_Scene
Scene_TitleScene ..|> Scene_IScene
Scene_SelectScene --|> Scene_Scene
Scene_SelectScene ..|> Scene_IScene
Scene_SettingScene --|> Scene_Scene
Scene_SettingScene ..|> Scene_IScene
Scene_ScenePowerUp --|> Scene_Scene
Scene_ScenePowerUp ..|> Scene_IScene
Scene_SceneGame --|> Scene_Scene
Scene_SceneGame ..|> Scene_IScene
Scene_SceneGame ..|> Scene_ISceneServices

%% =========================
%% Composition
%% =========================
class Scene_Composer["SceneComposer"] {
  +ComposeBootstrap()
  +Compose(result)
}

Scene_Composer ..> Scene_SceneGame : composes

%% =========================
%% Systems: physics + contacts + draw
%% =========================
class Sys_PhysicsMachine["Systems::PhysicsMachine"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +RegisterBody(entity)
  +UnregisterBody(entity)
  +CreateWeldJoint(a,b)
  +DestroyJoint(jointId)
}

class Sys_DrawSystem["Systems::DrawSystem"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +SetRenderer(renderer: ModelRender)
}

class Sys_IPhysicsSystem["Systems::IPhysicsSystem"] {
  <<interface>>
  +Apply(dt: float)
}

class Sys_Box2DPhysicsSystem["Systems::Box2DPhysicsSystem"] {
  +Apply(dt: float)
  +OnUpdate(ctx: Core_UpdateContext)
}

class Sys_Box2DContactListener["Systems::Box2DContactListener"] {
  +ProcessContacts()
  +SetBeginContactCallback(cb)
  +SetEndContactCallback(cb)
}

class Sys_IPhysicsBinder["Systems::IPhysicsBinder"] {
  <<interface>>
  +GetPhysicsMachine() Systems::PhysicsMachine*
  +RegisterPhysicsBody(entity)
  +UnregisterPhysicsBody(entity)
  +AddContactHandler(onBegin, onEnd)
}

class Sys_PhysicsBinder["Systems::PhysicsBinder"] {
  +GetPhysicsMachine() Systems::PhysicsMachine*
  +RegisterPhysicsBody(entity)
  +UnregisterPhysicsBody(entity)
  +AddContactHandler(onBegin, onEnd)
}

class Sys_PhysicsRegistration["Systems::PhysicsRegistration"] {
  +RegisterPhysicsBody(entity)
  +UnregisterPhysicsBody(entity)
  +AddContactHandler(onBegin, onEnd)
}

Sys_PhysicsMachine --|> Core_UpdateNode
Sys_DrawSystem --|> Core_UpdateNode
Sys_Box2DPhysicsSystem --|> Core_UpdateNode
Sys_Box2DPhysicsSystem ..|> Sys_IPhysicsSystem
Sys_PhysicsMachine o-- Sys_IPhysicsSystem
Sys_PhysicsMachine o-- Sys_Box2DContactListener
Sys_PhysicsBinder ..|> Sys_IPhysicsBinder

Sys_PhysicsBinder *-- Sys_PhysicsMachine
Sys_PhysicsBinder *-- Sys_PhysicsRegistration
Sys_PhysicsRegistration o-- Sys_Box2DContactListener

Sys_PhysicsMachine ..> Sys_Box2DPhysicsSystem : steps world
Sys_Box2DPhysicsSystem ..> Sys_Box2DContactListener : processes contacts
Sys_Box2DContactListener ..> Sys_PhysicsRegistration : dispatch callbacks

%% =========================
%% Camera
%% =========================
class Camera_ICamera["CameraSpace::ICamera"] {
  <<interface>>
  +GetView()
  +GetProjection()
}

class Camera_Base["CameraSpace::BaseCamera"] {
  +GetView()
  +GetProjection()
  +SetPosition()
  +SetRotation()
  +SetPerspective()
}

class Camera_Perspective["CameraSpace::PerspectiveCamera"] {
  +OnViewportChanged()
  +SetFieldOfViewDegrees()
  +SetClipPlanes()
}

class Camera_CameraRig["CameraRig"] {
  +Attach(target)
  +OnUpdate(ctx: Core_UpdateContext)
}

class Camera_CameraManager["CameraSpace::CameraManager"] {
  +AddCamera(camera)
  +SetActive(camera)
  +GetActiveCamera()
  +Update(ctx: Core_UpdateContext)
}

Camera_Base ..|> Camera_ICamera
Camera_Perspective --|> Camera_Base
Camera_CameraRig --|> Core_UpdateNode
Camera_CameraRig o-- Camera_Base
Camera_CameraRig o-- Ent_Entity
Camera_CameraManager o-- Camera_ICamera

%% =========================
%% Entities
%% =========================
class Ent_Entity["Entities::Entity"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +Draw(renderer, camera)
  +GetTransform()
}

class Ent_PhysicalEntity["Entities::PhysicalEntity"] {
  +GetPhysical()
  +SetPosition()
  +SetRotation()
  +SetBox2DBodyId()
  +GetBox2DBodyId()
}

class Ent_Player["Entities::Player"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +Draw(renderer, camera)
  +SetSweetsManager(manager)
}

Ent_Entity --|> Core_UpdateNode
Ent_PhysicalEntity --|> Ent_Entity
Ent_Player --|> Ent_PhysicalEntity

%% =========================
%% Sweets
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
  +Draw(renderer, camera)
}

class Sweets_LateNode["SweetsSpace::SweetsManager::LateUpdateNode"] {
  +OnUpdate(ctx: Core_UpdateContext)
}

class Sweets_Lifecycle["SweetsSpace::SweetsLifecycle"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +OnLateUpdate(ctx: Core_UpdateContext)
  +RegisterWithSystems(sweets)
}

class Sweets_Manager["SweetsSpace::SweetsManager"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +OnLateUpdate(ctx: Core_UpdateContext)
  +DrawAll(renderer, camera)
  +Spawn(kind, transform)
  +LoadPhaseSpawnConfig(path)
  +OnContactBegin(a,b)
  +OnContactEnd(a,b)
  +GetLateUpdateNode() Core_IUpdatable*
}

Sweets_Base --|> Ent_PhysicalEntity
Sweets_Manager --|> Core_UpdateNode
Sweets_LateNode --|> Core_UpdateNode

Sweets_Manager *-- Sweets_LateNode
Sweets_Manager *-- Sweets_Lifecycle
Sweets_Manager ..> Core_UpdateScheduler : registers late node
Sweets_Manager ..> Sys_DrawSystem
Sweets_Manager ..> Sys_PhysicsMachine

%% =========================
%% Stage + Config
%% =========================
class Cfg_ReadSectionLines["Config::ReadSectionLines"] {
  <<utility>>
  +ReadSectionLines(path, section, outLines) bool
}

class Cfg_SceneGameConfig["SceneGameConfig (free functions)"] {
  <<utility>>
  +LoadPhysicsTuningSettings()
  +LoadPlayerTransformConfig()
  +LoadPlayerColliderTuning(path)
  +LoadModelTextureLoadingSetting()
}

class Stage_StageData["Stage::StageData"] {
  +LoadFromFile(path)
  +LoadFromFile(path, section)
  +GetGuidelineHeight() float
  +GetBackgroundTexturePath() string
}

class Stage_LayoutLoader["StageLayoutLoader"] {
  +LoadFromCsv(request) StageLayoutLoadResult
}

Stage_StageData ..> Cfg_ReadSectionLines
Cfg_SceneGameConfig ..> Cfg_ReadSectionLines

%% =========================
%% UI
%% =========================
class UI_UIManager["UIManager"] {
  +Initialize()
  +Shutdown()
  +UpdateHUD(data)
  +Draw()
}

class UI_HudRenderer["HudRenderer"] {
  +Initialize()
  +Shutdown()
  +Draw()
}

class UI_ResultRenderer["ResultRenderer"] {
  +Initialize()
  +Shutdown()
  +Draw()
}

class UI_Overlay["SceneGameUIOverlay"] {
  +Initialize()
  +Shutdown()
  +Update(data)
  +Draw()
}

UI_UIManager *-- UI_HudRenderer
UI_UIManager *-- UI_ResultRenderer
UI_Overlay *-- UI_UIManager

%% =========================
%% PowerUp scene
%% =========================
class PU_Manager["PowerUpManager"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +AddNode(...)
  +DrawAll(renderer, camera)
}

class PU_Cursor["PowerUpCursor"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +Draw(renderer, camera)
}

class PU_Node["PowerUpNode"] {
  +OnUpdate(ctx: Core_UpdateContext)
  +Draw(renderer, camera)
  +LevelUp()
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
Scene_SceneGame *-- Camera_CameraRig
Scene_SceneGame *-- Ent_Player
Scene_SceneGame *-- Stage_StageData

Scene_SceneGame ..> Core_UpdateScheduler : registers nodes
Scene_SceneGame ..> Sys_PhysicsBinder : contact handlers
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
