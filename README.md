# HAL_HEW_216

## 必須アセット
本リポジトリでは、以下のプリビルド済み DirectXTex ライブラリを常にバージョン管理下に置いています。

- `HEWGame/DirectXTex/Win32/Debug`
- `HEWGame/DirectXTex/Win32/Release`
- `HEWGame/DirectXTex/x64/Debug`
- `HEWGame/DirectXTex/x64/Release`

これらの `.lib` / `.dll` ファイルはリンカ設定で直接参照されており、リポジトリから削除するとビルドが失敗します。ツールチェーン（Visual Studio、Windows SDK、プラットフォーム ツールセットなど）を更新してライブラリを再生成する必要がある場合は、DirectXTex のソリューションを同じ構成でビルドし直し、上記ディレクトリに成果物を上書きしてください。その後、差分を確認したうえでコミットし、常に最新のバイナリを共有できるようにしてください。

また、`HEWGame/DX22_Project.vcxproj.user` も意図的にバージョン管理されています。このファイルには Visual Studio 固有のユーザー設定が保存され、チーム全員のデバッガ設定やツールセット指定 (`ToolsVersion="Current"`) をそろえる役割を担っています。空の `<PropertyGroup>` を含めた最小構成ですが、Visual Studio が環境依存の値を勝手に書き込むのを防ぎ、設定差分を抑制する目的で維持しています。削除・無視すると開発環境ごとに挙動がばらつく原因になるため、リポジトリから除外しないでください。

クラス図（Mermaid用）：
---
config:
  class:
    hideEmptyMembersBox: false
  look: neo
  layout: elk
