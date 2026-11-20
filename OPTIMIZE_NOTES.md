# OPTIMIZE BUILD - RESUME HERE
This folder (G:/Claude/OpenXR Companion - Optimize) is a git worktree of G:/Claude/VR Radial Overlay on branch `optimize`.
Base commit 32028ef = test build g (all uncommitted work since v0.2.8). Local only, never push.
Goal (user, 2026-11-20): whole-program review for weaknesses, optimizations and anything that could disturb
tracking (Fallout tester: body twisting). Separate build + zip (release/OpenXRCompanion-optimize-<date>.zip here);
the main folder stays the testing version. Each fix = its own local commit on `optimize`.

## Progress (newest last)
- [x] worktree + baseline commit
- [x] third_party/openxr-sdk copied in from the main folder (submodule not fetched in the worktree)
- [x] FIX layer.cpp Hook_xrEndFrame: gMx released before N.EndFrame (was held while the runtime waits; stalled game input thread)
- [x] FIX common.cpp: DataDir()/ExeName() cached (were SHGetKnownFolderPath + migration checks + CreateDirectory per call, ~1/s x many from xrSyncActions)
- [x] TrackDiag log lines capped (8 per pose per 10 s)
- tests: 0 failures after each step
Reviewed so far: layer.cpp (hooks, locate, endframe), common.cpp.
Findings NOT fixed yet (decide/do next):
  * offsets ignore the game's poseInActionSpace: pose' = located*O, so the offset turns about the game's adjusted frame, not the controller. Correct: located*inv(Pg)*O*Pg (store Pg per space in Hook_xrCreateActionSpace).
  * a pose action bound to grip on one profile and aim on another: gPoseBinds takes the first kind -> wrong offset kind. Could key by current interaction profile.
  * our action set priority = min of the game's sets at OUR creation time; a game set created later with lower priority would be outranked by ours (OpenXR: lower set's same inputs inactive -> could make the game's hand pose/buttons inactive). Safer: create ours at attach time (all sets exist by then).
  * steadying leaves velocities raw (pose smoothed, velocity not) - minor.
Next to review: engine.cpp, panel.cpp (CPU raster per redraw), mirror.cpp (D3D on game context), screen.cpp capture thread, config.cpp, editor.
- [x] FIX config.cpp:445 notice path (missing backslash -> '<data>\profiles.defaults_moved_notice'; the user's data folder has that stray file, harmless)
- [x] FIX editor ActiveProfile setter writes active_profile.txt only when it changes (was every refresh -> running game reloaded profile, menus closed)
- [x] log the game's own poseInActionSpace offset (decides whether the offsets-about-controller fix matters)
- [x] FIX D3D12 Panel::Upload: no CPU wait on the game's queue (ring of 3 allocators/lists in Gfx, 3 staging buffers per panel); new WARP test (10 uploads, readback OK, 0.05 ms/upload). Possibly behind the Grimlord D3D12 fps drop.
- [x] screen TakeFrame swaps instead of copying 8 MB per frame
Reviewed OK (no change): engine.cpp per-frame logic, mirror.cpp (fps-limited, SRV cache, SwapDeviceContextState, Present DO_NOT_WAIT), screen capture thread.
Next: config.cpp parse/save robustness, keys.cpp (SendInput/focus), wrist/keyboard/offsets per-frame, editor (Updater, file writes), then build zip.
