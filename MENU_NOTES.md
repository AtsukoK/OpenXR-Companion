# RESUME HERE (2026-11-23, morning) -- dist2 only, release target: branch experimental
Branch **dist2-line** (= local branch **experimental**, last afdb57e + this notes commit), built into
G:/Claude/VR Radial Overlay/dist2: editor, 64-bit layer AND dist2/x86 32-bit layer (Fallout NV / TTWVR uses x86: always
build32 + copy it too). Version **0.2.14-experimental**. Nothing pushed. Main (G:/Claude/VR Radial Overlay, branch main)
has one local commit 764362b: README link to the experimental branch (push together with experimental).
Tests: layer quick_test 0 failures; editor --selftest OK; --tourshots ALL TOURS FINISHED.

Next: the user tests in the editor + headset; then push experimental, publish a PRERELEASE v0.2.14-experimental from it,
push main's README link. Ask the user before pushing.

## Done since the night notes (all untested by hand unless said)
- Binding sheet (editor): 1 How / 2 Button / 3 What happens + foldable Details (open by default); How sets "the game
  also gets the button" (press: no; hold: no + tap passthrough; double-tap / quick tap / touch: yes). "+" on a tag makes a
  starter binding (does nothing, game keeps the button) and opens the sheet; "Swap with..." in the side panel.
- Headset binding page matches: 1 How / 2 Button / 3 What happens / Details, same default rule; new "What happens" page:
  key/mouse/controller (binding keyboard), any wheel, VR keyboard, desktop screen, settings menu, nothing.
- Headset fixes: binding keyboard on the menu is drawn after the menu (was covered by the dimmed menu = grey);
  controller_picture.txt was never read (path "\c" lost its backslash) -> Quest 3 now drawn as Quest 3 (user saw the ring).
- **Direction zone now straightens the GAME's stick** (user's intent: jump on up must not turn): inside a zone the game
  gets exactly up/down/left/right, diagonals untouched, 0 = off; single-axis game actions use the whole stick
  (Engine::stickX/Y). Tests added. Headset degree sign fixed ("Â°").
- Tutorials: menu (rail cap icon + toolbar), full tour, How-do-I tours, page tours, "Show me this page" on every tip bar.
- Updater: experimental builds also look at prereleases (UpdateCheck.PickReleaseAsync).
- Editor starts on the icon bar and maximized. README (experimental) rewritten + docs/images from --github-shots.
- All bindings page (rail), wheel page (sub-wheel tabs, Opened by, All buttons), wrist page list/drag/Advanced, Overview fits one screen,
  Rename profile, version bottom right.

## To test in the headset (priority)
1. Controller pictures: Quest 3 (no ring) in the in-game menu.
2. Direction zone on the right stick: push up slightly right -> jump, no turn.
3. In-game binding page: How changes "game also gets", What happens -> a wheel; binding keyboard bright.
4. Editor: + on a tag -> sheet; Details fold; tutorials (full tour) end to end.

# RESUME HERE (2026-11-23, night) -- dist2 only
Work happens on branch **dist2-line** in this worktree (G:/Claude/OpenXR Companion - Dist2), built into
G:/Claude/VR Radial Overlay/dist2 (layer + editor, last full build 9fe1050). All committed locally, NOT pushed, nothing
merged to main. dist3 (branch controller-buttons, worktree "OpenXR Companion - Buttons") is a retired editor design
trial: take parts from it when asked, don't build it. Nothing from tonight is tested in a headset; most editor screens
were only checked with screenshots (editor --rail-shot <dir>, VRO_SHOTS_PROFILE = a copy of a profile, nothing saved).

Next: the user tests dist2 (editor + headset), especially the items marked (!) below. Merge / release only when they say so.

## In dist2 now (on top of 5ab37fa, the binding keyboard on the menu pane)
- Key dialog auto-layout with "OK, use this" (QuickBindingDialogs.Ask).
- Left list folds into an icon bar (MainForm.Rail.cs, list_folded.txt): Overview, Settings, Controllers, Wheels,
  L / R wrist, Search; section lit.
