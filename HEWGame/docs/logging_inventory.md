# Logging inventory and lifecycle candidates

Summary of notable debug log messages in core runtime systems, with frequency expectations and whether they should be preserved as lifecycle signals (initialization/registration/health) versus transient debug chatter.

## Scene scheduler

| Location | Message intent | Frequency guard | Lifecycle signal? |
| --- | --- | --- | --- |
| `Scene::LogExecutionPlan` | Dumps ordered UpdateScheduler plan when size changes or first seen. | Guarded by `m_loggedExecutionPlan`/size check; fires once per plan change. | Yes — useful to confirm scheduler composition after registration changes.【F:src/Scene.cpp†L223-L346】 |
| `Scene::Register` / `Scene::Unregister` | Logs every request to add/remove update nodes. | None; emits per call (potentially noisy inside bulk registration). | No — per-call debug noise, not lifecycle status.【F:src/Scene.cpp†L318-L346】 |

## Physics contact solver

| Location | Message intent | Frequency guard | Lifecycle signal? |
| --- | --- | --- | --- |
| Missing body list | Warns when `OnUpdate` has no body source configured. | Single-shot via `m_loggedMissingBodyList`. | Yes — highlights misconfiguration preventing collision resolution.【F:src/Systems/ContactLiteSystem.cpp†L362-L369】 |
| Kinematic filters | Notes when kinematic bodies (player or others) are skipped due to configuration. | Single-shot flags per category. | No — configuration debug; not critical lifecycle info.【F:src/Systems/ContactLiteSystem.cpp†L464-L489】 |
| Body snapshot | First pass dump of active bodies/tags. | One-time via `m_loggedBodySnapshot`. | No — diagnostic snapshot only.【F:src/Systems/ContactLiteSystem.cpp†L374-L397】 |
| Player presence/absence | Announces whether player body is in contact set. | Toggles between single-shot presence/absence flags. | Yes — indicates whether avatar collisions are enabled.【F:src/Systems/ContactLiteSystem.cpp†L399-L420】 |
| Player contact confirmations | Logs first player contact with platform or sweets. | One-time flags per contact type. | No — gameplay instrumentation.【F:src/Systems/ContactLiteSystem.cpp†L565-L576】 |
| Planar Z checks | First occurrence of excessive Z separation or ignored Z overlap. | One-time flags for separation/ignore cases. | No — geometry debugging.【F:src/Systems/ContactLiteSystem.cpp†L595-L633】 |
| SAT bypass / failure notes | Reports SAT bypass for shallow foundation contact, or failed SAT for sweets pairs. | No guard; can trigger per-collision pair inside the nested loop. | No — high-chatter debug; consider gating if noise is an issue.【F:src/Systems/ContactLiteSystem.cpp†L654-L734】 |

## Gravity damping

| Location | Message intent | Frequency guard | Lifecycle signal? |
| --- | --- | --- | --- |
| Kinematic skip | Notes when a kinematic entity is skipped and zeroed. | Per-entity single-shot via `m_loggedKinematicSkips`. | No — per-entity debug.【F:src/GravityDampingSystem.cpp†L36-L57】 |
| Planar violation | Warns when Z motion is found and clamped. | Per-entity single-shot via `m_loggedPlanarViolations`. | No — per-entity debug.【F:src/GravityDampingSystem.cpp†L59-L79】 |

## SceneGame lifecycle checks

| Location | Message intent | Frequency guard | Lifecycle signal? |
| --- | --- | --- | --- |
| Player collider build | Detailed collider parameters at creation; minimal variant for non-2D colliders. | Runs during player setup (once per spawn). | Yes — captures collider configuration for lifecycle/investigation.【F:src/SceneGame.cpp†L1822-L1855】 |
| Collider debug nudge | Reports manual collider nudges via debug input. | Per-input trigger (loop potential). | No — interactive debug only.【F:src/SceneGame.cpp†L2273-L2308】 |
| StageLayout load | Success/failure when opening stage CSV. | Once per load attempt. | Yes — shows resource availability during level init.【F:src/SceneGame.cpp†L2312-L2320】 |
| Player registration validation | Logs recovery when player is missing from physics/contact lists and confirmation afterward. | Single-shot flags reset after recovery. | Yes — indicates health of player registration across systems.【F:src/SceneGame.cpp†L2740-L2794】 |

## Sweets manager

| Location | Message intent | Frequency guard | Lifecycle signal? |
| --- | --- | --- | --- |
| Missing system warnings (scheduler/physics/draw) during unregister | Warns when cleanup cannot reach a subsystem. | One-time per subsystem via `m_warnedMissing*`. | Yes — highlights lifecycle teardown gaps.【F:src/SweetsManager.cpp†L2342-L2384】 |

### Lifecycle retention candidates

- Scene scheduler plan dump and player registration/StageLayout loader logs should be retained as lifecycle breadcrumbs for initialization and recovery flows.
- Missing body list warnings in `ContactLiteSystem` and missing subsystem warnings in `SweetsManager` surface critical configuration issues and should remain.
- Player collider construction logs document collider parameters at spawn time and are useful for lifecycle tracing.
