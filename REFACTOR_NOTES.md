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

- 2026-11-20: dist2 made in the main folder (G:/Claude/VR Radial Overlay/dist2) from the build before the items below, for the user to try.
- 2026-11-20: game_input.h/.cpp: GameInputFilter (button/trigger/stick filtering, InputFeature), tests. layer.cpp ~760 lines before format.
- 2026-11-20: layer/.clang-format added and applied to layer src + tests (one statement per line; no code changes).
- 2026-11-20: engine.h private state grouped per feature: KeyboardRun kbRun, ScreenRun scr, OffsetsRun off (fields renamed).
- 2026-11-20: tests: test_support.h (CHECK, fails, SavePng), test_parts.cpp (game offsets, overlay, game input). 201 checks pass.

- 2026-11-20: layer.cpp globals grouped: OurActions gOurs, SessionState gSession, WatchedFiles gFiles.
- 2026-11-20: editor: .editorconfig + `dotnet format whitespace` (one statement per line); converter classes laid out. Editor smoke-started OK.
- 2026-11-20: enums instead of strings: ScreenPlace + ScreenSource (config.h, ScreenPlaceKey/ParseScreenPlace), ActMode
  (ParseActMode/ActModeKey; unknown text -> ActMode::Unknown, which keeps the old fall-through behaviour). Profile files unchanged.
- 2026-11-20: dist2 refreshed with all of the above.

- 2026-11-20: every profile setting the code compared as text is now an enum with a { value, "text" } table (EnumName,
  ParseEnum/EnumKey in config.h): WheelAnchor, SelectMethod, BlockWhileOpen, ArmSide, Gesture, ConfirmMode, SelectHand,
  BindingBlock, TextUp, PressMethod, LaserCorrection. Unknown text keeps the old behaviour (errors where it errored, else
  the old fall-through value). No raw-string setting comparisons left outside config.cpp. dist2 refreshed.

- 2026-11-20: user TESTED dist2 (refactor up to the enums): works. Merged into main (671ae67), local only.
- 2026-11-21: ENGINE SPLIT (in this branch, not merged yet): feature classes OffsetsPanel (offsets_panel.*), VrKeyboard
  (vr_keyboard.*), ScreenMirror (screen_mirror.*), WristPanels (wrist_panels.*). Each owns its state (run_) and logic,
  holds `Engine& e`, Engine lists them as friends and holds one of each. engine_types.h has the shared enums.
  Engine public section grouped by purpose. Done with tools/split_feature.py + tools/fix_loop.py (compile-driven).
  LESSON: after engine.h layout changes do a CLEAN build (rm -rf layer/build layer/build32): a stale layer.cpp object
  crashed the pretend-runtime test once. Tests now print unbuffered. dist2 refreshed 2026-11-21 00:08 (clean build).
  Needs a headset test, then merge into main.

## Status / resume here
Done: steps 1, 2, 4 (layer, via clang-format), 6 (editor file split), 7; most of 3 (files per feature + grouped state; still one Engine class);
part of 5 (support header + part tests; the engine-flow tests in test.cpp share one running engine, so they stay together).
Untested in a headset. dist2 refreshed 2026-11-20 with everything above (copy this dist again after new changes).
Possible next: Engine feature classes (Keyboard, ScreenMirror, OffsetsPanel) taking an input view (big: they share input, grab, toasts);
(string settings: done.)

## 2026-11-21 evening: AGENTS.md cleanup round (branch refactor, from main 16ac877)
Done while the user was away; untested in a headset. dist2 in the main folder = this build.
Commits (oldest first): mirror full-screen state into HeadsetMirror; keys.cpp tidy (InputWorker class, one
game-focus helper, sequences parsed once, DryRun struct, + key sequence test); ComPtr in mirror.cpp, panel.cpp/h,
screen.cpp (no manual Release left); screen Run -> RunMonitor + WaitForConfigChange; ParseProfile -> one reader per
section + shared ParseItemList + CheckReferences; WristPanels::Update -> visibility/sync/grab/poke/point;
ReadInputsAndUpdate -> ReadHand/SendHaptics/PushHandOffsets/save helpers/UpdateHeadsetMirror; OffsetsPanel::Update
-> grab-to-set/save/panel input; editor Settings pages one builder method each; editor ActionType/GestureKind
constants; 3 old editor warnings fixed.
Checks: layer tests pass except the 2 monitor-capture tests, which fail on unchanged main too while the screen is
locked (rerun when the user is at the PC: they cover the ComPtr change in screen.cpp). Editor --selftest output
byte-identical to main's; --shots of every page pixel-identical (apart from the install-status text).
Not done: clang-format (not installed on this PC); MainForm constructor (154 lines) left as is.
To test in the headset: wheel + wrist panels (poke, point, grab in adjust), offsets panel (grab to set, stick
slider), screen mirror (monitor AND window), headset mirror (incl. full screen), key sequences, a D3D12 game.
If OK: merge refactor into main (fast-forward), rebuild dist.
