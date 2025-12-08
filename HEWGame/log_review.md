# Log call audit

Notes on current logging usage. Each entry lists the message, logging API, rough trigger cadence (per-frame vs event), and a keep/remove suggestion for later cleanup.

## src/Core/IndexBuffer.cpp
- `[IndexBuffer] Unsupported index element size.` via `DebugMessage` when an invalid element size is provided during initialization (event). Keep for misuse diagnostics.
- `[IndexBuffer] Failed to create buffer.` via `DebugMessage` if buffer creation fails on init (event). Keep for failure visibility.
- `[IndexBuffer] Bind called with null buffer.` via `DebugMessage` when binding before initialization (event, could spam if misused). Consider keeping but gate to debug builds.

## src/Core/DXContext.cpp
- `[DXContext] Failed to create device and swap chain.` via `DebugMessage` on init failure (event). Keep.
- `[DXContext] Failed to create default render targets.` via `DebugMessage` on init failure (event). Keep.
- `[DXContext] Resize requested before swap chain creation.` via `DebugMessage` when resize called early (event). Keep.
- `[DXContext] Clearing render targets and shader resources prior to resize.` via `DebugMessage` each resize call before releasing resources (per-event, but could be noisy if resize spammed). Consider downgrading to verbose.
- `[DXContext] ResizeBuffers failed.` via `DebugMessage` when swap chain resize fails (event). Keep.
- `[DXContext] Failed to recreate default render targets after resize.` via `DebugMessage` on resize failure (event). Keep.
- `[DXContext] D3D11CreateDeviceAndSwapChain failed for all driver types.` via `DebugMessage` when device creation fails (event). Keep.
- `[DXContext] Failed to create default render target from swap chain.` via `DebugMessage` when RTV recreation fails (event). Keep.
- `[DXContext] Failed to create default depth stencil.` via `DebugMessage` when DSV creation fails (event). Keep.

## src/Core/VertexBuffer.cpp
- `[VertexBuffer] Failed to create buffer.` via `DebugMessage` on initialization failure (event). Keep.
- `[VertexBuffer] Bind called with null buffer.` via `DebugMessage` if bound before init (event, potential spam). Consider gating to debug only.

## src/MeshBuffer.cpp
- `[MeshBuffer] Write called before vertex buffer initialization.` via `DebugMessage` if Write() is used before buffers exist (event). Keep for misuse detection.

## src/ShaderCache.cpp
- `[ShaderCache] VF[_[̃[hɎs: ` via `DebugMessage` when shader load fails (event). Keep; message text is currently truncated/garbled and should be cleaned later.

## src/SoundSystem.cpp
- Multiple `DebugMessage` calls in constructor and wave loading routines for COM/XAudio initialization failures and WAV parsing errors (events). These fire only on setup or file load issues; keep but localize to debug to reduce noise.
- `SourceVoice̍쐬Ɏs܂B` via `DebugMessage` when voice creation fails before playback (event). Keep.

## src/ModelCache.cpp
- `MODELCACHE_VERBOSE` macro can emit `[モデルキャッシュ] ...` messages when enabled (debug-only verbose; currently compiled out). Keep macro for optional verbose tracing.

