# Loader and Entity Logging Reintroduction Notes

The following call sites already emit debug traces or guardrails around asset/system setup. If we reintroduce structured logging, these contexts and suggested severities can guide which messages should become **errors** vs **warnings/info**.

## Stage layout loader (`SceneGame::LoadStageLayout`)
- Location: `HEWGame/src/SceneGame.cpp`.
- Behavior: traces when the CSV cannot be opened, when it is opened, before each model load, on per-model load failure, and after counting loaded stage objects.
- Config path: `Assets/Config/StageLayout.csv`.
- Suggested severity:
  - **Error** when the CSV cannot be opened (`file.is_open()` fails) because stage initialization aborts early.
  - **Warning** when an individual model fails to load; the loader continues after logging the failure.
  - **Info** for the start/end progress messages (file opened, per-model load attempt/success, total loaded count).

## Stage spawn anchors (`StageData::LoadFromFile`)
- Location: `HEWGame/src/Stage/StageData.cpp` (see the spawn lane sanitization block).
- Behavior: logs per-lane anchor counts, when defaults are injected for a missing primary lane, when the optional secondary lane is absent, and when both lanes are empty (fallback `(0,0,0)` anchor). Cross-plane mismatches are also reported.
- Suggested severity:
  - **Warning** when anchor parsing fails or when both lanes are empty and the fallback anchor is injected.
  - **Info** when defaults are applied intentionally (missing primary lane) or when lanes are intentionally empty (secondary omitted).

## Model loader (`AssimpModelLorder`)
- Location: `HEWGame/src/AssimpModelLorder.cpp`.
- Behavior: returns `nullptr` if `Assimp::Importer::ReadFile` fails (error string available under `_DEBUG`) and logs texture load failures with `OutputDebugStringA` while continuing without a texture.
- Suggested severity:
  - **Error** when `ReadFile` fails so callers know the model is unusable (currently silent outside debug builds).
  - **Warning** when a diffuse texture cannot be loaded because geometry still renders with default material.

## Entity system registration (`SweetsManager`)
- Location: `HEWGame/src/SweetsManager.cpp` (`RegisterWithSystems` and `UnregisterSweetsFromSystems`).
- Behavior: one-time warnings when scheduler/physics/draw systems are missing during registration or unregistration, gated by `logWarnings` for teardown paths.
- Suggested severity:
  - **Warning** for all three missing-system cases—they indicate a configuration issue but the game can limp along.

## Player asset resolution (`Entities::Player`)
- Location: `HEWGame/src/Entities/Player.cpp` constructor.
- Behavior: traces whether an explicit model was provided, resolves the fairy manifest entry, notes fallbacks when the descriptor is missing/empty, and logs when both procedural and cached assets fail, leaving the player without a model.
- Suggested severity:
  - **Info** for the standard resolution path (using provided model, resolving descriptor).
  - **Warning** when falling back to placeholder assets because manifest data is missing/empty.
  - **Error** when no fairy model is available after all fallbacks, since rendering will be missing entirely.

## Sweets spawn pipeline (`SweetsSpawner` / `SweetsManager` / `SweetsLifecycle`)
- Location:
  - `HEWGame/Game/Sweets/SweetsSpawner.cpp`
  - `HEWGame/Game/Sweets/SweetsManager.cpp`
  - `HEWGame/Game/Sweets/SweetsLifecycle.cpp`
- Config gate:
  - `HEWGame/Assets/Config/gameplay.cfg` section `[sweets.spawn.debug]` (`enabled`, `rateLimitSeconds`).
- Behavior:
  - Runtime snapshot (`SpawnDebugSnapshot`) is emitted only for state transitions and key events.
  - Block reason transitions include: `BlockedByLiveCap`, `BlockedByPerPointCap`, `NoOrigins`, `NoWeights`, `CoolingDown`, `None`.
  - Queue transition logs are emitted only on `0 -> >0` and `>0 -> 0` boundaries.
  - `NextSpawnOrigin()` emits a first-failure aggregate occupancy log when no lane can be selected.
  - Spawn result logs are unified in `SweetsManager::Spawn` with explicit fail reasons.
  - Despawn/removal logs are symmetric in lifecycle with removal context and post-removal state counts.
- Message keys for grep/analysis:
  - `key=block.transition`
  - `key=queue.transition`
  - `key=origin.none`
  - `key=origin.selection.fail`
  - `key=spawn.result.success`
  - `key=spawn.result.fail`
  - `[SpawnResult]` (manager-level spawn attempt result)
  - `[DespawnResult]` (lifecycle removal/clear summary)
