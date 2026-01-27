# Power-up effect mapping

| Power-up key | Effect type | Levels (0-5) |
| --- | --- | --- |
| `SpeedUp` | Additive % (base stat) | +0/10/20/30/40/50% |
| `SweetsSpawnSpeed` | Additive % (base stat) | +0/10/20/30/40/50% |
| `SweetsSpawnQuantity` | Additive absolute | +0/2/3/4/5/6 |
| `PointAmount` | Multiplicative final | ×1.0/1.1/1.2/1.4/1.6/1.8 |
| `Whip` | Feature toggle | Lv0=off, Lv1+=on |

Notes:
- Additive % values are stored as ratios (e.g. 0.10 = +10%).
- Feature toggles use `enabled=true` when level >= 1.

## SaveData + PowerUpEffects

### Storage format (PlayerData)

`PlayerData::SaveData()` writes the player progression snapshot to `Assets/Data/PlayerData.cfg` using an INI-like layout. The only parsed section is `[PlayerData]`, and each `entry=` line follows this CSV shape: `currency,<PowerUpKey>=<Level>,<PowerUpKey>=<Level>,...`. The first value is the stored currency/points (`nPoints`), and the rest are power-up level pairs. Loading requires at least one power-up level entry after the currency value. `PlayerData::SaveData()` always writes power-up keys in the same order as `PowerUpEffectConfig::Entries()` (the order in `[powerup.effects]` of `Assets/Config/gameplay.cfg`).【F:HEWGame/Game/Save/PlayerData.h†L12-L37】【F:HEWGame/Game/Save/PlayerData.cpp†L97-L217】【F:HEWGame/Game/Save/PlayerData.cpp†L245-L269】【F:HEWGame/Game/PowerUp/PowerUpEffectConfig.cpp†L239-L274】【F:HEWGame/Assets/Config/gameplay.cfg†L111-L119】

### Fields derived from effects

When loading, `PlayerData` resets the derived fields and applies effects in the order returned by `PowerUpEffectConfig::Entries()` (the same order as the config). Each effect writes a dedicated field, so the order is deterministic but non-overlapping: `SpeedUp` → `fSpeed`, `SweetsSpawnSpeed` → `fReplenishSpeed`, `SweetsSpawnQuantity` → `nReplenishCount`, `PointAmount` → `fPtMultipl`, and `Whip` → `creamEnabled`. This matches the effect types configured under `[powerup.effects]` in `Assets/Config/gameplay.cfg`.【F:HEWGame/Game/Save/PlayerData.cpp†L51-L109】【F:HEWGame/Game/PowerUp/PowerUpEffectConfig.cpp†L237-L272】【F:HEWGame/Assets/Config/gameplay.cfg†L110-L117】
