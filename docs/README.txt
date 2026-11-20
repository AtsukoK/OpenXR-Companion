OpenXR Companion - by Atsuko

Hand tools for any OpenXR game: radial menus (wheels), wrist panels, button combos, a VR keyboard,
your desktop mirrored into VR, controller offsets for the game's hands and guns, and steadying
for shaky hands. (Formerly OpenXR Hand Menus.)
Works with Virtual Desktop (VDXR), SteamVR OpenXR, Meta Link and other OpenXR runtimes.


INSTALL
1. Unzip this folder somewhere it can stay (for example C:\Games\OpenXRCompanion).
   The game loads the overlay from this folder, so don't move or delete it after installing.
   If you do move it, click Install layer again.
2. Run OpenXRCompanionEditor.exe.
   The first time, it asks which controllers you use (this picks the buttons that open
   the wheels), then offers a short tour. The tour is worth doing: it takes about two minutes.
   Windows may warn that the program is unrecognised (it isn't code-signed yet):
   click "More info", then "Run anyway".
3. Click "Install layer" at the top. Windows asks for admin rights, which it needs to
   register the overlay with OpenXR.
4. Start an OpenXR game. A message at the bottom of your view saying
   "OpenXR Companion: default.json loaded" means it's running.


DEFAULT CONTROLS
- Open a hand's wheel (depends on the controllers you picked):
    Quest / Rift / Pico: double-tap that hand's thumbrest
    Index / Pimax / Bigscreen: double-click that hand's thumbstick
    WMR / HP Reverb / Vive: hold that hand's menu button
  The wheel appears in front of you. Pick an item by pushing the stick toward it and letting
  it spring back, or by pointing at it and pulling the trigger (WMR/Vive: let go of menu).
  The "Controllers" button at the top of the editor changes the opening buttons later.
- Wrist panels: turn your wrist toward your face and look at it. Poke a button with your
  other hand's controller (a ring shows where it will touch), or point at it and pull
  the trigger.
- The VR keyboard is on the wheels and the left wrist panel.
- Move things to where you like them: right wheel > More... > Adjust positions. Grab the
  wheel or a wrist panel with your other hand's grip and move it; while holding, the stick
  resizes it (up/down) or turns a wrist panel (left/right). Adjust positions again saves.

Change anything in the editor and save (Ctrl+S). A running game picks it up within a second.


32-BIT GAMES (Fallout: New Vegas / Fallout 3 / TTW with TTWVR)
32-bit games only load 32-bit layers, which Windows keeps in a separate list. The x86
folder has the 32-bit layer, and "Install layer" in the editor installs it too, together
with the 64-bit one. The toolbar then shows "Layer: installed (+ 32-bit)".
1. Install as above.
2. Without the editor: run x86\install.ps1 as admin (right-click > Run with PowerShell, or from an admin
   PowerShell: powershell -ExecutionPolicy Bypass -File x86\install.ps1).
3. Start the game. The same profiles are used (per-game: profiles\FalloutNV.json).
"Uninstall layer" removes both (or run x86\uninstall.ps1 as admin).

SCREEN MIRROR (a monitor or window in VR)
Turn it on from the left wrist panel: More > Screen. Point at the screen and a button bar
appears under it:
  Choose...   pick any monitor or open window (your pick is remembered)
  Frame rate  steps through the frame rates from Settings
  Move...     left wrist / right wrist / in front of you / following your view, or Cancel
  Keyboard    VR keyboard; typing goes into the mirrored window
  Adjust      grab the screen (or wheel, wrist panels) with a grip and place it anywhere;
              press "Done adjusting" to save
  Hide        turns it off
Pointing at the screen moves the mouse: trigger = left click, A/X = right click,
stick = scroll. Recommended: frame rate 15-30, button bar size 0.7, and on your Screen
buttons leave Monitor on "Keep the current one" and the window name empty.

UNINSTALL
Open the editor and click "Uninstall layer", or run uninstall.ps1 as admin. Then delete the folder.
Your settings are in %LOCALAPPDATA%\OpenXRCompanion\ if you want to remove those too.


REPORTING PROBLEMS
Please send:
- what you did and what happened
- your headset and runtime (Virtual Desktop, SteamVR, Link...) and the game
- the log: %LOCALAPPDATA%\OpenXRCompanion\logs\<game>.log  (the editor's "Logs" button opens the folder)
- your profile: %LOCALAPPDATA%\OpenXRCompanion\profiles\default.json

If a game misbehaves, you can turn the overlay off for just that game: in the editor, make a
profile for it and untick "Overlay is on" in Settings. Or uninstall the layer.