- Controllers screen (ControllersHub.cs, tag "controllers", from dist3): both controllers stacked top + side, + on every
  tag (way cards -> action cards), side drawer with the button's ways / binding editor, only that hand shown while the
  drawer is open, big orange Close, Controller model dropdown + "Apply a controller preset..." (= Your controllers);
  no Configure buttons. (!) the dropdown fix (it assigned the ctor parameter, not the field).
- Settings (cog) = feature tiles (FeatureTiles.cs, from dist3). Wheels = wheel gallery cards (WheelsGallery.cs).
- Overview: two columns of pairs (Master switch | VR keyboard, wheels L | R, wrists L | R, other | tools), big
  "Profile: ..." button top middle (switch profile), "Master switch" (was Overlay for this game).
- + Add cards: Press a key, Swap with another button (two bindings A<->B), Hold: press a key (tap passthrough),
  Double flick / double-tap-hold, Something else. Removed: turn off in game, hold: act as another button.
- Compact way cards (+ Add on the same row, press point only once bound). Stick options: "Direction counts at (%)".
- Double flick "Keeps going until (%)": Binding::releaseAt / "release_at" (layer, editor, headset menu); new double
  flicks 0.3; test TestDoubleFlickRelease. (!)
- Wrist panel page (SettingsEditor wrist): cards Pages and buttons (tabs, page title / shown first / delete page,
  + Add button / + Add page, click a button -> its ItemEditor beside the picture with Move / Delete; no page jump: the
  list rebuilds quietly, keepPage) | Panel | On the arm (big orange "Reset wrist panel location" = built-in place from
  the default profile, numbers folded under "Fine-tune the position with numbers") | When it shows (WristCones.cs view
  cones). Arm drawing removed. Page picture follows Columns.
- (!) Layer fix: wrist panels and the keyboard redraw after a profile reload (Engine::Reset kept version numbers
  counting; they restarted at 0, so an old picture / old columns could stay while pointing used the new layout).
  Test TestReloadRedraws.
- Layer tests: sh quick_test.sh (0 failures). This worktree needed third_party/openxr-sdk copied in (submodule).

## Added later on 2026-11-23 night (all editor, screenshots only, untested by hand)
- Wrist page: "Next page" instead of More (+ Add page adds one), page tabs in visit order with arrows, button list on the
  right (on/off, drag reorder, sort, click = select), drag-swap in the picture, Duplicate button / Duplicate page,
  tabs Buttons | All buttons | Advanced settings (Panel + When it shows, On the arm; custom angle under Fine-tune).
- Wrist + wheel items: grouped card picker for "What it does" (ActionEditor WristActions / WheelActions).
- Wheel page (WheelPageEditor.cs) replaces the old wheel editor: sub-wheel tabs + "+ Add sub-wheel", items list,
  "Opened by" tab (switch/delete openers, + Add a button that opens it), All buttons tab (ItemLists.cs library + search).
- Controllers: list's Left/Right rows open the new screen; drawer rows have on/off, duplicate, delete; Reset to default
  per way; touch inputs get Hold / Double-tap-and-hold.
- Rail icon "All bindings" (AllBindingsPage.cs): every binding, search, sort, on/off, duplicate, delete; bindings not
  on the chosen controller's picture listed first. Toolbar acts on nothing on Controllers / All bindings pages.
