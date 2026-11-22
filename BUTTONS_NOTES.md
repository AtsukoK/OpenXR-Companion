# Controller buttons pages (branch controller-buttons)

## What it is
The Right / Left controller entries in the editor's list open a page with a drawing of the controller. Click a
button to see each way of using it (press, touch, stick directions) and what it does now, with quick choices:
press a key, act as another of the game's buttons, turn it off in the game, or something else. Like SteamVR's
controller bindings, but the results are ordinary bindings in the profile.

## Design choices
- No new profile format and no layer change for the pages: the layer already reads press / touch / stick directions,
  fakes game inputs (`xr_input`) and hides buttons (`block`). Quick choices write:
  - key: `while_held`, key `mode: hold`, `block: trigger` (touch: `none`)
  - act as another button: `while_held`, `xr_input` `mode: hold`, `block: trigger`
  - off in the game: `while_held`, action `none`, `block: trigger`
  - "Only when held for a moment" (checkbox per card, not on touch rows): `hold` 400 ms, `tap_passthrough`,
    `block: trigger`: the game never sees the button while held, a quick tap is passed on as a press
    (TestHoldOnlyQuickBinding).
- Touch rows only offer "Press a key" and "Something else": the layer hides whole buttons (not just their touch),
  so a touch binding that hid the button would also hide its presses.
- Pictures are our own simple drawings (data in ControllerLayouts.cs), not the makers' artwork. Two views per hand:
  buttons from above, trigger and grip from the side.
- The picture type is remembered in `%LOCALAPPDATA%\OpenXRCompanion\controller_picture.txt` (editor only); without it,
  the choice in "Your controllers" decides.
- Layer fix found on the way: the HP Reverb G2 had no table of its own (read like the first WMR controllers, so its
  X / Y / A / B never reached the menus). Now `kHpReverb`.

## Checks
- `quick_test.sh`: 0 failures, including `TestQuickBindings` (touch keeps presses, remap swaps, off hides) and
  `HP Reverb G2` (X / Y / A / B suggested).
- `--selftest`: 776 quick bindings (every layout, hand, part, way) pass the layer's parser.
- `--controller-pics`, `--shots`: pictures checked by eye, dark and light.

## Not done / ideas
- Index trackpad and WMR touchpad: not read by our action set (shown greyed). Would need new actions + a Comp.
- Grip touch, trigger force, finger tracking (Index): not modelled.
- Hiding only a button's touch from the game (per feature blocking) would let touch rows offer "turn off".
- Stick direction rows could be one compact card.
- Not tested in a headset or with real clicks through the dialogs.
