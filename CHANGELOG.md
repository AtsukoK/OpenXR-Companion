# Changelog

## v0.2.14-experimental (2026-11-23)

An experimental build from the `experimental` branch: new and only lightly tested. The stable release stays v0.2.13.

### New
- **In-game menu home**: the headset menu opens on big tiles: Controllers, Radial wheels, Wrist panels, Sticks, All bindings and More settings.
- **Wheels in the headset**: every wheel shown as it looks in the game. Point at a slice to change that button: its label (typed on the VR keyboard), what it does (a key, mouse or controller button, a sub-wheel or a new wheel, the VR keyboard, desktop screen, headset mirror, offsets, adjust mode, back, close), move, duplicate, delete. Add buttons and wheels, rename and delete wheels, see what opens a wheel and add a button that opens it. Wheel look and picking: where it shows, size, text size, stick or laser, which hand, deadzone, what the game's buttons do while it is open.
- **Wrist panels in the headset**: each forearm's panel and pages shown as their button grid. Turn a panel on or off, show the other arm's buttons, add, rename, delete and reorder pages and buttons, pick the page shown first, columns, size, side of the arm, poke or point, and when it shows.
- **More settings in the headset**: laser steadying, the fingertip near wrist panels, the short guide laser, vibration and its strength, messages, controller offsets, adjust mode, headset mirror.
- **Binding sheet** in the editor and the headset: 1 How, 2 Button, 3 What happens, with Details; How sets whether the game also gets the button.
- **Tutorials** in the editor: a full tour, "How do I..." tours and a tour of each page.
- Experimental builds look for experimental (prerelease) updates.
- **Full squeeze / Full pull**: the click at the end of the Steam Frame's dual-stage grips and triggers (and the Index trigger) can be bound on its own. The Frame's grips can also be bound to touch.
- The **Index trackpad** and the **WMR touchpad** can be bound: press, touch, and swipes (up / down / left / right). The Steam Frame's X / Y, bumpers and D-pad can be bound to touch, like A / B.
- **Controller configuration overview** is now the first page of the in-game menu: both controllers from above and from the side, with a tag on every button showing its name and what it does: your bindings, or else the game's own controls under the game's own names (read from the game's actions). Point at a tag to open that button's page: what the game does with it and every way to use it (press, quick tap, hold, double-tap, touch, stick directions), assigned or not. **Edit bindings**, **Test controllers** and the sticks are under it. The overview panel is wider than the other pages.
- In-game bindings: pick what a binding does on the VR keyboard: a key (with Ctrl / Alt / Shift), a mouse button or the wheel, or **Controller button** (the game sees another controller button). Works for bindings made in the editor too.
- Clearer wording on a binding's page: Enabled, Input, Activation, Gesture, Action, Key / Button behavior (Tap, Hold, Toggle), Pass input to game.
- **Steam Frame controllers** as their own controller, with their own picture in the editor and the headset: right A / B / X / Y and Menu, left D-pad and View, and both bumpers can all be bound. SteamVR normally shows the Frame to games as Index controllers, where those buttons can't be read, so there is a new opt-in setting (**Use all the Steam Frame's buttons**, on the Controller buttons page when Steam Frame is picked). With it on, OpenXR Companion asks SteamVR for the Frame controller itself and gives it the game's own bindings moved onto the Frame (left X / Y or A / B on D-pad down / up, the menu on View / Menu). Games that support the Frame themselves keep their own bindings. Off by default; your SteamVR binding changes for a game are not used while it is on.
- Every controller picture is now drawn upright from slightly behind its buttons, so the handle and grip show, and the face buttons carry their letters (A, B, X, Y, the menu's three lines).
- **In-game settings menu**: a panel in the headset (like SteamVR's dashboard) to change your bindings and sticks without taking the headset off. Switch bindings on or off, pick their button by pressing it, choose press or touch and when it fires, type their key on the VR keyboard, add and delete bindings, and set stick deadzones and direction zones. Live pictures: **Test controllers** shows both controllers as you press, touch and push; the bindings pages show their controller live, and the stick pages the stick with its zones and what the game sees. Opens from **Settings** on the left wrist panel's More page, or any button with the new **In-game settings menu** action. Saved to the profile.
- **Controller buttons pages**: Right / Left controller in the editor now show a picture of your controller, outlined from real controller models (Quest 3 / 3S, Quest 2 / Rift S, Quest Pro, Pico 4, Valve Index and Index-style, Vive wands, Windows Mixed Reality, HP Reverb G2). Click a button to see what pressing it, touching it or pushing the stick does, and change it: **press a key**, **act as another of the game's buttons** (like SteamVR's remapping), **turn it off in the game**, or anything else. These are ordinary bindings, listed under the controller. **Only when held for a moment** makes a choice start only when you hold the button, while a quick tap still reaches the game as a normal press.

- **Stick options** per hand, on the thumbstick of the controller pages (all off unless you set them): **inner and outer deadzones** for what the game sees from the stick, and a **direction zone** so pushing forward and leaning a little sideways doesn't also fire your left / right bindings.
- **Double flick**: a new way to use a button or stick direction, double-tap and hold (`double_tap_hold`). For example sprint: flick the stick forward twice and keep it pushed to hold Shift.

### Changed
- The built-in profile no longer has the switched-off "Quick save (shift + A)" and "Quick load (shift + hold B)" examples. Quick save and quick load stay on the wrist panel and the wheel.

### Fixed
- **Direction zone**: straightens the game's own stick too (pushing up and a little right is a plain up, so a jump on up doesn't also turn). It now also works when the game reads both sticks through one action and the sticks have different settings, and for games that read a stick direction as on / off (snap turn).
- Choices made in the headset menu that open a wheel, the VR keyboard, the desktop screen or the menu were not saved to the profile.
- Wrist panels and the keyboard now redraw after a profile reload.
- Quest 3 / Pro controllers were drawn as Quest 2 in the headset.
- **HP Reverb G2**: bindings on its X / Y / A / B buttons now work. They were read as on the first WMR controllers, which have no such buttons.

