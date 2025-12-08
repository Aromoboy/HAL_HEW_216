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
| L115-L120 | Notes a change to the physics system pointer (previous vs. new). | Debug / informational | Emitted on each `SetSystem` call in debug builds. |
| L208-L212 | Flags duplicate body registration with the body pointer. | Warning | Emitted once per duplicate registration attempt; guarded by debug-only logging. |
| L223-L226 | Notes registration of a pre-created Box2D body for the entity tag. | Debug / informational | Debug-only logging on each registration of an existing Box2D body. |
| L243-L245 | Reports failure to create a Box2D body during registration, including the entity tag. | Warning | Debug-only logging when body creation fails; followed by an assertion. |
| L258-L260 | Warns when attempting to unregister an unknown body pointer. | Warning | Emitted once per unknown-body unregistration attempt; debug-only logging. |
| L292-L296 | Warns that the Box2D world is invalid during sync and skips syncing. | Warning | Emitted once per invalid-world condition until cleared (`m_loggedMissingWorld`). |
| L304-L306 | Warns about encountering a null body pointer during sync. | Warning | Fires each time a null pointer is encountered; debug-only logging. |
| L315-L319 | Reports that a registered entity is missing a Box2D body during sync. | Warning | Debug-only logging for each missing-body occurrence; paired with an assertion. |
| L334-L337 | Warns that a physics system is missing before stepping. | Warning | Emitted once per missing-system condition until a system is provided (`m_loggedMissingSystem`). |
| L343-L347 | Notes the first physics tick with delta time and registered body count. | Debug / informational | Debug-only logging emitted once at first tick (`m_loggedFirstTick`). |
| L356-L357 | Warns that the Box2D world is invalid before stepping and skips the step. | Warning | Emitted once per invalid-world condition until cleared (`m_loggedMissingWorld`). |
| L368-L369 | Warns about encountering a null body pointer before stepping. | Warning | Fires each time a null pointer is encountered; debug-only logging. |
| L381-L384 | Reports failure to create a Box2D body before stepping, including the entity tag. | Warning | Debug-only logging for each failure; followed by an assertion. |
| L406-L406 | Warns that the Box2D world is invalid when creating a body. | Warning | Emitted once per invalid-world condition until cleared (`m_loggedMissingWorld`). |
| L567-L567 | Warns when `b2CreateBody` returns an invalid ID. | Warning | Logs on each failure to create a Box2D body. |
| L688-L692 | Notes mesh fixture creation for a body, including polygon and boundary counts. | Debug / informational | Debug-only logging after creating fixtures from mesh polygons. |

