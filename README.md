# HAL_HEW_216

## 必須アセット (Required Assets)
本リポジトリでは、以下のプリビルド済み DirectXTex ライブラリを常にバージョン管理下に置いています。

- `HEWGame/DirectXTex/Win32/Debug`
- `HEWGame/DirectXTex/Win32/Release`
- `HEWGame/DirectXTex/x64/Debug`
- `HEWGame/DirectXTex/x64/Release`

これらの `.lib` / `.dll` ファイルはリンカ設定で直接参照されており、リポジトリから削除するとビルドが失敗します。ツールチェーン（Visual Studio、Windows SDK、プラットフォーム ツールセットなど）を更新してライブラリを再生成する必要がある場合は、DirectXTex のソリューションを同じ構成でビルドし直し、上記ディレクトリに成果物を上書きしてください。その後、差分を確認したうえでコミットし、常に最新のバイナリを共有できるようにしてください。

また、`HEWGame/HEWGame.vcxproj.user` も意図的にバージョン管理されています。このファイルには Visual Studio 固有のユーザー設定が保存され、チーム全員のデバッガ設定やツールセット指定 (`ToolsVersion="Current"`) をそろえる役割を担っています。空の `<PropertyGroup>` を含めた最小構成ですが、Visual Studio が環境依存の値を勝手に書き込むのを防ぎ、設定差分を抑制する目的で維持しています。削除・無視すると開発環境ごとに挙動がばらつく原因になるため、リポジトリから除外しないでください。

クラス図：
---
config:
  class:
    hideEmptyMembersBox: false
  look: neo
  layout: elk
  theme: neo
