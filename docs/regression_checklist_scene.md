# Checklist scene regression notes

This checklist uses `Assets/Config/stage_checklist.cfg`, which positions the platform so its top sits at world Y=0 and aligns spawn anchors with that surface. The scene intentionally mirrors the default gameplay layout while exposing easier visual checks for collider/pivot tweaks.

## Quick facts
- Stage config order: try `stage_checklist.cfg`, fall back to `stage_default.cfg` if missing.
- Ground baseline: `SceneGame` logs `[Ground]` entries with the platform tag and top Y so testers can confirm the rendered plate matches the physics floor.
- Spawn anchors: primary uses `spawnPoints` (defaults only when omitted), secondary is optional via `spawnPointsSecondary`/`spawnPointsB` and is never auto-filled; SweetsManager alternates only when both lanes exist.

## Regression checklist
| Status | Area | What to check | Evidence/notes |
| --- | --- | --- | --- |
| ✅ | Ground alignment | Top of the platform and the computed `groundY` log line match (expected top at **Y=0**). Verify the `[Ground]` debug trace during scene initialization. |
| ✅ | Walk-through path | Player can traverse the plate from the spawn anchors without hitting the previous invisible line because spawns and ground share the same Y/Z plane. |
| ✅ | Debug overlay toggle | Turning the debug/telemetry overlay off/on does not change Box2D contacts or movement speed; physics values remain stable in logs before and after toggling. |
| ✅ | Pivot vs. collider | Visual pivot adjustments on the platform do not move its collider; collider dimensions follow the descriptor scale while the pivot only shifts rendering. |

Use this table as regression evidence after running the scene; update the notes column when a run reveals differences or confirms parity.
