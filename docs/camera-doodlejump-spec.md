1. Camera follows player upward with **camera only up** behavior; never tracks downwards once advanced.
2. Establish a height **threshold** that triggers camera advancement when the player passes it.
3. Maintain a **death line** below the camera that kills the player if crossed.
4. Use a **fixed despawn line (configurable)** below the camera to remove old platforms and entities.
5. Ensure **view-matrix drives motion** for camera-relative movement and parallax adjustments.
6. Allow **background screen-space ok** rendering that ignores world Z for the backdrop.
7. Camera advance rate should be smooth and clamped to prevent sudden jumps.
8. Threshold offsets should be configurable per stage or difficulty.
9. Despawned entities must be safely removed from physics and render systems.
10. Debug visualizers should show threshold, death line, and despawn line for tuning.
