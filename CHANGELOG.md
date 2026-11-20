# Changelog

## v0.2.8 (2026-11-20)

### New
- **New default left wrist panel** with the screen and calibration tools close at hand: Quick save, Pause, **Monitor** (screen on your wrist), Keyboard and More; under More: Quick load, Reload profile, **Calibrate Positions**, **Controller offsets**, **Headset mirror** and **Screen options** (place, monitor, window, frame rate). Smaller panel, placed a little further up the arm. Only new profiles get it; yours stays as it is (or add the buttons in the editor).

### Fixed
- **Headset mirror with steadying on no longer shows the picture's edges.** The crop leaves some room around it, and the steadied view never trails far enough to run past the edge of the game's picture.
- Headset mirror is sharper: smooth shrinking when the window is smaller than the eye picture, and a sharper stretch when it is bigger.
- The mirrored screen is hidden when seen from behind.

## v0.2.7 (2026-11-19)

### New
- **Editor redesign**: Settings is split into short pages in the list (General, Wheel look and picking, Pointing, VR keyboard, Screen mirror, Controller offsets, Headset mirror, Advanced); each section sits in a card tagged **Saved with profile** or **Applies straight away**; help moved behind (i) icons (hover or click); content centred; the search box finds settings too and highlights the matching rows.
- **Dark mode** for the editor, **on by default**: Dark, Light or Auto (follows Windows) from the button on the right of the toolbar, with a dark title bar and scrollbars.
- **Faster, calmer editor**: pages open 2-3x faster and appear in one go (no controls popping in one by one), the list no longer flickers, and it stays scrolled where it was when you move or drag items.
- "Created by Atsuko" in the editor's status bar.
- **Headset mirror**: a desktop window showing what the headset shows, **including the wheel, wrist panels, mirrored screen, lasers and messages** (the game's own mirror can't show those, they're added after the game). Good for recording or streaming (capture the window in OBS) or for someone watching. One switch, always first on the editor's toolbar; also an action `headset_mirror` for wheels and wrist panels (the default left wrist panel has it under More). Closing the window turns it off. Settings > Headset mirror: eye (right by default), zoom (fills the window), **steady the picture** (irons small head shakes out of the window like a camera stabiliser, on at 0.5 by default) and a frame rate cap. Never holds the game up. D3D11 games for now.

## v0.2.6 (2026-11-19)

### Renamed: OpenXR Hand Menus is now **OpenXR Companion**
- It does a lot more than menus now. The layer, the editor (`OpenXRCompanionEditor.exe`) and the settings folder (`%LOCALAPPDATA%\OpenXRCompanion`) have the new name. **Your profiles are copied over by themselves** the first time either one runs.
- **Click Install layer once** in the new editor: it registers the new layer and removes the old one, so the two never run together.

### New
- **Controller offsets for the game** (new Settings section and action `controller_offsets`): turn and move the controllers the *game* sees (its hands, guns and its own laser), for games whose hands point too low on Quest controllers or that have no pitch setting. Pitch, yaw, roll and up/down, left/right, forward/back, for both hands mirrored or each hand on its own, for the grip and aim poses together or apart. Works in any OpenXR game; each game's profile keeps its own numbers, and one tick turns it off.
  - **In the headset**: a panel with + / - buttons (hold to repeat, or point at a row and push the stick like a slider), the value in the middle resets it, **Grab to set** (hold grip, turn the controller until it feels right, let go: the game's hand holds still while you turn), Reset hand / Reset all, step size and on/off. Changes save by themselves.
  - **Steady the game's hands** (off by default): the same steadying as below, applied to the hands and guns in the game.
  - The default left wrist panel has a **Controller offsets** button under More.
- **Steadying for shaky hands** (off by default): **Settings > Screen mirror > Steady the screen** keeps a screen on your wrist (or following your view) still while you look at it, and **Settings > Pointing > Steady the laser** smooths the laser and the mouse on the mirrored screen. Both hold still through small shakes but follow straight away when you really move (a one-euro filter). 0.5 is a good start.

## v0.2.5 (2026-11-18)

### New
- **Screen mirror: show a desktop monitor in VR**, on either wrist, in front of you, or following your view. Works with every monitor (numbered left to right) and in 32-bit games.
- **Frame rate cap you choose** (Settings > Screen mirror), from a few frames a second up to as fast as the headset, plus a list of rates a button steps through while playing.
- **New action "screen"**: show / hide it (optionally at a set place and monitor), next / previous monitor, next frame rate, move it to the next place. All of them work while playing, from a wheel or a wrist panel.
- **Adjust positions moves the screen too**: it shows while adjusting, grip to move, stick to resize, saved when you finish.
- **Place anything anywhere by grabbing it**: in Adjust positions the wheel, both wrist panels and the screen keep exactly the position and angle you leave them at (x, y, z, tilted or turned any way), relative to the hand, arm or head they belong to. Settings shows "Placed by hand" with a button to go back to the numbers. (Replaces turning wrist panels with the stick.)
- **Mirror one window instead of a whole monitor** (a browser, Discord, a video player...), like SteamVR or Discord let you pick. It shows even when other windows cover it. Pick it in Settings > Screen mirror, give a button a window name (e.g. "chrome"), or flip through open windows with the new "next window" / "previous window" buttons while playing. Clicking on a mirrored window brings it to the front first.
- **Button bar on the mirror**: point at the screen and a bar appears under it: **Choose...** (a list of every monitor and open window to pick from with the laser, 12 per page), **Frame rate**, **Move**, **Keyboard** (opens the VR keyboard and brings the mirrored window to the front so typing goes there) and **Hide**. A "choose" screen action opens the same list from a wheel or wrist button.
- **Bar: Move... is a list** (left wrist, right wrist, in front, following view) with Cancel; new **Adjust** button (the bar turns into "Done adjusting" while you move things); the bar is 30% smaller by default and has a size setting (Settings > Screen mirror > Button bar size).
- **The screen remembers your pick**: it starts on a monitor, and whatever monitor or window you pick in the headset (Choose..., next monitor/window) is saved as the default for next time. A window comes back by its exact title if it's still open, otherwise the same program.
- Saving positions or a screen pick from the headset no longer reloads the profile (which closed open menus).
- **Use the desktop from VR**: point at the mirrored screen and the mouse follows; trigger = left click (hold to drag), A / X = right click, stick up/down = scroll. The game gets its window back when you point away. Both can be switched off in Settings > Screen mirror.
- The built-in default profile has the buttons on the left wrist panel under More > Screen.

## v0.2.4 (2026-11-18)

### Changed
- **Install layer now also installs the 32-bit layer** (for Fallout: New Vegas / Fallout 3 with TTWVR) in the same admin prompt, and **Uninstall layer** removes both. The toolbar shows whether the 32-bit layer is installed too. Running the scripts in `x86` by hand still works.

## v0.2.3 (2026-11-18)

### New
- **32-bit build for 32-bit OpenXR games**, such as Fallout: New Vegas, Fallout 3 and Tale of Two Wastelands with TTWVR. It is in the zip's `x86` folder with its own `install.ps1` / `uninstall.ps1` (run as admin), because 32-bit layers are registered in a separate place that the editor's Install layer button doesn't cover. See "32-bit games" in the README.

## v0.2.2 (2026-11-17)

### New
- **Aim correction for Touch controllers.** Quest/Rift Touch controllers point a little high. The laser, wheel, keyboard and wrist panels now use a corrected aim (tilted down 15°, set in Settings > Pointing). It applies automatically when a Touch controller profile is detected, and can be set to always or off.
- **Guide laser.** While a wrist panel is open and your other hand isn't aimed at it, a short beam shows where that hand points.
- **Fingertip.** A see-through fingertip shows along the pointer when you are close to any wrist panel, including point-only panels, so you can see where you'll touch.

### Changed
- The laser no longer disappears when you get close to a wrist panel. It now hands over to the touch cursor only once the cursor is shown, which also fixes point-only panels losing the laser up close.
- The touch cursor appears from a little further away (8 cm instead of 6 cm).

## v0.2.1 (2026-11-11)

### New
- **Update check.** On start the editor checks GitHub for a newer release and shows an orange "Update available" button that opens the releases page. Turn it off in Settings > General > Updates.

### Changed
- The editor's wrist preview now fits the hand and panel at any angle and centres the panel by its real height.
- The default right wrist panel now mirrors the left one.

## v0.2.0 (2026-11-10)

### New
- **Adjust mode.** A new "Adjust positions" action (in the right wheel under More..., and on the left wrist panel's More page). While it's on, the main wheel and both wrist panels stay visible. Grab one with your other hand's grip and move it. While holding it, push the stick up or down to resize it, and left or right to turn a wrist panel. Use Adjust positions again to save. Your grips don't reach the game while adjusting.
- **Laser pointers.** A beam and a dot show where a controller is aiming at the wheel, the VR keyboard or a wrist panel. Close to a wrist panel the beam hides and the touch ring takes over.
- **Touch ring on wrist panels.** A ring shows where your other controller will touch the panel. It shrinks as you get closer and turns into a solid dot on contact.
- **Controller choice on first start.** The editor asks which controllers you use and picks the buttons that open the wheels:
  - Quest, Rift and Pico: double-tap the thumbrest
  - Index, Pimax and Bigscreen: double-click the thumbstick
  - WMR, HP Reverb and Vive: hold the menu button
  You can change it later with the new **Controllers** button.
- **F1 to F12** on the VR keyboard.
- **Custom angle** for wrist panel text, set in the editor or by turning the panel in adjust mode. The editor also has a **Sideways** setting.

### Changed
- **The project is now called OpenXR Hand Menus.** Settings move from `%LOCALAPPDATA%\VRRadialOverlay` to `%LOCALAPPDATA%\OpenXRHandMenus`, and are copied across automatically on first start. Click **Install layer** once after updating.
- **New default placement:**
  - The wheel opens in front of your face, 0.70 m away.
  - Pick wheel items with the stick or the pointer.
  - The wrist panels are smaller and mirrored on both arms.
  - Wrist panels respond to both poking and pointing with the trigger.
- The editor reloads a profile when it changes on disk (for example after adjust mode). If you have unsaved changes, it asks first.
- If the editor can't find the layer files next to it, it now explains to extract the whole zip first.

### Fixed
- A double tap on a touch input no longer fires when the button is really pressed twice (for example, pressing B twice quickly to loot).
- The release zip now extracts correctly with every unzip tool.

## v0.1.0 (2026-11-10)
- First test build: radial wheels, wrist panels, VR keyboard, button combos, layers, macros and controller remaps, plus the profile editor with a guided tour.
