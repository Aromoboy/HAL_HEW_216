# Render and logging surfaces

This file records where the game emits on-screen overlays or logging output, grouped by whether they are per-frame overlays or error/diagnostic cases.

## Per-frame or recurring overlays
- **SceneGame telemetry overlay**: In debug builds, `SceneGame::RenderRunTelemetryOverlay` writes timer, height, score, and carry status strings to the debug console every frame using `DebugConsole::SetDrawPos` and `std::cout`.【F:src/SceneGame.cpp†L3249-L3317】【F:src/SceneGame.cpp†L2461-L2490】
- **Startup FPS counter**: In debug builds, `WinMain` writes FPS and processing time once per second through the debug console when the main loop advances. This is gated by the `_DEBUG` build guard around the `std::cout` writes and the debug console cursor positioning.【F:src/Startup.cpp†L126-L133】

## Error, warning, and diagnostic logging
- **Window creation failures**: `WinMain` shows a modal `MessageBox` when registering the window class fails. General error popups now go through `DebugMessage`, which writes to the debugger output stream and stdout without a modal dialog.【F:src/Startup.cpp†L38-L43】【F:src/Debug.cpp†L22-L37】
- **Scene draw preconditions**: `Scene::RootDraw` logs once via `DebugTrace` when no active camera is set during drawing, preventing repeated spam while the issue persists.【F:src/Scene.cpp†L120-L139】
- **Draw system safeguards**:
  - `DrawSystem::RenderQueued` warns once if either the renderer or active camera is missing before attempting to render.【F:Engine/Systems/Draw/DrawSystem.cpp†L267-L330】
  - `DrawSystem::ValidateTransform` emits a debug trace when an entity carries non-finite transform data.【F:Engine/Systems/Draw/DrawSystem.cpp†L294-L308】
  - When debug logging is enabled, `RenderEntity` and `RenderCollision` report empty command queues and summarize submitted draw counts, collider shapes, and AABB fallbacks.【F:Engine/Systems/Draw/DrawSystem.cpp†L348-L388】【F:Engine/Systems/Draw/DrawSystem.cpp†L454-L468】
  - Entities without models are recorded once through `LogMissingModel` to avoid repeated noise.【F:Engine/Systems/Draw/DrawSystem.cpp†L332-L345】
- **Model renderer fallbacks**: `ModelRender::Draw` logs null cameras, expired mesh resources, and missing materials (including the default-material fallback). In debug builds it also emits a trace when a texture is bound during rendering.【F:src/ModelRender.cpp†L40-L96】
- **Background renderer readiness**: Initialization logs missing texture paths, texture load failures, and the configuration used when initialization succeeds. Texture allocation failures during load are also reported.【F:src/BackgroundRenderer.cpp†L43-L116】
- **Score telemetry**: `SceneGame::ComputeScore` issues a debug trace whenever the computed score or multiplier changes, providing peak height and multiplier details. This runs as part of per-frame HUD preparation but only logs on value changes.【F:src/SceneGame.cpp†L3324-L3349】
- **Debug console lifecycle**: Debug console initialization prints a startup line to `stdout`, providing visibility that the console was attached. Runtime visibility toggles do not log.【F:src/DebugConsole.cpp†L8-L45】
