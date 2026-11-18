# Changelog

## Unreleased

### New
- **Screen mirror: show a desktop monitor in VR**, on either wrist, in front of you, or following your view. Works with every monitor (numbered left to right) and in 32-bit games.
- **Frame rate cap you choose** (Settings > Screen mirror), from a few frames a second up to as fast as the headset, plus a list of rates a button steps through while playing.
- **New action "screen"**: show / hide it (optionally at a set place and monitor), next / previous monitor, next frame rate, move it to the next place. All of them work while playing, from a wheel or a wrist panel.
- **Adjust positions moves the screen too**: it shows while adjusting, grip to move, stick to resize, saved when you finish.
- **Place anything anywhere by grabbing it**: in Adjust positions the wheel, both wrist panels and the screen keep exactly the position and angle you leave them at (x, y, z, tilted or turned any way), relative to the hand, arm or head they belong to. Settings shows "Placed by hand" with a button to go back to the numbers. (Replaces turning wrist panels with the stick.)
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