## v0.2.13 (2026-11-22)

### Fixed
- **Games could crash at startup when another OpenXR layer sits in front of OpenXR Companion** (seen with Cheeky foveated DLSS in Bannerlord). That layer asked us for a function before our own setup had run, and we passed the call on to nothing. We now answer "not available" until we are set up.

### Changed
- **Headset mirror: head camera smoothing is off by default** (also in the default preset, "Centred where you look"). Turn it on under Headset mirror if you want it; the strength stays at 0.5. Your own saved choice is kept.

## v0.2.12 (2026-11-22)

### New
- **Headset mirror presets**: one dropdown sets zoom, head camera smoothing, centre, edge margin and one or both eyes together (Centred where you look, the default; Centred with smoothing's own room; Wide: both eyes; Original; Exact headset view). Change anything afterwards and it shows Custom.
- **Both eyes** in the headset mirror's Eye choice: the view centres on where that eye looks, and the other eye fills in the nose side, where one eye's picture ends in a black strip. Far things line up; close things on that side can look a little doubled.
- **Head camera smoothing** (was "Steady the picture") is now an on/off switch with its own strength, kept while it is off.
- **Centre the view on** where the eye looks (new default) or the middle of the picture (the old way), an **Edge margin** to keep the black strip out, and **Smoothing makes its own room** (off by default) for smoothing that always works fully at low zoom.
- **Picture: Light / Full** for the headset mirror. Light (default) draws straight from the game's picture and costs about half as much GPU time; Full is the old copy with mipmaps.
- The headset mirror shows the game's **flat menu screens** (for example TTWVR's theater menus and loading screens) when the game shows no VR view, and black instead of leftover garbage when there is nothing.