- Keys on wrist + wheel pages: Del, Ctrl+D, Alt+Left/Right on the chosen button / item.
- Not done (user's call): "Move to another wheel/page" button.

## Tutorials + release target (2026-11-23, late night)
- Tutorial.cs rewritten: menu (toolbar Tutorial = OptionPicker): Start here (full tour, open a wheel from a button),
  How do I... (sprint double flick, hold second job, swap, reuse, desktop on wrist, offsets), About each page (8).
  "Show me this page" on every tip bar (SectionTour). Targets found by on-screen text (FindShown); --tourshots runs
  all 16 tours with Auto actions: ALL TOURS FINISHED. Fixed: a done last step restarted itself.
- --github-shots <dir>: README screenshots from the built-in default (1920x1040, PrintWindow). docs/images replaced +
  new ones; README "Using the editor" rewritten.
- RELEASE TARGET: the user wants the next GitHub release on a branch named **experimental** (prerelease), NOT main.
  Local branch `experimental` created at this commit; nothing pushed. Push + release only after the user has tested.

## Open / asked
- Wrist text-direction presets (fingers / between / along arm) are old and unchecked; user to choose: replace them,
  or place the panel per preset in the headset and measure. The arm drawing is gone.
- editor controller_picture.txt is quest3 again (checked 2026-11-23; earlier it said "questpro", cause unknown).
- Page internal name (for "open another page") only renamable via the list.

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
- Press points + Index grip full squeeze (2026-11-22, late): Binding::pressAt / JSON "press_at" (0 = profile default,
  0.02..0.98; release 0.12 lower) for analog firing inputs (trigger, grip, Index trackpad force, Index grip full).
  Engine::AnalogOf / UpdateBindingPressPoints / TrigDown / TrigPressed (per-binding latch in BState). Menu row "Press point"
  ("Squeeze strength" for the Index full squeeze, Default at the minimum); editor "Press point (%)" shown for analog inputs.
  Index grip: squeeze/force read (A_GRIPF); its F_FULL = force > 0.5 (off 0.35), offered in menu + editor.
  Frame grip: user report = analog squeeze + physical click at the end (= squeeze/click, already F_FULL). Test TestPressPoints.
- Headset test 2026-11-23 fixes: (1) Quest 3 via VD/SteamVR reports plain touch_controller -> drew Quest 2; now the
  editor's pick (controller_picture.txt, read every 2 s) upgrades Touch to TouchPlus / TouchPro. (2) Overview bigger and
  further: MenuPage scale 2.4 + distance 1.4 m (MenuView.scale keeps texture pixels the same); other pages 0.6 m.
  (3) Menu opens toward the playspace's front (STAGE space -Z, yaw only) at the head; falls back to head yaw.
- Headset test 2 fixes (2026-11-23): overview hands spaced apart (x0 0.005 / 0.517, tag col 0.105) so middle tags
  don't touch; stick pages split: StickOptions = hub (Deadzones / Directions / Bindings) with a big live stick
  (span 5); MenuPage/MenuView.stickFree = sticks don't scroll or change values (also Test controllers), footer says so.
  In-game stick ways now include Double flick up/down/left/right (DoubleTapHold, block none, 350 ms), like the editor's
  "+ Add..." on a direction card.
- Editor "+ Add..." (2026-11-23): the dropdown is now a popup of cards (editor/OptionPicker.cs: PickOption group/title/
  does/example, laid out by hand, card height measured + 20 px). Choices in ControllerButtonsPage.AddOptions (AddKind).
  Picture: editor --picker-shot <png> (editor_add_picker.png). Other dropdowns (gesture, block, action type) could use it too.
- Menu keyboard (2026-11-23): when the menu asks for a key, the VR keyboard lies on the menu's pane (KeyboardView.inMenu:
  same facing, 6 mm x scale in front, width min(0.94 w, fits 0.8 h)), menu dimmed (MenuView.covered), no dock bar, not
  moved/saved/docked; closes with the menu. The trigger pull that presses a key doesn't reach the menu row under it
  (InGameMenu kbWasOnMenu_ / kbHeldTrigger_). Test: "the binding keyboard lies inside the menu's pane".
