# Architecture

OpenXR Companion has two parts:

- **The layer** (`layer/`, C++): an OpenXR API layer DLL that the OpenXR loader puts between a game and the runtime.
  It reads the controllers, runs the menus, draws them into the headset and changes what the game sees
  (button presses, hand poses, the head when head smoothing is on).
- **The editor** (`editor/`, C# WinForms): edits the JSON profiles and settings files the layer reads, installs the
  layer, updates the app.

The two never talk directly. The editor writes files under `%LOCALAPPDATA%\OpenXRCompanion`, and the layer
checks their timestamps about once a second and reloads what changed.

## The layer

```
           game
            |  OpenXR calls
            v
  layer.cpp  ---- hooks, session state, the order things run in
   |   |   |   \
   |   |   |    head_smoothing.cpp   HeadSmoother: the steadied head (LocateViews / LocateSpace / EndFrame)
   |   |   game_offsets.cpp          GameHandOffsets: controller offsets + steadying on the game's hand poses
   |   |    `- tracking_log.cpp      TrackingLog: tracking changes, jumps, 10 s summaries
   |   overlay.cpp                   OverlayRenderer: draws what the engine shows, adds quad layers to the frame
   |    `- panel.cpp                 Panel (one swapchain), Gfx (D3D11 / D3D12), GDI+ drawing of each element
   |    `- screen.cpp, wincap.cpp    ScreenCapture: monitors and windows, on its own thread
   engine.cpp                        Engine: input in, menus and actions out (no OpenXR or graphics calls)
    `- wheel.cpp, adjust.cpp         the wheel, Adjust mode (grabbing things to place them)
    `- vr_keyboard.cpp               VrKeyboard: the VR keyboard and docking it with the screen
    `- screen_mirror.cpp             ScreenMirror: where the mirrored screen sits, pointing at it, its bar and picker
    `- wrist_panels.cpp              WristPanels: the buttons on each forearm
    `- offsets_panel.cpp             OffsetsPanel: the controller offsets panel
    `- in_game_menu.cpp              InGameMenu: the in-game settings menu (pages from menu_pages.cpp, rows in menu.h)
    `- config.cpp                    profiles and settings files (JSON), atomic saves
    `- keys.cpp                      key presses and mouse input sent to Windows, on a worker thread
   mirror.cpp                        HeadsetMirror: the desktop window that shows one eye
```

### Engine

`Engine` (engine.h) is the heart and has no OpenXR or graphics dependency, which is why the tests can drive it
directly. Each frame the layer fills a `Frame` (buttons, sticks, grip and aim poses, head pose) and calls
`Engine::Update`. The engine then:

- decides what the game should not see (`Blocked`) and what it should see instead (`Ov`, overrides);
- updates the visible elements, each described by a plain "view" struct the overlay draws from
  (`WheelView`, `KeyboardView`, `WristView`, `ScreenView`, `OffsetView`, `PointerView`);
- queues requests for the layer to carry out: haptics, saving the profile (`saveRequested`, `offsSaveRequested`),
  the headset mirror (`mirrorAsk`), toasts.

Each feature (`VrKeyboard`, `ScreenMirror`, `WristPanels`, `OffsetsPanel`) is its own class that owns its state and
logic. The engine holds one of each, calls them from `Update`, and they reach shared things (input, grabbing, toasts,
the views) through a reference to the engine, which lists them as friends.

Views carry a `version` that changes when their look changes, so the overlay only redraws a picture when needed.

### Threads and locks

| Thread | What runs on it |
| --- | --- |
| Game input thread | `xrSyncActions` (reads controllers, runs `Engine::Update`), `xrGetActionState*` |
| Game render thread(s) | `xrLocateSpace(s)`, `xrLocateViews`, `xrEndFrame` (drawing the overlay) |
| Screen capture thread | `ScreenCapture`: grabs monitor / window pictures, hands them over by swap |
| Keys worker thread | `keys.cpp`: key presses and mouse input, so waits never stall a frame |
| Mirror window thread | `HeadsetMirror`: the desktop window |

| Lock | Guards | Taken by |
| --- | --- | --- |
| `gMx` (layer.cpp, recursive) | the engine, our action set, the game's bindings, the overlay | input hooks, `xrEndFrame` (released before calling the runtime's `xrEndFrame`, which may wait for the compositor) |
| `GameHandOffsets` own lock | registered hand spaces, current offsets, steadying filters, tracking log | locate hooks (render thread), the input thread when it hands over new offsets |
| `HeadSmoother` own lock | head smoothing settings, history, view spaces | locate hooks, `xrEndFrame`, the settings file check |

Rule: the locate hooks never take `gMx`. Games may locate poses from their render thread while the input thread
holds `gMx`; taking it there could stall a frame. Data they need is copied over under the smaller locks instead.

### Changing what the game sees

- **Buttons**: our own action set reads the controllers (`EnsureActions`). The game's actions are answered from
  the real state, unless the engine blocks a part (held for a menu) or overrides it (a binding that presses a game
  input). See `Evaluate` and the `Hook_xrGetActionState*` functions.
- **Hand poses**: `GameHandOffsets::Apply` on every pose the game locates for its hand spaces.
- **Head**: `HeadSmoother` (experimental, off by default).

### Files the layer reads and writes

All under `%LOCALAPPDATA%\OpenXRCompanion` (see `config.cpp`): `profiles\*.json`, `active_profile.txt`,
`head_smoothing.json`, `headset_mirror.json`, `logs\<game>.log`. Saves go through a temp file and a rename
(`WriteFileAtomic`), so a reader never sees half a file.

### The in-game settings menu

A panel in front of you with pages of settings, like SteamVR's dashboard, opened by any binding or menu button
with the `settings_menu` action (the default left wrist panel has one on its More page).

- `menu.h` / `menu.cpp`: the model. A `MenuPage` is a title and `MenuRow`s: Heading, Note, Link, Button, Toggle,
  Number, Choice. Rows carry small functions that read and change their value, so a page is built straight from
  the profile and edits go straight back into it. Also the view the overlay draws (`MenuView`) and `MenuLayout`,
  the one place that says where each clickable part sits (used by drawing and pointing alike).
- `in_game_menu.cpp`: `InGameMenu` runs it: the page stack (Back), pointing and the trigger, the stick (up / down
  scrolls, left / right moves the number pointed at), asking for a button (`CaptureInput`) or a key from the VR
  keyboard (`CaptureKey`), and saving a moment after the last change (`menuSaveRequested`, then `SaveMenuEdits`).
- `menu_pages.cpp`: the pages. Bindings change through `Engine::EditBindings`, which ends whatever a binding is
  doing first, so nothing sticks.

**Adding a part of the app to the menu**: write a page builder in `menu_pages.cpp` (rows with get / set functions
on the profile or the engine), add a `MenuRow::Link` to it on `Home`, and, if it changes something kept in the
profile file, write that part in `SaveMenuEdits` (config.cpp). Add a test in `layer/test/test_menu.cpp`.

## The editor: controller pages

The Right / Left controller pages draw the controller and offer quick choices per button. All of it is editor-side:
the quick choices make ordinary bindings, so the layer needs nothing new for them.

- `ControllerLayouts.cs`: each controller type's drawing and parts, as data (design units, right hand; the left hand is
  mirrored). A part names the input its bindings use, how it can be used (press, touch, stick directions), and why
  not, for parts the layer can't read.
- `ControllerPicture.cs`: draws one layout for one hand, with name tags, and reports clicks.
- `ControllerButtonsPage.cs`: the page; `QuickBindings.cs`: the bindings its choices make;
  `QuickBindingDialogs.cs`: the key / game button questions.
- `--controller-pics <dir>` renders every picture; `--selftest` checks every quick binding against the layer's parser.

## Tests

`layer/test/test.cpp` builds into `vro_test.exe` and runs without a headset:

- the engine driven with made-up controller frames (menus, bindings, keyboard, wrist panels, offsets, docking);
- the real layer DLL over a pretend runtime (head smoothing, Unity and Unreal style);
- panels on software GPUs (WARP): D3D12 uploads, and the overlay through D3D11 with pretend swapchains;
- pictures saved as PNG for checking by eye.

Build and run: `quick_test.sh` (64-bit layer + tests), or `build_all.ps1` for everything including the editor
and the 32-bit layer.