---
classDiagram
direction TB
namespace Core {
  class DXContext {
    +Init(window)
    +BeginDraw()
    +EndDraw()
    +Resize(width, height)
    +Shutdown()
  }
  class StepTimer {
    +Tick()
    +DeltaTime() float
    +SetFixedStep(dt:float)
  }
  class ActionId { <<enumeration>>
    MoveX
    RotateYaw
    Drop
    Confirm
    Cancel
    Pause
  }
  class Binding { <<struct>>
    key:int
    btn:int
    axis:int
    sign:float
  }
  class Gamepad {
    +IsConnected() bool
    +AxisLX() float
    +AxisRX() float
    +IsButtonDown(code:int) bool
  }
  class InputMap {
    - bindings : map<ActionId, Binding>
    +GetAxis(a:ActionId) float
    +Is(a:ActionId) bool
    +Rebind(a:ActionId, b:Binding)
  }
  class Input {
    - pad : Gamepad
    - map : InputMap
    +Update()
    +GetAxis(a:ActionId) float
    +IsAction(a:ActionId) bool
    +IsPress(vk:int) bool     %% キーボードの簡易チェック（保険）
    +IsTrigger(vk:int) bool
    +IsRelease(vk:int) bool
  }
  class ShaderProgram { +Bind() }
  class ShaderCache {
    - programs : map<string, ShaderProgram>
    +Get(name) ShaderProgram&
    +Load(name, vsPath, psPath)
  }
  class Texture2D { +LoadFromFile(path) +BindPS(slot) }
  class TextureCache {
    - textures : map<string, shared_ptr<Texture2D>>
    +Get(path) shared_ptr<Texture2D>
  }
  class VertexBuffer { +Init(data,stride,count) +Bind(slot) +Shutdown() }
  class IndexBuffer  { +Init(indices,count)     +Bind()     +Shutdown() }
}
Input o-- Gamepad
Input o-- InputMap
InputMap *-- Binding
InputMap *-- ActionId
ShaderCache *-- ShaderProgram
TextureCache *-- Texture2D
namespace Components {
  class Transform { <<struct>>
    position : float3
    rotation : float3
    scale    : float3
  }
  class Physical { <<struct>>
    velocity : float3
    force    : float3
    weight   : float
    damping  : float
  }
  class ICollider { <<interface>>
    +CheckCollision(other:ICollider) bool
    +UpdateBounds(t:Transform)
  }
  class AABBCollider {
    +CheckCollision(other:ICollider) bool
    +UpdateBounds(t:Transform)
  }
  class OBBCollider {
    +CheckCollision(other:ICollider) bool
    +UpdateBounds(t:Transform)
  }
  class AABB { <<struct>> min:float3 max:float3 }
}
AABBCollider ..|> ICollider
OBBCollider  ..|> ICollider
namespace CameraSpace {
  class ICamera { <<interface>> +GetView() +GetProjection() }
  class BaseCamera {
    - transform : Components_Transform
    - priority  : int
    +GetView()
    +GetProjection()
  }
  class PerspectiveCamera {
    - fov   : float
    - aspect: float
    - nearZ : float
    - farZ  : float
  }
  class CameraManager {
    - cameras : list<ICamera>
    +GetActiveCamera() ICamera&
    +Update()
  }
  class CameraRig {
    - target : SceneSpace_Entity*
    - offset : float3
    - yaw:float
    - pitch:float
    +Attach(target:SceneSpace_Entity*)
    +Update(input:Core_Input, dt:float, cam:BaseCamera)
  }
}
BaseCamera ..|> ICamera
BaseCamera <|-- PerspectiveCamera
CameraManager *-- ICamera
CameraRig ..> CameraSpace_BaseCamera
namespace Render {
  class Mesh {
    - vbo      : Core_VertexBuffer
    - ibo      : Core_IndexBuffer
    - localAABB: Components_AABB
  }
  class Material {
    - albedo : shared_ptr<Core_Texture2D>  %% 最初はアルベドのみ
    - params : float4                      %% 将来PBR等を拡張
  }
  class Model {
    - meshes    : vector<Mesh>
    - materials : vector<Material>
    +GetMeshCount() int
  }
  class IModelLoader { <<interface>> +Load(path:string) shared_ptr<Model> }
  class AssimpModelLoader { +Load(path:string) shared_ptr<Model> }
  class ModelCache {
    - loader : IModelLoader
    - models : map<string, shared_ptr<Model>>
    +Get(path:string) shared_ptr<Model>
    +SetLoader(loader:IModelLoader)
  }
  class ModelRenderer {
    +Draw(model:Model, world:Components_Transform, cam:CameraSpace_ICamera)
  }
}
AssimpModelLoader ..|> IModelLoader
ModelCache ..> IModelLoader
ModelCache *-- Model
Mesh *-- Core_VertexBuffer
Mesh *-- Core_IndexBuffer
Material *-- Core_Texture2D
ModelRenderer ..> Core_DXContext
ModelRenderer ..> Core_ShaderCache
ModelRenderer ..> CameraSpace_ICamera
ModelRenderer ..> Render_Model
namespace Systems {
  class IPhysicsSystem { <<interface>> +Apply(obj:SceneSpace_PhysicalEntity, dt:float) }
  class GravityDampingSystem {
    - gravity : float3
    +Apply(obj:SceneSpace_PhysicalEntity, dt:float)
  }
  class PhysicsMachine {
    - system : IPhysicsSystem
    +Process(obj:SceneSpace_PhysicalEntity, dt:float)
    +SetSystem(sys:IPhysicsSystem)
  }
  class ContactLiteSystem {
    +ResolveY(obj:SceneSpace_PhysicalEntity, staticList:list<SceneSpace_CollisionEntity*>)
  }
}
GravityDampingSystem ..|> IPhysicsSystem
PhysicsMachine ..> IPhysicsSystem
namespace GameFlow {
  class PhaseManager {
    - currentPhase : int
    - thresholds   : vector<float>   %% 例：高さライン [H1, H2, ...]
    +Reset(phases:int, thresholds:vector<float>)
    +Update(playerY:float)           %% 閾値を越えたら currentPhase++
    +GetCurrentPhase() int
    +IsPhaseCleared() bool           %% 今フェーズを抜けた瞬間か
  }
  class UpgradeId { <<enumeration>>
    Heavier      %% 重さUP（風に流されにくく 等）※例
    ControlPlus  %% 操作性UP（回転制御 等）※例
    DropAssist   %% 落下補助（スナップ 等）※例
  }
  class UpgradeMenu {
    +Open()
    +SelectUpgrade(option:int)
    +ApplyToPlayer(player:Entities_Player, up:UpgradeId)
  }
}
namespace Stage {
  class StageData {
    - phaseHeights : vector<float>   %% 各フェーズの高さライン
    - spawnPoints  : vector<float3>  %% スイーツの出現位置（2.5DならZは固定）
    - planeZ       : float           %% プレイ面のZ（固定値）
    +LoadFromFile(path)
  }
}
namespace SceneSpace {
  class IUpdatable { <<interface>> +Update(dt:float) }
  class IDrawable  { <<interface>> +Draw(renderer:Render_ModelRenderer, cam:CameraSpace_ICamera) }
  class Entity {
    - transform : Components_Transform
    - tag : string
    +Update(dt:float)
    +Draw(renderer:Render_ModelRenderer, cam:CameraSpace_ICamera)
    +GetTransform() Components_Transform&
  }
  class CollisionEntity {
    - collider : shared_ptr<Components_ICollider>
    +GetCollider() Components_ICollider&
  }
  class PhysicalEntity {
    - physical : Components_Physical
    +GetPhysical() Components_Physical&
  }
  class IScene { <<interface>> +Load() +Update() +Draw() }
  class Scene {
    - cameras  : CameraSpace_CameraManager
    - physics  : Systems_PhysicsMachine
    - renderer : Render_ModelRenderer
    +Load() +Update() +Draw()
  }
  class SceneManager {
    - current : shared_ptr<IScene>
    +SetScene(scene:shared_ptr<IScene>)
    +Update()
    +Draw()
  }
  class TitleScene
  class SelectScene
  class PlayScene {
    - stage       : Stage_StageData
    - phases      : GameFlow_PhaseManager
    - upgrades    : GameFlow_UpgradeMenu
    - player      : Entities_Player
    - sweets      : SweetsSpace_SweetsManager
    - contactLite : Systems_ContactLiteSystem   %% 任意
    - rig         : CameraSpace_CameraRig       %% 任意
    +Load()
    +Update()
    +Draw()
  }
}
Entity ..|> IUpdatable
Entity ..|> IDrawable
Entity *-- Components_Transform
Entity <|-- CollisionEntity
CollisionEntity *-- Components_ICollider
CollisionEntity <|-- PhysicalEntity
PhysicalEntity *-- Components_Physical
IScene <|-- Scene
SceneManager *-- IScene
IScene <|-- TitleScene
IScene <|-- SelectScene
IScene <|-- PlayScene
Scene o-- CameraSpace_CameraManager
Scene o-- Systems_PhysicsMachine
Scene o-- Render_ModelRenderer
PlayScene o-- Stage_StageData
PlayScene o-- GameFlow_PhaseManager
PlayScene o-- GameFlow_UpgradeMenu
PlayScene o-- Systems_ContactLiteSystem
PlayScene o-- CameraSpace_CameraRig
namespace Entities {
  class Player {
    - model : shared_ptr<Render_Model>
    - planeZ : float             %% 2.5D用。Z固定の基準
    +HandleInput(input:Core_Input)  %% MoveX/RotateYaw/Drop を反映
  }
}
namespace SweetsSpace {
  class SweetsState { <<enumeration>> Active Stacked Ground }
  class BaseSweets {
    - model : shared_ptr<Render_Model>
    +Update(dt:float)
    +Draw(renderer:Render_ModelRenderer, cam:CameraSpace_ICamera)
  }
  class Cake
  class Pudding
  class Sherbet
  class SweetsManager {
    - byState : map<SweetsState, list<shared_ptr<BaseSweets>>>
    +UpdateAll(dt:float)
    +DrawAll(renderer:Render_ModelRenderer, cam:CameraSpace_ICamera)
    +GetByState(state:SweetsState) list<shared_ptr<BaseSweets>>&
    +Spawn(kind:string, world:Components_Transform)
  }
}
SceneSpace_PhysicalEntity <|-- Entities_Player
SceneSpace_PhysicalEntity <|-- SweetsSpace_BaseSweets
SweetsSpace_BaseSweets <|-- Cake
SweetsSpace_BaseSweets <|-- Pudding
SweetsSpace_BaseSweets <|-- Sherbet
SweetsManager *-- BaseSweets
SweetsManager *-- SweetsState
Entities_Player o-- Render_Model
SweetsSpace_BaseSweets o-- Render_Model
namespace App {
  class Game {
    - dx       : Core_DXContext
    - input    : Core_Input
    - timer    : Core_StepTimer
    - shaders  : Core_ShaderCache
    - textures : Core_TextureCache
    - models   : Render_ModelCache
    - renderer : Render_ModelRenderer
    - scenes   : SceneSpace_SceneManager
    +Initialize()
    +Update()
    +Draw()
  }
}
App_Game o-- Core_DXContext
App_Game o-- Core_Input
App_Game o-- Core_StepTimer
App_Game o-- Core_ShaderCache
App_Game o-- Core_TextureCache
App_Game o-- Render_ModelCache
App_Game o-- Render_ModelRenderer
App_Game o-- SceneSpace_SceneManager

クラス図ざっくりまとめ：

Core：
DirectXまわり。GitHubリポジトリに入ってる初期化コードをそのままラップしてるだけ
DXContext・Input・Shader/TextureCacheあたりはいじらなくてOK

Components：
オブジェクトの基礎データ
Transform（位置）、Physical（速度とか）、AABBCollider（当たり判定）だけで十分

CameraSpace：
カメラは1個だけ。priorityとかあるけど今は使わない
3Dの奥行きはあるけど、プレイ自体は横方向（2.5D）固定

Render：
モデル描画まわり。ModelRenderer.Draw()一発で描ける
ローダーは仮でOK（立方体とかで代用）

Systems：
物理系。重力＋減衰だけ
接地感は、余裕あったら ContactLiteSystem で

GameFlow：
フェーズ管理と強化メニュー
一定の高さを超えたらフェーズ進行 → 終わったら強化選択 (まだ決まってないので、例になっている)
ここが今回の新要素

Scene / Entities / SweetsSpace：
PlaySceneに全部入ってる
プレイヤー（横移動＋回転＋落下　←例）とスイーツの管理
フェーズごとに難易度（出現間隔とか）を調整

App：
全部まとめて動かすメイン的なやつ
Initialize → Update → Draw で完結