- Editor hub screens (2026-11-23, built into G:/Claude/VR Radial Overlay/dist3): the editor opens on Home (editor/HubPages.cs:
  HomeHub 2x2 tiles -> Controllers / Radial wheels / Wrist panels / Configure features). Hub tags "hub_*" (Hubs) have no
  tree row: MainForm.ShowHub/OnSelect/GoTo/GoBack handle them; the left list is hidden (mainSplit.Panel1Collapsed) on hubs,
  settings pages and the old Overview; BuildTree keeps a hub on screen (keepHub). ControllersHub = both ControllerPictures
  with AddButtons (+ on each tag -> way picker cards -> ControllerButtonsPage.AddFor = the "+ Add..." cards). WheelsHub:
  main / sub-wheels with picture, opener or "Not assigned". FeaturesHub = SettingsPage tiles + "What's on now".
  Key dialog (QuickBindingDialogs.Ask) now auto-laid-out with "OK, use this" (buttons were cut off). Pictures:
  editor --hub-shots <dir> (VRO_SHOTS_PROFILE for another profile), --keydlg-shot, --picker-shot.
- From 2026-11-23 only dist2 is built (user): dist3 / branch controller-buttons is a finished design trial to take parts
  from, not built anymore. Work happens on dist2-line (G:/Claude/OpenXR Companion - Dist2).
- dist2 vs dist3 (2026-11-23): dist2 is built from branch dist2-line (worktree G:/Claude/OpenXR Companion - Dist2):
  5ab37fa + key dialog fix + the list folding into section icons (MainForm.Rail.cs, list_folded.txt, --rail-shot).
  dist3 = the hub redesign (branch controller-buttons). Don't copy controller-buttons editor builds into dist2.
- Compact UI step 1 (2026-11-23, dist3): ControllersHub drawer (Dock Right, 42% width): click a button -> its ways
  (ControllerButtonsPage onlyPart mode), click a binding -> BindingEditor in the drawer (Back / Delete / Close); + on a tag
  finishes in the drawer. ChoiceBox card mode (ChoiceCards.Use: action type, gesture, pass-input) opens OptionPicker with
  descriptions (3 columns above 8 choices). BindingEditor folds the layer rows under "More options". Previews:
  previews/compact-ui-2026-11-23/, editor --actions-shot. Next ideas: drawer for wheels hub (wheel picture + item drawer),
  breadcrumb, tip bars dismissable, toolbar "..." menu, collapse settings cards.
- Compact UI complete (2026-11-23, dist3, previews/compact-ui-2026-11-23/10_..22_): the list is hidden on every screen
  (MainForm.listShown; "List" button shows it, the tutorial turns it on; BuildTree never leaves a screen while hidden:
  keepHub). Wheels / wrist pages / their items open in WheelStudio (Studios.cs: big WheelPreview, tools row, item drawer
  with Move / Duplicate / Delete, wheel settings drawer, "Pick a button to open it" for unassigned wheels; wrist: on /
  mirror switch + page tabs). DrawerPage base (HubPages.cs) shared by ControllersHub and WheelStudio. Configure Left/Right
  controller = side by side (ControllerButtonsPage.SideBySide: stacked picture left, details right, bindings edit in the
  right column). Breadcrumb in the nav bar (CrumbsFor/ShowCrumbs); Duplicate/Delete/Up/Down only with the list. Toolbar:
  Profile, Save, "More" menu (new/delete/revert profile, controllers, tutorial, folders, install/uninstall layer). Tip
  bars have an x (dismissed_tips.txt). Settings pages with > 2 cards fold all but the first (Card.MakeFoldable).
- Double flick release (2026-11-23, dist2 + dist3, layer + editor + headset menu): Binding::releaseAt / JSON "release_at"
  (0 = old: stops on leaving the direction zone / below stick_off). When > 0 and the double_tap_hold on a stick / trackpad
  direction has fired, Engine::TrigDown uses the stick's distance from the middle (any direction). New double flicks
  (editor quick add, headset menu) start at 0.3. Editor row "Keeps going until (%)", headset row "Keeps going until".
  Test TestDoubleFlickRelease. The Dist2 worktree needed third_party/openxr-sdk copied in (a submodule, empty in a new worktree).
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
