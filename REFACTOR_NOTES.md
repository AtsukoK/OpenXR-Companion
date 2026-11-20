# Refactor build (branch `refactor`, folder `G:\Claude\OpenXR Companion - Refactor`)

Goal: same behaviour as v0.2.9, structured the way a human developer would expect: named types instead of
magic numbers, one job per file, features as their own classes, readable lines. Nothing here is released;
the main folder (G:\Claude\VR Radial Overlay) stays the shipping version. Merge back only after a headset test.

Base: v0.2.9 (589fb68). Build: `powershell -NoProfile -ExecutionPolicy Bypass -File build_all.ps1` from bash
(dist goes to this folder's own dist, not registered with OpenXR). Tests: `layer/build/vro_test.exe` must say 0 failures
after every step. Commit after every step.

## Plan
1. Named values instead of magic numbers: grab targets, keyboard bar buttons, screen bar/picker modes,
   mirror requests, hand indices, offset selections.
2. Split layer.cpp (1465 lines) by job: OpenXR hooks / dispatch, head smoothing, tracking diagnostics,
   settings reload + saving, graphics (panel submit).
3. Engine: move each feature's state + logic into its own class (Keyboard, Dock, ScreenMirror UI, Offsets panel,
   Wrist panels, Wheel, Adjust/grab), Engine keeps input + coordination. Shared input view passed in.
4. Shorter, readable lines: one statement per line where code is dense; comments above blocks.
5. Tests split per feature (test/ folder, one file each, shared helpers).
6. Editor: MainForm.cs (1461 lines) split into partial classes / page classes; Editors.cs per editor.
7. Write docs/ARCHITECTURE.md: modules, threads and locks (gMx, gOffMx, gHeadMx: who takes which).

## Progress
(newest last)
- 2026-11-20: step 1 done: GrabTarget, ScreenBarButton, PickerMode, KbBarButton, MirrorRequest (engine.h), DockMode (config.h) replace numbers.
- 2026-11-20: quick_test.sh = build 64-bit layer + run tests (use it after every step; the full build_all.ps1 also builds x86 + editor).
- 2026-11-20: step 2 started: tracking_log.h/.cpp (class TrackingLog), head_smoothing.h/.cpp (class HeadSmoother, owns its lock; the old gHeadMx is gone).
  layer.cpp 1465 -> 1242 lines. Head smoothing is covered by the existing "real layer over a pretend runtime" tests.
  Next in step 2: game hand offsets (OffsetLocation, gPoseBinds, gGameSpaces, gSpaceSmooth, gOff, gOffMx) -> class GameHandOffsets;
  then the EndFrame drawing (Hook_xrEndFrame ~330 lines) -> compositor/overlay file; then input hooks (Evaluate, GetActionState*).
- 2026-11-20: game_offsets.h/.cpp (class GameHandOffsets owns gOffMx's job + the TrackingLog; PoseKind enum; tests added).
- 2026-11-20: overlay.h/.cpp: OverlayRenderer draws everything (screen + bar + picker, wheel, keyboard + bar, wrists + cursors,
  offsets panel, pointers, fingertips, toast); OverlayQuad = swapchain + drawn version + quad. Screen source picking
  (window by name/step, picker list, remembering the choice) lives there too (UpdateScreenSource, FillScreenChoices).
  New test renders through it on a WARP D3D11 device with pretend swapchains. layer.cpp now 868 lines.
  NOT yet tried in a headset. Next: input hooks (Evaluate/GetActionState*) + action set setup -> own file; then Engine split (step 3).
- 2026-11-20: docs/ARCHITECTURE.md written (modules, threads, locks, tests).
- 2026-11-20: engine.cpp split by feature: screen_ui.cpp (mirrored screen), wheel.cpp, adjust.cpp; engine.cpp 1080 -> ~450 lines.
  Still one Engine class (engine.h); step 3 proper (feature classes) not done.
- 2026-11-20: editor split: MainForm.cs -> MainForm.{Editor,List,Files,DragDrop,Pages,Helpers,Install}.cs + NewProfileDialog.cs;
  Editors.cs -> Choices, FormControls, Theme, EditorBase, KeyBox, ActionEditor, BindingEditor, WheelPreview, WheelEditor.
  Editor builds and opens (6 s smoke start). Full build_all OK, tests 0 failures.

## Status / resume here
Done: steps 1, 2 (layer.cpp 1465 -> ~870), 6 (file split), 7. Untested in a headset.
Left: step 3 (Engine feature classes: group private state per feature in engine.h, e.g. KeyboardRun/ScreenRun/OffsetsRun,
then classes with an input view), step 4 (dense one-liners: config.cpp, panel.cpp, keyboard.cpp, wrist.cpp, offsets.cpp,
input hooks in layer.cpp), step 5 (test.cpp ~1400 lines -> per-feature files with shared helpers),
layer.cpp input hooks (Evaluate / GetActionState* / EnsureActions) -> game_input.cpp.
To try it: dist in this folder is NOT registered; copy the dist DLLs over G:/Claude/VR Radial Overlay/dist (both x86 and 64)
or register this dist with the editor's Install layer, after the user agrees.
