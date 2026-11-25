OpenXR Companion - by Atsuko

Hand tools for any OpenXR game: radial menus (wheels), wrist panels, button combos, a VR keyboard,
your desktop mirrored into VR, a headset mirror window for recording, controller offsets for the
game's hands and guns, stick shaping for jump-on-stick games, and steadying for shaky hands.
Works with Virtual Desktop (VDXR), SteamVR OpenXR, Meta Link and other OpenXR runtimes.
Full guide with pictures: https://github.com/AtsukoK/OpenXR-Companion


INSTALL
1. Unzip this folder somewhere it can stay (for example C:\Games\OpenXRCompanion), not in
   Program Files. The game loads the overlay from this folder, so don't move or delete it after
   installing. If you do move it, click Install layer again.
2. Run OpenXRCompanionEditor.exe.
   The first time, it asks which controllers you use (this picks the buttons that open
   the wheels), then offers a short tour.
   Windows may warn that the program is unrecognised (it isn't code-signed yet):
   click "More info", then "Run anyway".
3. Click "Install layer" at the top. Windows asks for admin rights, which it needs to
   register the overlay with OpenXR.


YOUR FIRST GAME: SET UP YOUR MENU POSITIONS
At the top of the editor's Overview is the card "Set up your menu positions at the start of
the next game?". For a new install it is already on. Start your VR game: a few seconds in, a
card above your view walks you through placing each thing where you like it:
  left wrist panel, right wrist panel, the wheel, the screen mirror, the VR keyboard, fingertips.
- Grab each one with a grip, move it, let go. Thumbstick up / down while holding it: size.
- A (or X) = next, B (or Y) = skip. A, B, X and Y don't reach the game meanwhile.
- At the end everything is saved and the setup switches itself off.
Your positions are saved to your own copy of the profile (user_defaults.json). Redo it any time:
switch the Overview card on again, or press "Calibrate Menu Positions" on a wrist panel.


DEFAULT CONTROLS
- Wrist panels (both arms): turn your forearm toward your face and look at it. Touch a button
  with your other hand's fingertip, or point at it and pull the trigger.
    First page: Quick save / F5, Pause / ESC, Screen Mirror, Open VR Keyboard, -> Configuration Page
    Configuration Page: Quick load / F9, Reload profile, Calibrate Menu Positions,
      Controller offsets Configuration, VR mirror to Desktop, OpenXR Companion Ingame Editor
- Open a hand's wheel (depends on the controllers you picked):
    Quest / Rift / Pico: double-tap that hand's thumb rest
    Index / Pimax / Bigscreen: double-click that hand's thumbstick
    WMR / HP Reverb / Vive: hold that hand's menu button
  Point at an item and pull the trigger, or push the stick toward it.
- "OpenXR Companion Ingame Editor" opens the editor inside the headset. While it is open the
  game gets no controller input.

Change anything in the editor and save (Ctrl+S). A running game picks it up within a second.
default.json is the standard profile and updates with each release. Changes to it are saved to
user_defaults.json (your own defaults), which is never overwritten.
THE PROFILE YOU SELECT IN THE EDITOR IS THE ONE YOUR GAMES USE.


UPDATING
When a new version is out, a blue "Update available" button appears on the editor's toolbar.
Click it and choose "Update now". It downloads and checks the release, keeps your current files
in the _previous_version folder and restarts. Your profiles are not touched. Close VR games first.
Coming from v0.2.15 or older: the old updater may say "The update didn't finish". It did:
just start the editor again.


32-BIT GAMES (Fallout: New Vegas / Fallout 3 / TTW with TTWVR)
The x86 folder has the 32-bit layer, and "Install layer" installs it together with the 64-bit
one (the toolbar then shows "Layer: installed (+ 32-bit)"). Without the editor: run
x86\install.ps1 as admin. "Uninstall layer" removes both.


STICK OPTIONS (jump on a stick without turning)
Editor > Controllers > click a stick. Direction zone (degrees): a push near straight up counts as
exactly up for the game. "Ignore pushes outside the zones": diagonals give the game nothing.
Everything is off by default. Try 25-30 degrees with "Ignore outside" ticked.


SCREEN MIRROR, HEADSET MIRROR, CONTROLLER OFFSETS, STEADYING
- Screen Mirror (wrist panel): your monitor or a window in VR. Point at it for its button bar
  (Choose, Frame rate, Move, Keyboard, Adjust, Hide). Trigger = left click, A/X = right click,
  stick = scroll.
- VR mirror to Desktop (wrist panel) or the first toolbar button: a desktop window with what
  the headset shows, menus included. Good for OBS. Settings > Headset mirror.
- Controller offsets Configuration (wrist panel): fix how the game's hands point. Saved per game.
- Steadying for shaky hands (all off by default, 0.5 is a good start): Settings > Pointing >
  Steady the laser; Settings > Screen mirror > Steady the screen; Settings > Controller offsets >
  Steady the game's hands.

EXPERIMENTAL (hidden, off by default) - WARNING: CAN CAUSE MOTION SICKNESS
Settings > Advanced > Show experimental features adds the Experimental page with head smoothing.
Start with "For recording" and a low strength. Take the headset off at the first sign of feeling
unwell.


UNINSTALL
Open the editor and click "Uninstall layer", or run uninstall.ps1 as admin. Then delete the folder.
Your settings are in %LOCALAPPDATA%\OpenXRCompanion\ if you want to remove those too.


REPORTING PROBLEMS
https://github.com/AtsukoK/OpenXR-Companion/issues - please send:
- what you did and what happened
- your headset and runtime (Virtual Desktop, SteamVR, Link...) and the game
- the log: %LOCALAPPDATA%\OpenXRCompanion\logs\<game>.log  (the editor's "Logs" button opens the folder)

To turn the overlay off for one game: make a profile for it in the editor and switch off its
Master switch on the Overview. Or uninstall the layer.