---
classDiagram
direction TB
namespace Core {
  class UpdatePhase["Core::UpdatePhase"] { <<enumeration>>
    Input
    PrePhysics
    Physics
    PostPhysics
    Gameplay
    Camera
    UI
    Late
  }
  class UpdateContext["Core::UpdateContext"] { <<struct>>
    dt : float
    time : StepTimer&
    input : Input&
    scene : SceneSpace_Scene*
  }
  class IUpdatable["Core::IUpdatable"] { <<interface>>
    +OnUpdate(ctx:UpdateContext)
    +GetPhase() UpdatePhase
    +GetPriority() int
    +IsEnabled() bool
  }
  class UpdateNode["Core::UpdateNode"] { <<abstract>>
    - phase : UpdatePhase
    - priority : int
    - enabled : bool
    +UpdateNode(phase:UpdatePhase, priority:int=0)
    +OnUpdate(ctx:UpdateContext)
    +GetPhase() UpdatePhase
    +GetPriority() int
    +SetPriority(p:int)
    +SetPhase(ph:UpdatePhase)
    +Enable(v:bool)
  }
  class UpdateScheduler["Core::UpdateScheduler"] {
    - nodes : list<IUpdatable*>
    - deps  : list<pair<IUpdatable*,IUpdatable*>>
    +Register(node:IUpdatable*)
    +Unregister(node:IUpdatable*)
    +SetOrder(node:IUpdatable*, phase:UpdatePhase, priority:int)
    +AddDependency(before:IUpdatable*, after:IUpdatable*)
    +Tick(ctx:UpdateContext)
  }
  class DXContext["Core::DXContext"] {
    +Init(window)
    +BeginDraw()
    +EndDraw()
    +Resize(width, height)
    +Shutdown()
  }
  class StepTimer["Core::StepTimer"] {
    +Tick()
    +DeltaTime() float
    +SetFixedStep(dt:float)
  }
  class ActionId["Core::ActionId"] { <<enumeration>>
    MoveX
    RotateYaw
    Drop
    Confirm
    Cancel
    Pause
    Retry
    DebugConsole
    CameraToggle
    CameraHeightToggle
    SpeedBoost
    SpeedReset
    ColliderNudgeUp
    ColliderNudgeDown
  }
  class Binding["Core::Binding"] { <<struct>>
    key:int
    btn:int
    axis:int
    sign:float
  }
  class Gamepad["Core::Gamepad"] {
    +IsConnected() bool
    +AxisLX() float
    +AxisRX() float
    +IsButtonDown(code:int) bool
  }
  class Input["Core::Input"] {
    - pad : Gamepad
    - map : InputMap
    +OnUpdate(ctx:Core_UpdateContext)
    +GetAxis(a:ActionId) float
    +IsAction(a:ActionId) bool
    +IsPress(vk:int) bool
    +IsTrigger(vk:int) bool
    +IsRelease(vk:int) bool
  }
  class InputMap["Core::InputMap"] {
    - bindings : map<ActionId, Binding>
    +GetAxis(a:ActionId) float
    +Is(a:ActionId) bool
    +Rebind(a:ActionId, b:Binding)
  }
  class ShaderProgram["Core::ShaderProgram"] { +Bind() }
  class ShaderCache["Core::ShaderCache"] {
    - programs : map<string, ShaderProgram>
    +Get(name) ShaderProgram&
    +Load(name, vsPath, psPath)
  }
  class Texture2D["Core::Texture2D"] { +LoadFromFile(path) +BindPS(slot) }
  class TextureCache["Core::TextureCache"] {
    - textures : map<string, shared_ptr<Texture2D>>
    +Get(path) shared_ptr<Texture2D>
  }
  class VertexBuffer["Core::VertexBuffer"] { +Init(data,stride,count) +Bind(slot) +Shutdown() }
  class IndexBuffer["Core::IndexBuffer"]  { +Init(indices,count) +Bind() +Shutdown() }
}
UpdateNode ..|> IUpdatable
UpdateNode <|-- Input
UpdateScheduler o-- IUpdatable
Input o-- Gamepad
Input o-- InputMap
InputMap *-- Binding
InputMap *-- ActionId
ShaderCache *-- ShaderProgram
TextureCache *-- Texture2D
namespace Components {
  class Transform["Components::Transform"] { <<struct>>
    position : float3
    rotation : float3
    scale    : float3
  }
  class Physical["Components::Physical"] { <<struct>>
    velocity : float3
    force    : float3
    weight   : float
    damping  : float
  }
  class ICollider["Components::ICollider"] { <<interface>>
    +CheckCollision(other:ICollider) bool
    +UpdateBounds(t:Transform)
  }
}
namespace CameraSpace {
  class ICamera["CameraSpace::ICamera"] { <<interface>> +GetView() +GetProjection() }
  class BaseCamera["CameraSpace::BaseCamera"] {
    - transform : Components_Transform
    - priority  : int
    +GetView()
    +GetProjection()
  }
  class PerspectiveCamera["CameraSpace::PerspectiveCamera"] {
    - fov   : float
    - aspect: float
    - nearZ : float
    - farZ  : float
  }
  class CameraManager["CameraSpace::CameraManager"] {
    - cameras : list<ICamera>
    +GetActiveCamera() ICamera&
    +Update()
  }
  class CameraRig["CameraSpace::CameraRig"] {
    - target : SceneSpace_Entity*
    - offset : float3
    - yaw:float
    - pitch:float
    +Attach(target:SceneSpace_Entity*)
    +OnUpdate(ctx:Core_UpdateContext)
  }
}
BaseCamera ..|> ICamera
BaseCamera <|-- PerspectiveCamera
CameraManager *-- ICamera
Core_UpdateNode <|-- CameraRig
CameraRig ..> BaseCamera
namespace Render {
  class Mesh["Render::Mesh"] {
    - vbo      : Core_VertexBuffer
    - ibo      : Core_IndexBuffer
  }
  class Material["Render::Material"] {
    - albedo : shared_ptr<Core_Texture2D>
    - params : float4
  }
  class Model["Render::Model"] {
    - meshes    : vector<Mesh>
    - materials : vector<Material>
    +GetMeshCount() int
  }
  class IModelLoader["Render::IModelLoader"] { <<interface>> +Load(path:string) shared_ptr<Model> }
  class AssimpModelLoader["Render::AssimpModelLoader"] { +Load(path:string) shared_ptr<Model> }
  class ModelCache["Render::ModelCache"] {
    - loader : IModelLoader
    - models : map<string, shared_ptr<Model>>
    +Get(path:string) shared_ptr<Model>
    +SetLoader(loader:IModelLoader)
  }
  class ModelRenderer["Render::ModelRenderer"] {
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
  class IPhysicsSystem["Systems::IPhysicsSystem"] { <<interface>> +Apply(obj:SceneSpace_PhysicalEntity, dt:float) }
  class PhysicsMachine["Systems::PhysicsMachine"] {
    - system : IPhysicsSystem
    +OnUpdate(ctx:Core_UpdateContext)
    +SetSystem(sys:IPhysicsSystem)
  }
}
Core_UpdateNode <|-- PhysicsMachine
PhysicsMachine ..> IPhysicsSystem
namespace GameFlow {
  class PhaseManager["GameFlow::PhaseManager"] {
    - currentPhase : int
    - thresholds   : vector<float>
    +Reset(phases:int, thresholds:vector<float>)
    +OnUpdate(ctx:Core_UpdateContext)
    +GetCurrentPhase() int
    +IsPhaseCleared() bool
  }
  class UpgradeId["GameFlow::UpgradeId"] { <<enumeration>>
    Heavier
    ControlPlus
    DropAssist
  }
  class UpgradeMenu["GameFlow::UpgradeMenu"] {
    +Open()
    +SelectUpgrade(option:int)
    +ApplyToPlayer(player:Entities_Player, up:UpgradeId)
  }
}
Core_UpdateNode <|-- PhaseManager
namespace Stage {
  class StageData["Stage::StageData"] {
    - phaseHeights : vector<float>
    - spawnPoints  : vector<float3>
    - planeZ       : float
    +LoadFromFile(path)
  }
}
namespace SceneSpace {
  class IDrawable["SceneSpace::IDrawable"]  { <<interface>> +Draw(renderer:Render_ModelRenderer, cam:CameraSpace_ICamera) }
  class Entity["SceneSpace::Entity"] {
    - transform : Components_Transform
    - tag : string
    +OnUpdate(ctx:Core_UpdateContext)
    +Draw(renderer:Render_ModelRenderer, cam:CameraSpace_ICamera)
    +GetTransform() Components_Transform&
  }
  class CollisionEntity["SceneSpace::CollisionEntity"] {
    - collider : shared_ptr<Components_ICollider>
    +GetCollider() Components_ICollider&
  }
  class PhysicalEntity["SceneSpace::PhysicalEntity"] {
    - physical : Components_Physical
    +GetPhysical() Components_Physical&
  }
  class IScene["SceneSpace::IScene"] { <<interface>> +Load() +Update() +Draw() }
  class Scene["SceneSpace::Scene"] {
    - cameras  : CameraSpace_CameraManager
    - physics  : Systems_PhysicsMachine
    - renderer : Render_ModelRenderer
    - scheduler: Core_UpdateScheduler
    +Load() +Update() +Draw()
    +Register(node:Core_IUpdatable*)
    +Unregister(node:Core_IUpdatable*)
  }
  class SceneManager["SceneSpace::SceneManager"] {
    - current : shared_ptr<IScene>
    +SetScene(scene:shared_ptr<IScene>)
    +Update()
    +Draw()
  }
  class TitleScene["SceneSpace::TitleScene"]
  class SelectScene["SceneSpace::SelectScene"]
  class PlayScene["SceneSpace::PlayScene"] {
    - stage       : Stage_StageData
    - phases      : GameFlow_PhaseManager
    - upgrades    : GameFlow_UpgradeMenu
    - player      : Entities_Player
    - sweets      : SweetsSpace_SweetsManager
    - rig         : CameraSpace_CameraRig
    +Load()
    +Update()
    +Draw()
  }
}
Core_UpdateNode <|-- Entity
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
Scene o-- Core_UpdateScheduler
PlayScene o-- Stage_StageData
PlayScene o-- GameFlow_PhaseManager
PlayScene o-- GameFlow_UpgradeMenu
PlayScene o-- CameraSpace_CameraRig
namespace Entities {
  class Player["Entities::Player"] {
    - model : shared_ptr<Render_Model>
    - planeZ : float
    +HandleInput(input:Core_Input)
    +OnUpdate(ctx:Core_UpdateContext)
  }
}
SceneSpace_PhysicalEntity <|-- Player
namespace SweetsSpace {
  class SweetsState["SweetsSpace::SweetsState"] { <<enumeration>> Active Stacked Ground Carried }
  class BaseSweets["SweetsSpace::BaseSweets"] {
    - model : shared_ptr<Render_Model>
    +OnUpdate(ctx:Core_UpdateContext)
    +Draw(renderer:Render_ModelRenderer, cam:CameraSpace_ICamera)
  }
  class SweetsManager["SweetsSpace::SweetsManager"] {
    - byState : map<SweetsState, list<shared_ptr<BaseSweets>>>
    +OnUpdate(ctx:Core_UpdateContext)
    +DrawAll(renderer:Render_ModelRenderer, cam:CameraSpace_ICamera)
    +GetByState(state:SweetsState) list<shared_ptr<BaseSweets>>&
    +Spawn(kind:string, world:Components_Transform)
  }
}
SceneSpace_PhysicalEntity <|-- BaseSweets
SweetsManager *-- BaseSweets
SweetsManager *-- SweetsState
Player o-- Render_Model
BaseSweets o-- Render_Model
Core_UpdateNode <|-- BaseSweets
Core_UpdateNode <|-- SweetsManager
namespace App {
  class Game["App::Game"] {
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
Game o-- Core_DXContext
Game o-- Core_Input
Game o-- Core_StepTimer
Game o-- Core_ShaderCache
Game o-- Core_TextureCache
Game o-- Render_ModelCache
Game o-- Render_ModelRenderer
Game o-- SceneSpace_SceneManager

クラス図ざっくりまとめ：

Core：
DirectXまわり。GitHubリポジトリに入ってる初期化コードをそのままラップしてるだけ
DXContext・Input・Shader/TextureCacheあたりはいじらなくてOK

Components：
オブジェクトの基礎データ
Transform（位置）、Physical（速度とか）、2D コライダー（当たり判定）だけで十分

CameraSpace：
カメラは1個だけ。priorityとかあるけど今は使わない
3Dの奥行きはあるけど、プレイ自体は横方向（2.5D）固定

Render：
モデル描画まわり。ModelRenderer.Draw()一発で描ける
ローダーは仮でOK（立方体とかで代用）

Systems：
物理系。重力＋減衰だけ
接地感は、余裕あったら別システムで

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

UpdateNode:
毎フレーム、Scene がスケジューラを呼び出し、登録済みオブジェクトを フェーズ → priority の順に実行する。更新が必要なクラスは Core::UpdateNode を継承し、フェーズと priority を設定して Scene に登録する。以後は自動で OnUpdate(ctx) が呼ばれるので、順序を変えたい時はフェーズ/priorityを調整するか、Enable(false) で無効化するだけ。
