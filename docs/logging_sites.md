# Loader and Entity Logging Reintroduction Notes

The following call sites already emit debug traces or guardrails around asset/system setup. If we reintroduce structured logging, these contexts and suggested severities can guide which messages should become **errors** vs **warnings/info**.

## Stage layout loader (`SceneGame::LoadStageLayout`)
- Location: `HEWGame/src/SceneGame.cpp`.
- Behavior: traces when the CSV cannot be opened, when it is opened, before each model load, on per-model load failure, and after counting loaded stage objects.
- Suggested severity:
  - **Error** when the CSV cannot be opened (`file.is_open()` fails) because stage initialization aborts early.
  - **Warning** when an individual model fails to load; the loader continues after logging the failure.
  - **Info** for the start/end progress messages (file opened, per-model load attempt/success, total loaded count).

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
