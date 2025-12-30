# Gameplay tuning snapshot (Box2D control)

These values capture the tuned parameters after normalizing transforms for Box2D-driven movement and stackable sweets.

## Player movement
- Base horizontal speed: **10.0 units/s** (stage config `horizontalSpeedCellsPerSec` with cell size 1.0).
- Base vertical speed: **12.0 units/s** (`verticalSpeedCellsPerSec`).
- Speed boost step: **1.2x** per increment.
- Maximum speed multiplier: **2.0x** cap.
- Fallback/base move speed used when configs are missing or invalid: **10.0 units/s**.

These values are defined in `Assets/Config/stage_default.cfg`, `StageData` defaults, and the `SceneGame` runtime constants that drive debug boost controls.

## Sweets stacking
- Scales tightened to keep cross-sections compact for predictable contacts:
  - Cake: 0.26–0.32
  - Cream (Sherbet alias): 0.22–0.30
  - Strawberry: 0.20–0.28
- Densities reduced to lower mass and jitter risk while retaining frictional grip:
  - Cake: 1.2 (Box)
  - Cream/Sherbet: 0.75 (Circle)
  - Defaults/others: 0.9

Configuration lives in `Assets/Config/gameplay.cfg` (`colliders.sweets.factory` for scales/padding and
`colliders.registry` for densities/materials) plus `SweetsManager::BuildBaseShapeConfig`.
