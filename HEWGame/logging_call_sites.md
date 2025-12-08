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

