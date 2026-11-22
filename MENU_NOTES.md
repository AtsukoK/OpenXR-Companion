# RESUME HERE (2026-11-22, late)
Branch controller-buttons (worktree "OpenXR Companion - Buttons"), all committed locally (last 416c417 + this notes
commit), NOT pushed; dist2 (G:/Claude/VR Radial Overlay/dist2) matches it. Nothing below tested in a headset yet.
Next: user tests in the headset (overview menu, binding keyboard, Frame / Index extras); then merge to main and release
only when the user says so.
- In-game menu opens on **Controller configuration** (MenuPages::Overview, wide page 0.82 m): both controllers top + side
  (silhouettes now carry side views: SilPart.side, Silhouette.sideBody), tags per part (MenuRowKind::Overview, layout
  OverviewLayout in menu.cpp shared by drawing/pointing, MenuHit.sub). Tags: part name + the player's bindings (blue,
  gesture line grey) + the game's own action names (Engine::gameUses, recorded in layer.cpp RecordGameUses from
  xrCreateAction names + suggested bindings, per interaction profile). Bar under it: Edit bindings (old Home), Test
  controllers, Right/Left stick. Tag -> MenuPages::PartPage (game use + each way, "Not assigned" -> new binding + keyboard).
- Binding keyboard (KbLayout forBinding): mouse buttons, wheel, Home/End..., "Controller button" -> xr_input target
  (CaptureAction). Saved by SaveMenuEdits (xr_input with target).
- Wording: Enabled / Input / Activation / Gesture / Action / Key-Button behavior (Tap Hold Toggle) / Pass input to game.
  Editor aligned the same day (Enabled, Input, Gesture, Action, Key behavior, Pass input to game); "unused" / "Not used" / "Not assigned" now say "Default".
- Steam Frame: own ControllerKind, comps C_BTN3..C_DPAD_RIGHT, opt-in steam_frame.json native mode (see BUTTONS_NOTES),
  outlines from Valve CAD (CC BY-NC-SA, credited).
- Controller pictures fixed 2026-11-22: face view no longer flipped (right Quest: stick RIGHT of A/B, user confirmed with
  the real controller). Index view flipped too (user confirmed: right Index stick right of A/B). Fallback drawings (ControllerLayouts top parts, controller_art kTouch)
  still have the old mirrored layout but are only used without a silhouette.
- Trackpad (2026-11-22): own comp C_TRACKPAD ("trackpad") for the Index trackpad (force = press) and WMR touchpad
  (click): press, touch, swipe directions (touched toward an edge past stick_on). Vive's trackpad stays its STICK; a
  block on the Vive stick also blocks the game's trackpad bindings. Profiles that wrote "trackpad" for the Vive stick
  now mean C_TRACKPAD (none of ours did). Steam Frame: touch read for X / Y, bumpers and D-pad too.
- Dual-stage (2026-11-22): feature F_FULL ("right.grip.full", "right.trigger.full") = the click at the end: Frame
  grips + triggers (squeeze/click, trigger/click), Index trigger (trigger/click). Frame grip touch (squeeze/touch) read too.
  Game paths squeeze/click and trigger/click still count as the grip / trigger for blocking.
- Open ideas: overview pictures could be bigger (spare height); long tag text gets cut; highlight the part on PartPage.
- Pictures: layer/menu_overview.png, menu_part.png, menu_binding.png, keyboard_binding.png, menu_test_frame.png (tests).

# In-game settings menu (branch controller-buttons)

## What it is
A panel in front of you (like SteamVR's dashboard) to change bindings and sticks in the headset. Opened by the
`settings_menu` action; the default left wrist panel has **Settings** on its More page.

## Design (see docs/ARCHITECTURE.md, 'The in-game settings menu')
- menu.h: rows with get / set functions (Heading, Note, Link, Button, Toggle, Number, Choice), MenuView, MenuLayout
  (one layout for drawing and pointing).
- in_game_menu.cpp: InGameMenu (page stack, pointing + trigger, stick scroll / adjust, CaptureInput, CaptureKey via
  VrKeyboard::CaptureNextKey, save 1 s after the last change).
- menu_pages.cpp: Home, ControllerBindings(hand), BindingDetail(index), StickOptions(hand).
- Engine::EditBindings ends running bindings before a change; SaveMenuEdits (config.cpp) writes bindings by their
  sourceIndex (keeps fields the menu doesn't edit), sticks, stick thresholds.
- While a hand points at the menu its trigger and stick work the menu (hidden from the game, its bindings skipped);
  while a button is being picked everything is hidden from the game and no binding fires.

## Live pictures
- MenuRowKind::Live (span > 1 slots; MenuLayout / MenuMaxFirst handle tall rows). LiveKind: Controller, BothControllers, Stick.
- controller_art.cpp: top-view drawings for Touch, Index, Vive, WMR, Reverb G2 (ControllerKindOf(interaction profile);
  HandRaw::kind set by the layer). Unknown = the Touch drawing.
- InGameMenu::UpdateLive copies press / touch / trigger / grip / stick and the stick options into MenuView::live at
  most every 50 ms, and only bumps the version when something changed beyond 0.01 (a GDI+ redraw per change).

## Checks
- test_menu.cpp: TestInGameMenu (open, link, numbers, save timing, new binding by pressing a button + typing the key,
  choices, On switch, delete, close) and TestMenuSave (kept fields, delete, add, sticks). Pictures: layer/menu_*.png.

## Ideas for later (the framework is made for these)
- Pages for: wheels and wrist panel items, screen mirror settings, headset mirror, pointing / steadying, head smoothing.
- Grab the panel with a grip to move it; remember where it was.
- Change what a binding does (wheel / macro / layer) in the headset, not only keys.
- Editor: an 'In-game settings' toggle in the Overview tools card is already listed (finds the menu button).

## Not tested
- In a headset. The trigger, stick and keyboard flows are driven by the engine tests only.
