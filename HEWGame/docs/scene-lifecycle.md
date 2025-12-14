# Scene lifecycle and transition rules

All scenes are expected to follow the same initialization and teardown sequence managed by `SceneSpace::SceneManager`:

1. `Load(Core::Input&)`
2. `OnEnter()`
3. `Update(Core::StepTimer&, Core::Input&)` / `Draw()`
4. `OnExit()` (before the scene is replaced)

`Update` and `Draw` must only run after both `Load` and `OnEnter` have completed. `SceneManager` enforces this ordering; scene code should assume it as the single source of truth.

### Transition rules

* The only supported entry point for scenes is `SceneManager::SetInitialScene`.
* Runtime transitions must go through `SceneManager::RequestSceneChange`.
* Avoid keeping global scene pointers or constructing scenes outside of these APIs; doing so bypasses lifecycle guarantees and `OnExit` notifications.

Any new scene should derive from `SceneSpace::IScene` and rely on the manager to control activation, adhering to the ordering above.

