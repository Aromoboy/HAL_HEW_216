# Logging sites and trigger conditions

## src/Stage/StageData.cpp
- **Parsing helpers**: `ParseFloatList` logs when a comma-separated float token cannot be parsed and aborts list creation; `TryParseSpawnPoint` logs malformed spawn points (too few values or not grouped in triples). These messages appear during stage config load.
- **Color sanitization**: `SanitizeColor` logs if incoming color components are non-finite (fallback used) or if values are clamped into the 0–1 range.
- **File loading**: `LoadFromFile` logs when the stage config file is missing, when a line lacks an '=' delimiter, or when an unknown key is encountered while parsing.
- **Missing values**: After parsing, defaults are logged for unset fields such as plane Z, platform geometry/fractions, movement speeds, guideline and bonus heights, and background texture.
- **Value sanitization**: Fraction and depth sanitizers log when values are invalid, interpreted as percentages, or clamped to safe ranges.
- **Summary output**: `ApplyDefaults` and successful/derived loads call `LogSummary`, which prints aggregate settings and per-platform summaries.
- **Reward multipliers**: `EnsureRewardMultiplierCoverage` logs when invalid multipliers are filtered or when the multiplier list is expanded/truncated to match phase counts.

## src/Stage/FoundationPlatform.cpp
- Constructor logs when the foundation platform instance is initialized after syncing colliders and loading its model.
- `SetAnchorTransform` logs anchor transform updates in debug builds whenever the anchor transform changes.
- `SetDebugColor` logs the RGBA color applied to the model materials (and notes if the model is not yet loaded).
- `LoadPlatformModel` logs failure to load the base model asset and logs the clone statistics once the model is loaded successfully.

## src/SceneManager.cpp
- `RequestSceneChange` logs scene swap requests with the pending scene pointer.
- `ActivatePendingScene` logs lifecycle transitions (`Exit`, `Load`, `Enter`) with pointers and RTTI names during scene activation.

## src/SelectScene.cpp
- Lifecycle events (`Load`, `Enter`, `Exit`) are logged with the scene pointer to trace scene transitions.

## src/Systems/DrawSystem.cpp
- Registration helpers log duplicate registrations or unregister attempts for unknown entities in debug builds.
- `OnUpdate` logs queued render commands (transform details) when debug logging is enabled; `ValidateTransform` logs and rejects entities with non-finite transforms.
- Rendering pipeline logs missing renderer/camera only once per session; missing-model warnings are emitted the first time an entity lacks an assigned model.
- `RenderQueued`/`RenderCollision` emit debug logs when called with no commands and report counts of submitted draw and collider commands (including compound child and fallback counts) when debug logging is enabled.