### Changed
- **Headset mirror full screen**: maximizing (button or title double-click) now covers the whole monitor, taskbar included; double-click the picture to go in and out of full screen.
- The headset mirror is centred on where you look. On Quest that sits toward the nose, so the old view was a little off to one side.
- Zoom means the same with smoothing on or off: 1 is the whole eye picture.
- The headset mirror is sharper when the window is a bit smaller than the headset picture.
- **The screen mirror is sharper in 32-bit games** (Fallout / TTWVR): a 1920-wide monitor is shown at full size instead of half (the automatic limit is 1920 in every game now, and a picture is only halved when it is over 1.5 times the limit).
- Code cleanup: long functions split into named steps, graphics objects that free themselves, named constants in the editor. Nothing changes in how the app works.

### Fixed
- **Keys from your menus (quick load and so on) could go to the headset mirror window** instead of the game after you clicked or moved the mirror. The mirror is never taken for the game any more and hands focus straight back.
- The headset mirror's settings file kept numbers like 1.399999976 for 1.4.

## v0.2.11 (2026-11-21)

### New
- **SteamVR and Steam Frame support**: controllers without a built-in table (Steam Frame and other new controllers) get the menus' bindings from the inputs the game itself uses on them. Choose Index-style controllers for the Steam Frame.
- **Calibrate the poking fingertip**: in Adjust / Calibrate Positions both fingertips show; grab one with the other hand and put it where your real fingertip is. Also as numbers in Settings > Pointing (forward, up, toward the middle).
- A message in the headset when the menus get no controller input (for example SteamVR using saved bindings for the game), with what to do.

### Changed
- The laser near wrist panels stays until your fingertip is about 3.5 cm away, instead of disappearing early.
- The fingertip shown near wrist panels is smaller: just the tip.
- The controller choice in the editor preselects the controllers you use now (it always showed Meta Quest first).
- The logs show the controller type of each hand and whether the menus receive input, to help with new headsets.

## v0.2.10 (2026-11-21)

### Changed
- **Better optimized and better organized code.** The layer and the editor were restructured: each feature in its own class and file, named settings instead of loose text, one statement per line. Nothing changes in how the app works.

## v0.2.9 (2026-11-20)

### Fixed
- **Smoother in D3D12 games**: menus, toasts and the screen mirror no longer make the game wait for the GPU each time they redraw.
- Less work on the game's threads: the layer no longer holds its lock while the frame is shown, looks up its settings folder only once, and the screen mirror hands frames over without copying them.
- Profiles and settings are saved safely (written to a temp file, then swapped in), in the layer and the editor, so a crash can't leave a half-written profile.
- **Grab to set** (Controller offsets) cancels itself after 5 seconds without a grip, so a stray press can't hold your grips back or change your offsets later.
- The editor no longer rewrites the selected profile on every refresh (which made a running game reload and close open menus).
- The "your defaults were moved" notice was written to the wrong place and never shown.
- Editor, dark mode: key and steps boxes turned white while typing or recording a key, making the text unreadable.
- The layer logs tracking changes and sudden jumps of the hands, to help find tracking problems.
- **Steadying of the game's hands is safer.** A controller that loses tracking (or switches to hand tracking) is no longer steadied toward a stale or zero position, which could pull a hand or a body IK out of shape for a moment. When a game reads the hands at two times in one frame, both reads now get the same steadying instead of one raw and one steadied.

