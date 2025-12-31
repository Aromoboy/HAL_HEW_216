# Scene lifecycle / UpdateNode order

This document captures the **current** `UpdateNode` execution order for the main gameplay scene (`SceneGame`) based on the configured phases, priorities, and registration order.

## Scheduling rules

The scheduler sorts registered nodes by:

1. **Phase** (`Core::UpdatePhase` enum order),
2. **Priority** (ascending integer),
3. **Registration order** (earlier `Register` calls run first).

These rules are implemented in `Core::UpdateScheduler::NodeLess`.

## UpdateNode table (requested classes)

| UpdateNode | Phase | Priority | Where set |
| --- | --- | --- | --- |
| `Core::Input` | `Input` | `0` | Constructor sets `UpdatePhase::Input` (`HEWGame/src/Input.cpp`). |
| `Systems::PhysicsMachine` | `Physics` | `0` | Constructor sets `UpdatePhase::Physics` (`HEWGame/src/Systems/PhysicsMachine.cpp`). |
| `Systems::DrawSystem` | `Camera` | `0` | Constructor sets `UpdatePhase::Camera` (`HEWGame/src/Systems/DrawSystem.cpp`). |
| `CameraRig` | `Camera` | `0` | Constructor sets `UpdatePhase::Camera` (`HEWGame/src/CameraRig.cpp`). |
| `Entities::Player` | `Gameplay` | `0` | `SceneGame` constructs the player with `UpdatePhase::Gameplay` + priority `0` (`HEWGame/src/SceneGame.cpp`). |
| `SweetsSpace::SweetsManager` | `Gameplay` | `0` | Defaults to `UpdateNode` base constructor (`UpdatePhase::Gameplay`, priority `0`) (`HEWGame/include/Core/UpdateNode.h`, `HEWGame/src/SweetsManager.cpp`). |
| `SceneSpace::SceneUpdateNode` | `Gameplay` | `0` | `SceneGame::ConfigureUpdates` calls `RegisterSceneUpdateNode(UpdatePhase::Gameplay, 0)` (`HEWGame/src/SceneGame.cpp`, `HEWGame/src/Scene.cpp`). |
| `SweetsSpace::LateUpdateNode` | `Late` | `0` | `SweetsManager` creates `LateUpdateNode(UpdatePhase::Late, 0)` (`HEWGame/src/SweetsManager.cpp`). |

## Concrete order in `SceneGame::ConfigureUpdates`

`SceneGame::ConfigureUpdates` registers nodes in this sequence:

1. `Systems::PhysicsMachine`
2. `Core::Input`
3. `SceneSpace::SceneUpdateNode`
4. `Systems::DrawSystem`
5. `CameraRig`
6. `SweetsSpace::SweetsManager`
7. `SweetsSpace::LateUpdateNode`
8. `Entities::Player`

Because the scheduler sorts by phase first, the **effective execution order** for the requested nodes is:

1. **Input phase**: `Core::Input`
2. **Physics phase**: `Systems::PhysicsMachine`
3. **Gameplay phase** (priority `0`, registration order):
   - `SceneSpace::SceneUpdateNode`
   - `SweetsSpace::SweetsManager`
   - `Entities::Player`
4. **Camera phase** (priority `0`, registration order):
   - `Systems::DrawSystem`
   - `CameraRig`
5. **Late phase**: `SweetsSpace::LateUpdateNode`

> Note: `Scene::RootUpdate` also registers `SceneSpace::CameraUpdateNode` (Camera phase, priority `0`) after `ConfigureUpdates`, so it runs **after** `DrawSystem` and `CameraRig` within the Camera phase.