## src/TextureDX.cpp
- `[Texture2D] WICt@Ng[̍쐬Ɏs` via `DebugMessage` when WIC factory creation fails (event). Keep.
- `[Texture2D] eNX`\[X̍쐬Ɏs` via `DebugMessage` when texture creation fails (event). Keep.
- `[Texture2D] SRV̍쐬Ɏs` via `DebugMessage` when SRV creation fails (event). Keep.

## src/MeshDX.cpp
- `頂点バッファの作成に失敗しました` via `DebugMessage` on vertex buffer creation failure (event). Keep.
- `インデックスバッファの作成に失敗しました` via `DebugMessage` on index buffer creation failure (event). Keep.

## src/TestObject.cpp
- `<model key>が読み込めませんでした` via `DebugMessage` when model load fails in constructor (event). Keep.

## src/ShaderProgram.cpp
- `[Shader] _VF[_[t@CJ܂: <path>` via `DebugMessage` when vertex shader blob load fails (event). Keep.
- `[Shader] sNZVF[_[t@CJ܂: <path>` via `DebugMessage` when pixel shader blob load fails (event). Keep.

## src/TitleScene.cpp
- `DebugTrace` wrappers around `OutputDebugStringA` for lifecycle (`Construct`, `Load`, `Enter`, `Exit`) and camera registration status (events). Keep for scene transitions; not per-frame.

## src/SceneManager.cpp
- `DebugTrace` logs scene swap lifecycle (`RequestSwap`, `Exit`, `Load`, `Enter`) with scene pointers via `OutputDebugStringA` (events). Keep for transition audit; not per-frame.

## src/DrawMatrix.cpp
- `OutputDebugStringA` helper `DebugTrace` used to log matrix load failures (event). Keep; triggered on file I/O failure, not per-frame.

## src/AssimpModelLorder.cpp
- `[AssimpLoader] テクスチャロード失敗: <path>` via `OutputDebugStringA` when texture load fails (event). Keep.

## src/SweetsManager.cpp
- `OutputDebugStringA` `DebugTrace` helper used for spawn/debug messages; current calls are event-driven (e.g., initialization). Keep if needed for spawn diagnostics.

## src/CameraRig.cpp
- `OutputDebugStringA` `DebugTrace` used for rig initialization diagnostics (event). Keep or gate to debug.

## src/SweetsPrototypeManifest.cpp
- `OutputDebugStringA` `DebugTrace` logs manifest load state (event). Keep.

## src/Systems/IntegrateSystem.cpp
- `OutputDebugStringA` `DebugTrace` used for integrator setup logging (event). Keep.

## src/Systems/PhysicsMachine.cpp
- `OutputDebugStringA` `DebugTrace` for physics machine creation/config failures (event). Keep.

## src/Stage/StageData.cpp
- `OutputDebugStringA` `DebugTrace` for stage data load messages (event). Keep.

## src/Stage/FoundationPlatform.cpp
- `OutputDebugStringA` `DebugTrace` for foundation platform initialization (event). Keep.

## src/Collision2D.cpp
- `OutputDebugStringA` `DebugTrace` used for collision system initialization/status (event). Keep.

## src/CameraManager.cpp
- `OutputDebugStringA` `DebugTrace` for camera manager setup (event). Keep.

## src/GameFlow/PhaseManager.cpp
- `OutputDebugStringA` `DebugTrace` for phase manager lifecycle events (event). Keep.

## src/Entities/Player.cpp
- `OutputDebugStringA` `DebugTrace` for player initialization (event). Keep.

## src/Systems/DrawSystem.cpp
- `OutputDebugStringA` `DebugTrace` for draw system setup and shader binding (event). Keep.

## src/Entities/CollisionEntity.cpp
- `OutputDebugStringA` `DebugTrace` for collision entity creation (event). Keep.

## src/PerspectiveCamera.cpp
- `OutputDebugStringA` `DebugTrace` for camera setup (event). Keep.

## src/Core/InputMap.cpp
- `OutputDebugStringA` logs input map registration details (`Describe`, `DumpBindings`) on demand (event). Keep; not per-frame unless called every frame explicitly.

## src/Core/Gamepad.cpp
- `OutputDebugStringA` logs gamepad connection state on initialization (event). Keep.

## src/Core/UpdateScheduler.cpp
- `OutputDebugStringA` logs scheduler registration, ordering, and tick diagnostics (per-event unless tick logging is enabled). Tick logs inside `Tick` could be per-frame if left active; consider gating or removing runtime tick spam.

## src/Scene.cpp
- `OutputDebugStringA` `DebugTrace` for base scene lifecycle events (event). Keep.

## src/Input.cpp
- `OutputDebugStringA` warnings for Input initialization/state fallbacks (`UpdateContext`, `Initialize`, `GetKeyboardState` errors). Event-driven; keep.

## src/BackgroundRenderer.cpp
- `OutputDebugStringA` `DebugTrace` for background renderer setup (event). Keep.

## src/Collision/ConvexHullBuilder.cpp
- `OutputDebugStringA` `DebugTrace` for convex hull build issues (event). Keep.

## src/BaseCamera.cpp
- `OutputDebugStringA` `DebugTrace` for base camera setup (event). Keep.

## src/ColliderConfigLoader.cpp
- `OutputDebugStringA` `DebugTrace` for collider config loading errors (event). Keep.

## src/SelectScene.cpp
- `OutputDebugStringA` `DebugTrace` for select scene lifecycle events (event). Keep.

## src/ModelRender.cpp
- `OutputDebugStringA` `DebugTrace` for model render setup plus `[ModelRender] Texture bound.` informational log (per-draw if left enabled; candidate for removal or gating). Others are event-level.

## src/SceneGame.cpp
- Extensive `DebugTrace` (`OutputDebugStringA`) usage across initialization, configuration, spawning, drawing, and physics hooks. Many entries sit inside gameplay update/draw paths (per-frame when verbose toggles are left true), e.g., draw skips and object render traces. Others cover config load failures and setup events. Per-frame style traces should be candidates for removal or conditional gating; setup/error logs can remain.
- Also uses `std::cout` for padding/progress output around HUD formatting and flush (likely debug-only, could be per-frame in debug builds). Consider removing or guarding.

## src/DebugConsole.cpp
- `std::cout` logs console initialization message once on startup (event). Keep or guard.

## src/Startup.cpp
- `std::cout` prints FPS and processing time inside update loop (per-frame debug print). Candidate for removal or gated profiling flag to avoid console spam.

