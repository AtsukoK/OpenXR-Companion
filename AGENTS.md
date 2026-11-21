# AGENTS.md

Guidance for AI coding agents (Claude Code, Codex and others) working on OpenXR Companion.
Read `docs/ARCHITECTURE.md` for how the code fits together; this file is about how to work on it.

## What this is
- `layer/`: an OpenXR API layer DLL (C++, CMake, MSVC), built 64-bit and 32-bit (32-bit for Fallout NV / TTWVR).
- `editor/`: the settings editor (C# WinForms, .NET), shipped as one self-contained exe.
- `package/`: the layer manifests and install scripts that go into the release zip.
- The layer and editor only talk through files in `%LOCALAPPDATA%\OpenXRCompanion` (profiles, settings, logs).

## Build and test
- Everything into `dist/`: `powershell -NoProfile -ExecutionPolicy Bypass -File build_all.ps1`
- Layer + tests only (fast): `sh quick_test.sh` must end with `0 failures`. Run it after every change.
- After changing a header's layout (`engine.h`, `config.h`), do a clean build (`rm -rf layer/build layer/build32`):
  a stale object once crashed the tests.
- If copying into `dist/` fails, a game or the editor has the files open: ask the user to close it.
- Tests run without a headset (`layer/test/`): the engine with made-up controller frames, the real DLL over a
  pretend runtime, drawing on software GPUs. Add a test for every behaviour change.

## Code style
- C++: `layer/.clang-format` (run `clang-format -i` on changed files). C#: `editor/.editorconfig`
  (`dotnet format whitespace`). One statement per line.
- Named enums, not numbers or strings: profile text is converted once when a profile is read
  (`EnumName` tables, `ParseEnum`/`EnumKey` in `config.h`).
- One feature per class and file: `VrKeyboard`, `ScreenMirror`, `WristPanels`, `OffsetsPanel` hold their own state;
  `Engine` coordinates. Keep new features in that shape instead of growing `engine.cpp` or `layer.cpp`.
- Comments and user-facing text in plain English, no jargon. Logs say what happened in words a user can send us.
- Locate hooks never take `gMx` (see the locks table in `docs/ARCHITECTURE.md`).

## Behaviour rules
- Defaults must suit a new user. Tests use the built-in default profile, never the maintainer's own bindings.
- Anything that changes what the game sees (inputs, hand poses, head) is off by default or a clear user choice.
- Profile files must keep loading: unknown values fall back to the old behaviour; saves go through `WriteFileAtomic`.

## Git, releases and sharing
- Commit locally after each finished step. Do not push, tag or release unless the maintainer asks.
- Releases: bump `<Version>` in `editor/OpenXRCompanionEditor.csproj`, move CHANGELOG "Unreleased" to the version,
  tag `vX.Y.Z`, let CI build, zip the CI artifact with a top folder `OpenXRCompanion/`, publish with
  `release/notes-vX.Y.Z.md`. Notes end with the Patreon and Ko-fi links.
- Shareable test builds: `release/OpenXRCompanion-test-<date>-<name>.zip`, same layout as a release.
- Never include personal paths, names or emails in commits, notes or logs meant for users.

## Working notes
- `SESSION_NOTES.md` (not in git) has a "RESUME HERE" section at the top: update it after builds and long sessions.
- Bigger efforts keep their own notes file next to the code (`REFACTOR_NOTES.md`, `OPTIMIZE_NOTES.md`).