### New
- **Adjust the VR keyboard any time**: an **Adjust keyboard** button on the bar under the keyboard (and a **Move** key on the keys) lets you grab the keyboard with a grip and place it, without Adjust mode. The stick changes its size; **Done adjusting** saves it.
- **Use the mirrored screen while the keyboard is open**: point at the screen to move the mouse and click, and point at the keys to type, one hand each if you like.
- **Dock the screen and the keyboard together** for easier typing into a mirrored window: a bar under the VR keyboard has **Dock screen above keys** (the screen sits just above the keyboard and moves with it), **Dock keys under screen** (the keyboard follows the screen), **Adjust dock** (grab the docked one with a grip and put it where you like; the stick sizes the docked screen) and **Close keyboard**. The choice and the spot are saved with the profile (also in Settings > VR keyboard).
- **The profile you select in the editor is the one your games use**, always (it used to be a profile named after the game, else default.json). A running game switches within a second. If nothing is selected, the old order applies: the game's own profile, then `user_defaults.json`, then `default.json`.
- **One-click updates**: the editor's orange **Update available** button now downloads the new release from GitHub, checks it against GitHub's SHA-256 checksum, keeps the current version in `_previous_version`, swaps the files (even while the editor or a game has them open) and restarts. Profiles and settings aren't touched. Falls back to the download page if the folder can't be written.
- **Standard and your own defaults**: `default.json` is now the standard profile and is refreshed with every release, so new defaults reach everyone. Your changes go to `user_defaults.json` (created the first time you save changes to the standard; if you already have one, saving changes to the standard warns you and makes a new profile, `user_defaults 2.json`, instead of replacing yours; or when the headset saves offsets, placement or a screen choice), which becomes the selected profile. **An existing `default.json` with your edits is kept as `user_defaults.json`** automatically, once.
- **Headset mirror: Always on top** (Settings > Headset mirror). The mirror window also never takes focus from the game any more, even when you click, move or resize it.
- **Experimental features**, hidden behind Settings > Advanced > Show experimental features, with motion sickness warnings:
  - **Head smoothing** steadies the direction the game renders from, for using the headset as a camera. "For recording" (the headset still corrects to your real head) or "In the headset too". Strength 0 to 100: a heavy camera that smooths every movement the same way and eases into and out of turns (about 0.12 s of lag per step). Off by default, asks to confirm, and switches itself off whenever a game starts unless "Keep it on" is ticked.

### Changed
- **The VR keyboard can be placed by hand**: in Adjust / Calibrate Positions, open it with its Keyboard button, grab it with a grip and put it anywhere (the stick changes its width). It then opens in that spot, turned to wherever you face. Settings > VR keyboard > Placed by hand goes back to the numbers. The default spot is unchanged.
- **Adjust / Calibrate Positions** no longer brings out a wheel that is set to appear in front of you (it floated in your face). Open it with its usual button if you want to place it.
- **Wheel pages in the editor** now show what opens the wheel (buttons, wheel items, wrist buttons) and where it appears, with a **Change...** button to that setting.
- **Adjust / Calibrate Positions**: the wheel turns see-through so it no longer blocks the wrist panels or the screen you are placing. It becomes solid again when a hand comes near it to grab it, and fades further while you hold something else.
- **Overview page**: the on/off checkboxes are now green **ON** / red **OFF** buttons, each with how to open the feature in the headset in plain words ("Double-tap Right Thumbrest"). Wrist panel cards say how to bring them up, the keyboard card lists the wheel and wrist buttons that open it, and a new **Tools on your menus** card shows where the Adjust, Screen, Controller offsets and Headset mirror buttons are.

- Editor: more space between settings rows, and the Controller offsets sliders line up (units and 0 buttons in one column).

### Fixed
- The Controller offsets panel in the headset now faces your eyes; it used to lean its top toward you.
- Editor: scrolling the page with the mouse wheel no longer changes a dropdown, number box or slider that passes under the mouse; the page scrolls instead.
- Editor: the Overview page no longer turns light after a switch is clicked in dark mode.
- Editor: links (such as "+ add another button") were dark blue on dark and hard to read in dark mode.

### Docs
- README restructured (feature table at the top, quick start, then one section per feature, then reference) with new editor screenshots in dark mode. Sections for the headset mirror, controller offsets, steadying, the new default wrist panel, the editor's Settings pages and dark mode, experimental features, and the missing actions (`page`, `keyboard`, `adjust`, `screen`, `controller_offsets`, `headset_mirror`).

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
