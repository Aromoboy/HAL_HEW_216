# Camera Logging Reference

This document summarizes camera-related debug logs and whether they can appear every frame or only when an error/state change occurs.

## BaseCamera (`src/BaseCamera.cpp`)
- `[Camera] <label> invalid value=...` – emitted when position/rotation components exceed configured bounds; **error-only** and blocked by `_DEBUG` guards.
- `[Camera] invalid <label>=... (must be > 0)` – logged for invalid perspective parameters; **error-only**.
- `[Camera] SetTransform rejected invalid input. camera=...` – rejected transform assignment; **error-only**.
- `[Camera] priority out of range value=...` – priority magnitude exceeds limit; **error-only**.
- `[Camera] invalid frustum far=... must be greater than near=...` – bad near/far ordering; **error-only**.
- `[Camera] Transform mutated without marking dirty. caller=...` – detects transform edits without marking dirty; **error-only** and self-corrects after logging.

## CameraManager (`src/CameraManager.cpp`)
- `[CameraManager] Attempted to add null camera.` – null registration; **error-only**.
- `[CameraManager] Duplicate camera registration detected ptr=... duplicates=...` – duplicate add; **error-only**, counts repeats.
- `[CameraManager] Registered camera ptr=... total=...` – successful registration; **state change** (not per-frame).
- `[CameraManager] Clearing active camera.` – active camera cleared; **state change**.
- `[CameraManager] SetActive received unmanaged camera ptr=..., auto-registering.` – unmanaged camera passed to `SetActive`; **state change**.
- `[CameraManager] Active camera set ptr=...` – new active camera assigned; **state change**.
- `[CameraManager] No active camera set. framesWithoutActive=...` – first frame without an active camera; **per-frame guarded** (logs once until resolved).

## CameraRig (`src/CameraRig.cpp`)
- `[CameraRig] Translation lock enabled/disabled. target=... followSpeed=...` – toggling translation lock; **state change**.
- `[CameraRig] Yaw/Pitch lock enabled/disabled. target=... followSpeed=...` – toggling yaw/pitch lock; **state change**.
- `[CameraRig] Attach called. target=... camera=...` – rig attached to target; **state change**.
- `[CameraRig] Update skipped because camera is null.` – OnUpdate with no camera pointer; **per-frame** until camera assigned.
- `[CameraRig] Target expired. Freezing camera update. camera=...` – attached target expired; **per-frame guarded** (logs once per expiry period).

## Scene base (`src/Scene.cpp`)
- `[Scene] RootDraw detected missing active camera in scene ptr=...` – first draw without an active camera; **per-frame guarded** (once until camera set).

## TitleScene (`src/TitleScene.cpp`)
- `[TitleScene] Failed to register title camera; RootDraw may lack an active camera.` – registration failure; **error-only**.
- `[TitleScene] Title camera created/reused ... set active.` – camera created or reused and activated; **state change**.

## SceneGame (`src/SceneGame.cpp`)
- `[SceneGame] Draw skipped: no active camera registered.` – draw without active camera; **per-frame**.
- `[SceneGame] ConfigureCamera skipped: gameplay camera missing.` – configuration attempt with missing camera; **error-only**.
- `[SceneGame] Configured camera prototype: ...` (and `[CameraStatic] Final position ...`) – configuration details; **state change** when applying presets.
- `[SceneGame] [PlatformAlign] DefaultRTV missing; falling back to constant screen size.` – platform anchor recompute fallback; **error-only** and only when recomputation forced with missing render target.
- `[SceneGame] ResetPlaySession skipped anchor realignment: camera anchor missing.` – missing camera anchor during reset; **error-only**.

## DrawSystem (`src/Systems/DrawSystem.cpp`)
- `[DrawSystem] RenderQueued skipped: active camera missing.` – render pass invoked without camera; **per-frame guarded** (logs once per missing span).

## ModelRender (`src/ModelRender.cpp`)
- `[ModelRender] Draw aborted: camera pointer was null.` – draw request without camera; **per-frame** for each bad call.
