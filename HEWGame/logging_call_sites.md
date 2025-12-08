# Logging call sites

Summary of every `DebugTrace` logging call in the targeted files, including when they trigger and the intended severity level inferred from context.

## src/Scene.cpp
| Location | Message context | Severity | Trigger frequency / guards |
| --- | --- | --- | --- |
| L125-L132 | Warns when `RootDraw` finds no active camera and notes the scene pointer. | Warning | Emitted once per missing-camera period; `m_warnedAboutMissingCameraForDraw` suppresses repeats until a camera becomes active. |
| L223-L313 | Dumps the current UpdateScheduler execution plan with node details and missing phases. | Debug / informational | Logs only in debug builds when the execution log is non-empty and the size has changed since the last dump (`m_loggedExecutionPlan`). |
| L318-L327 | Reports a registration request for an `IUpdatable`, including phase and priority. | Debug / informational | Fires on each call to `Register` in debug builds. |
| L333-L342 | Reports an unregistration request for an `IUpdatable`, including phase and priority. | Debug / informational | Fires on each call to `Unregister` in debug builds. |

## src/Systems/PhysicsMachine.cpp
| Location | Message context | Severity | Trigger frequency / guards |
| --- | --- | --- | --- |
| L53-L59 | Notes a change to the physics system pointer (previous vs. new). | Debug / informational | Emitted on each `SetSystem` call in debug builds. |
| L73-L79 | Notes a change to the integrator pointer (previous vs. new). | Debug / informational | Emitted on each `SetIntegrator` call in debug builds. |
| L106-L109 | Flags duplicate body registration with the body pointer. | Warning | Emitted once per duplicate registration attempt; guarded by debug-only logging. |
| L123-L126 | Warns when attempting to unregister an unknown body pointer. | Warning | Emitted once per unknown-body unregistration attempt; debug-only logging. |
| L145-L149 | Warns that a physics system is missing while bodies are registered. | Warning | Emitted once per missing-system condition until a system is provided (`m_loggedMissingSystem`). |
| L154-L156 | Warns that no integrator is configured before the translation step. | Warning | Emitted once per missing-integrator condition until set (`m_loggedMissingIntegrator`). |
| L164-L165 | Warns about encountering a null body pointer during update. | Warning | Fires each time a null pointer is encountered; debug-only logging. |

## src/Systems/ContactLiteSystem.cpp
| Location | Message context | Severity | Trigger frequency / guards |
| --- | --- | --- | --- |
| L84-L85 | Normalized near-vertical collision normal snapped to Y axis for stability. | Debug / informational | Only in debug builds when the normal is snapped and has significant X/Z components. |
| L245-L267 | Body source pointer set (with optional count). | Debug / informational | Each `SetBodySource` call in debug builds. |
| L362-L369 | No body list available; contact resolution skipped. | Warning | Once per missing-body-list state (`m_loggedMissingBodyList`). |
| L374-L397 | Snapshot of active body list with tags. | Debug / informational | First `OnUpdate` after body source set; repeats when reset (`m_loggedBodySnapshot`). |
| L399-L411 | Player body present in solver input; avatar collisions enabled. | Info | On first detection after absence; resets when player absent (`m_loggedPlayerPresence`). |
| L413-L420 | Player body missing; avatar collisions disabled. | Warning | On first absence after presence; resets when player returns (`m_loggedPlayerAbsence`). |
| L464-L472 | Kinematic player body skipped because filtering disallows it. | Info | Once per filtered player body state (`m_loggedKinematicPlayerFilter`). |
| L483-L489 | Kinematic non-player body filtered from contact pass. | Info | Once per filtered kinematic non-player state (`m_loggedKinematicFilter`). |
| L565-L569 | Player contacted the stage foundation. | Info | Logged once per session when detected (`m_loggedPlayerPlatformContact`). |
| L570-L576 | Player contacted a sweets body with resolved type. | Info | Logged once per session when detected (`m_loggedPlayerSweetsContact`). |
| L595-L609 | Collision skipped due to excessive Z separation for 3D pair. | Debug / warning | Logged once when condition first seen (`m_loggedPlanarZSeparation`). |
| L614-L633 | Proceeding with planar evaluation despite ignoring Z penetration or 2D pair. | Debug / informational | Logged once per session when condition first met (`m_loggedPlanarZIgnore`). |
| L671-L683 | SAT bypassed for shallow foundation contact; forced vertical normal. | Debug / informational | Occurs each time shallow foundation contact is handled (no suppression flag). |
| L718-L727 | SAT failed for sweets pair; AABB fallback skipped. | Debug / warning | Logged whenever SAT attempts fail under sweets preference (no suppression flag). |
| L829-L833 | Ground contact demoted due to slope limit. | Debug / warning | Logged whenever a would-be ground contact is rejected for slope (no suppression flag). |
| L893-L905 | Support analysis summary for sweets/foundation support pairs. | Debug / informational | Logged each time support analysis runs for qualifying pairs. |
| L912-L919 | Unstable support slide boost applied to the top sweets body. | Debug / informational | Logged each time boost is applied to an unstable support. |
| L957-L968 | Z translation requests clamped to zero to maintain planar constraint. | Debug / warning | Logged whenever non-zero Z translation is overridden. |
| L1044-L1060 | Velocity components clamped due to minimum speed thresholds. | Debug / informational | Logged each time clamping occurs. |
| L1087-L1094 | Restitution impulse scaled for low-speed contact. | Debug / informational | Logged when restitution scaling reduces impulse. |
| L1151-L1153 | Normal velocity clamped for body A due to static contact or unstable support. | Debug / informational | Logged whenever body A’s normal velocity is adjusted as described. |
| L1163-L1187 | Normal velocity clamped or damped for body B due to contact/support state. | Debug / informational | Logged whenever body B’s normal velocity is adjusted as described. |
| L1215-L1221 | Static friction applied for low tangential speed ground contact. | Debug / informational | Logged each time static friction sharing is used. |
| L1228-L1237 | Static friction suppressed because contact is steep or non-ground. | Debug / informational | Logged each time suppression occurs. |
| L1243-L1248 | Static friction bypassed because tangential speed exceeds threshold. | Debug / informational | Logged each time bypass occurs. |
| L1264-L1273 | Dynamic friction applied with computed scale. | Debug / informational | Logged each time dynamic friction is applied. |
| L1285-L1302 | Contact resolution summary for each resolved pair (method, normals, flags). | Debug / informational | Logged per resolved contact pair in debug builds. |
| L1305-L1325 | Post-contact Z velocity reset for bodies with non-zero Z velocity. | Debug / warning | Logged whenever Z velocity is zeroed after contact. |
| L1335-L1349 | Frame summary of resolved overlaps and solver parameters. | Debug / informational | Logged once per update when any overlaps were resolved. |

