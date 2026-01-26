# Power-up effect mapping

| Power-up key | Effect type | Levels (0-5) |
| --- | --- | --- |
| `SpeedUp` | Additive % (base stat) | +0/10/20/30/40/50% |
| `SweetsSpawnSpeed` | Additive % (base stat) | +0/10/20/30/40/50% |
| `SweetsSpawnQuantity` | Additive absolute | +0/2/3/4/5/6 |
| `CakeSize` | Additive % (base stat) | +0/10/20/30/40/50% |
| `PointAmount` | Multiplicative final | ×1.0/1.1/1.2/1.4/1.6/1.8 |
| `Whip` | Feature toggle | Lv0=off, Lv1+=on |

Notes:
- Additive % values are stored as ratios (e.g. 0.10 = +10%).
- Feature toggles use `enabled=true` when level >= 1.
