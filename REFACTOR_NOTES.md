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
