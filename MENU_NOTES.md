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
