# Cream adhesion (bond) system

## Source of truth: carried state
- The authoritative “player is carrying a sweets” flag is the **SweetsRegistry state** (`SweetsState::Carried`).
- Transitions into/out of `Carried` happen via **SweetsLifecycle**:
  - `TryMarkAsCarried` sets `SweetsState::Carried`.
  - `ReleaseCarriedSweets` returns to `SweetsState::Active`.
- `SweetsContacts::IsCreamCarried` consults the registry, so adhesion gating is always based on the current state (not historical flags like `WasEverCarried`).

## Contact truth (when contacts are evaluated)
- Box2D contacts are processed **after the physics step** in `PhysicsMachine::ContactDispatchNode` (PostPhysics).
- `SceneGameContactHandler` forwards begin/end/lost/hit callbacks to `SweetsManager`, which updates `SweetsContacts`.
- Adhesion candidates are created from those PostPhysics contact events, and the candidate timer is advanced each update tick.

## States + transitions
```
No contact
  └─(contact begin)→ Candidate (timeTouching increments)
      ├─(break contact)→ Candidate (grace timer starts)
      ├─(touching >= adhesionDelaySeconds)→ Bond (weld joint)
      └─(grace expires)→ Candidate removed

Bond
  ├─(breakImpulseThreshold hit or manual release)→ Cooldown
  └─(contact lost)→ Cooldown

Cooldown
  └─(cooldownRemaining == 0)→ Bond record cleaned up
```

### Rules
- **New bonds are created only if `requireCarried=true` and the cream is currently `Carried`.**
- **Existing bonds are not removed** just because carried state changes; bonds only break via explicit release or impulses/contact loss.
- Targets must pass `allowedTags`/`blockedTags` checks and be valid (sweets or stage cake).

## Config surface (`Assets/Config/gameplay.cfg` → `[cream]`)
| Key | Purpose |
| --- | --- |
| `adhesionDelaySeconds` | Time to hold contact before creating a bond. |
| `adhesionGraceSeconds` | Grace window to restore contact without losing candidate. |
| `releaseCooldownSeconds` | Cooldown after bond break/release. |
| `breakImpulseThreshold` | If > 0, impulse to break a bond. |
| `requireSupport` | Require supporting contact (foundation/stage/sweets). |
| `requireCarried` | Gate bond creation by carried state. |
| `allowAirAdhesionToSweets` | Allow air adhesion when both sides are sweets and delay elapsed. |
| `allowedTags`/`blockedTags` | Tag-based allow/block list. |

## Debug overlay + counters
- Toggle debug overlay with **F5** (same as `r.DebugDrawEnabled`).
- **Bond lines**: green for active bonds; red for cooldown/invalid joint.
- **Candidate lines**: blue for active candidates; yellow when in grace (contact lost but within grace).
- **Cooldown bar**: short red bar above the cream showing remaining cooldown.
- **Reject counters** (logged every ~1s while overlay is enabled):
  - `blockedTag`, `notCarried`, `notAllowed`, `alreadyBonded`, `cooldown`, `targetNotSupportedType`.

### “Why didn’t it stick?”
1. Check the reject counters log: `[CreamAdhesionRejects]`.
2. Confirm the cream is in `SweetsState::Carried` (via pickup flow).
3. Verify the target passes allow/block tags and is a supported type (sweets or stage cake).
4. Watch candidate/grace/cooldown state in the debug overlay.
