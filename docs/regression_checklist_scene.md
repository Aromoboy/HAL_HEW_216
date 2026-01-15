# Checklist scene regression notes

This checklist uses the `stage.checklist` section in `Assets/Config/gameplay.cfg`, which positions the platform so its top sits at world Y=0 and aligns spawn anchors with that surface. The scene intentionally mirrors the default gameplay layout while exposing easier visual checks for collider/pivot tweaks.

## Quick facts
- Stage config order: try `stage.checklist`, fall back to `stage.default` if missing (both in `Assets/Config/gameplay.cfg`).
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

## Cream adhesion scenarios (manual dev checks)
- **Spawn drop (not carried) → no adhesion**: Let a cream spawn and fall without carrying it. It must not create a bond, and the reject counter should increment `notCarried`.
- **Carried + contact ≥ delay → adhesion**: Pick up cream (state becomes `Carried`), hold it against a valid target for at least `adhesionDelaySeconds`, and confirm a bond forms.
- **Short separation < grace → still candidate**: While carried, briefly break contact and restore within `adhesionGraceSeconds`; candidate should persist.
- **Blocked tags → no adhesion**: Contact `osara`/`fairy` or other blocked tags; ensure no bond, and `blockedTag` increases.
- **Break impulse threshold** (if enabled): Apply a hit over `breakImpulseThreshold` and confirm the bond breaks and cooldown starts.
