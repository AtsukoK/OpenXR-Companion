namespace OpenXRCompanionEditor;

// The controller pictures on the Right / Left controller pages: each controller type's outline and its buttons.
// Drawn for the right hand in two views: Top (looking down at the buttons) and Side (trigger and grip, from the
// thumb's side, front to the left). The left hand is the same picture mirrored.
// Coordinates are design units: the Top view is 200 x 260, the Side view 130 x 260.
// These are simple drawings of our own, not copies of the makers' artwork.

public enum ControllerView { Top, Side }
public enum BodyShape { Ellipse, RoundRect, Ring }
public enum PartShape { Button, Stick, Pad, Pill, Trigger, Grip }

// One piece of a controller's outline. Ring = an ellipse outline `Thickness` wide; a strap is drawn dashed.
public sealed record BodyPiece(ControllerView View, BodyShape Shape, RectangleF Box, float Turn = 0, float Thickness = 0, bool IsStrap = false);

// One button, stick, pad, trigger or grip.
// Comp: the input name used in bindings ("primary", "secondary", "trigger", "grip", "thumbstick", "thumbrest", "menu";
// on the Steam Frame also "button3" / "button4" (its right X / Y), "bumper", "dpad_up" / "dpad_down" / "dpad_left" / "dpad_right"),
// or a part OpenXR Companion can't read ("trackpad", "touchpad", "system"), which then has a NotUsable reason.
// Ways: how it can be used, as the input's last part: "" (press / pull / squeeze / click), "touch", "up", "down", "left", "right".
public sealed record ControllerPart(string Comp, string Label, string LeftLabel, ControllerView View, PartShape Shape, RectangleF Box,
                                    string[] Ways, int Hands = ControllerPart.BothHands, string NotUsable = null, float Turn = 0)
{
    public const int LeftHand = 1, RightHand = 2, BothHands = 3;
    public bool IsUsable => NotUsable == null;
    public bool IsOn(int hand) => (Hands & (hand == 0 ? LeftHand : RightHand)) != 0;
    public string LabelFor(int hand) => hand == 0 && LeftLabel != null ? LeftLabel : Label;
}

// Models: the headsets it stands for, shown under the choice. SetupKind: the matching choice in "Your controllers".
public sealed record ControllerLayout(string Id, string Name, string Models, string SetupKind, BodyPiece[] Body, ControllerPart[] Parts)
{
    public IEnumerable<ControllerPart> PartsFor(int hand) => Parts.Where(p => p.IsOn(hand));
}

public static class ControllerLayouts
{
    public const float TopWidth = 200, SideWidth = 130, Height = 260;

    static readonly string[] Press = { "" };
    static readonly string[] PressTouch = { "", "touch" };
    static readonly string[] StickWays = { "", "touch", "up", "down", "left", "right" };
    static readonly string[] StickNoTouch = { "", "up", "down", "left", "right" };

    const string SystemButton = "This button opens the headset's own menu, so games and OpenXR Companion never see it.";

    static RectangleF R(float x, float y, float w, float h) => new(x, y, w, h);

    // Side view shared by most controllers: a head with the trigger at the front, a handle with the grip.
    static BodyPiece[] SideOfTouch() => new[]
    {
        new BodyPiece(ControllerView.Side, BodyShape.Ellipse, R(16, 22, 108, 74), Turn: -18),
        new BodyPiece(ControllerView.Side, BodyShape.RoundRect, R(46, 82, 52, 168), Turn: -10),
    };
    static ControllerPart TriggerOf(string[] ways) => new("trigger", "Trigger", null, ControllerView.Side, PartShape.Trigger, R(8, 80, 26, 44), ways);
    static ControllerPart GripOf() => new("grip", "Grip", null, ControllerView.Side, PartShape.Grip, R(30, 138, 24, 74), Press, Turn: -10);

    public static readonly ControllerLayout[] All =
    {
        new("quest3", "Meta Quest 3 / 3S (Touch Plus)", "Quest 3, Quest 3S", "touch",
            new[]
            {
                new BodyPiece(ControllerView.Top, BodyShape.Ellipse, R(30, 40, 146, 110), Turn: -10),
                new BodyPiece(ControllerView.Top, BodyShape.RoundRect, R(74, 126, 56, 128), Turn: 6),
            }.Concat(SideOfTouch()).ToArray(),
            new ControllerPart[]
            {
                new("thumbstick", "Thumbstick", null, ControllerView.Top, PartShape.Stick, R(48, 66, 38, 38), StickWays),
                new("secondary", "B", "Y", ControllerView.Top, PartShape.Button, R(126, 58, 22, 22), PressTouch),
                new("primary", "A", "X", ControllerView.Top, PartShape.Button, R(112, 88, 22, 22), PressTouch),
                new("thumbrest", "Thumbrest", null, ControllerView.Top, PartShape.Pill, R(148, 84, 14, 32), Press),
                new("menu", "Menu", null, ControllerView.Top, PartShape.Button, R(72, 114, 14, 14), Press, ControllerPart.LeftHand),
                new("system", "Meta button", null, ControllerView.Top, PartShape.Button, R(72, 114, 14, 14), Press, ControllerPart.RightHand, SystemButton),
                TriggerOf(PressTouch),
                GripOf(),
            }),
        new("quest2", "Meta Quest 2 (Touch with a ring)", "Quest 2", "touch",
            new[]
            {
                new BodyPiece(ControllerView.Top, BodyShape.Ring, R(18, 6, 156, 112), Turn: -10, Thickness: 16),
                new BodyPiece(ControllerView.Top, BodyShape.Ellipse, R(36, 78, 134, 88), Turn: -12),
                new BodyPiece(ControllerView.Top, BodyShape.RoundRect, R(76, 142, 52, 112), Turn: 8),
            }.Concat(SideOfTouch()).ToArray(),
            new ControllerPart[]
            {
                new("thumbstick", "Thumbstick", null, ControllerView.Top, PartShape.Stick, R(50, 94, 36, 36), StickWays),
                new("secondary", "B", "Y", ControllerView.Top, PartShape.Button, R(124, 90, 20, 20), PressTouch),
                new("primary", "A", "X", ControllerView.Top, PartShape.Button, R(110, 116, 20, 20), PressTouch),
                new("thumbrest", "Thumbrest", null, ControllerView.Top, PartShape.Pill, R(144, 110, 13, 30), Press),
                new("menu", "Menu", null, ControllerView.Top, PartShape.Button, R(76, 138, 13, 13), Press, ControllerPart.LeftHand),
                new("system", "Meta button", null, ControllerView.Top, PartShape.Button, R(76, 138, 13, 13), Press, ControllerPart.RightHand, SystemButton),
                TriggerOf(PressTouch),
                GripOf(),
            }),
        new("rifts", "Meta Rift S / Quest 1 (Touch)", "Rift S, Quest 1", "touch",
            new[]
            {
                new BodyPiece(ControllerView.Top, BodyShape.Ring, R(18, 6, 156, 112), Turn: -10, Thickness: 16),
                new BodyPiece(ControllerView.Top, BodyShape.Ellipse, R(36, 78, 134, 88), Turn: -12),
                new BodyPiece(ControllerView.Top, BodyShape.RoundRect, R(76, 142, 52, 112), Turn: 8),
            }.Concat(SideOfTouch()).ToArray(),
            new ControllerPart[]
            {
                new("thumbstick", "Thumbstick", null, ControllerView.Top, PartShape.Stick, R(50, 94, 36, 36), StickWays),
                new("secondary", "B", "Y", ControllerView.Top, PartShape.Button, R(124, 90, 20, 20), PressTouch),
                new("primary", "A", "X", ControllerView.Top, PartShape.Button, R(110, 116, 20, 20), PressTouch),
                new("thumbrest", "Thumbrest", null, ControllerView.Top, PartShape.Pill, R(144, 110, 13, 30), Press),
                new("menu", "Menu", null, ControllerView.Top, PartShape.Button, R(76, 138, 13, 13), Press, ControllerPart.LeftHand),
                new("system", "Meta button", null, ControllerView.Top, PartShape.Button, R(76, 138, 13, 13), Press, ControllerPart.RightHand, SystemButton),
                TriggerOf(PressTouch),
                GripOf(),
            }),
        new("questpro", "Meta Quest Pro (Touch Pro)", "Quest Pro", "touch",
            new[]
            {
                new BodyPiece(ControllerView.Top, BodyShape.Ellipse, R(36, 44, 132, 100), Turn: -8),
                new BodyPiece(ControllerView.Top, BodyShape.RoundRect, R(78, 118, 48, 138), Turn: 4),
            }.Concat(SideOfTouch()).ToArray(),
            new ControllerPart[]
            {
                new("thumbstick", "Thumbstick", null, ControllerView.Top, PartShape.Stick, R(52, 68, 34, 34), StickWays),
                new("secondary", "B", "Y", ControllerView.Top, PartShape.Button, R(122, 60, 20, 20), PressTouch),
                new("primary", "A", "X", ControllerView.Top, PartShape.Button, R(110, 88, 20, 20), PressTouch),
                new("thumbrest", "Thumbrest", null, ControllerView.Top, PartShape.Pill, R(142, 84, 13, 30), Press),
                new("menu", "Menu", null, ControllerView.Top, PartShape.Button, R(74, 112, 13, 13), Press, ControllerPart.LeftHand),
                new("system", "Meta button", null, ControllerView.Top, PartShape.Button, R(74, 112, 13, 13), Press, ControllerPart.RightHand, SystemButton),
                TriggerOf(PressTouch),
                GripOf(),
            }),
        new("pico4", "Pico 4 / Neo 3", "Pico 4, Pico 4 Ultra, Neo 3 (as Touch-style controllers)", "touch",
            new[]
            {
                new BodyPiece(ControllerView.Top, BodyShape.Ring, R(96, 10, 96, 160), Turn: 16, Thickness: 14),
                new BodyPiece(ControllerView.Top, BodyShape.Ellipse, R(28, 64, 132, 94), Turn: -10),
                new BodyPiece(ControllerView.Top, BodyShape.RoundRect, R(70, 136, 54, 118), Turn: 6),
            }.Concat(SideOfTouch()).ToArray(),
            new ControllerPart[]
            {
                new("thumbstick", "Thumbstick", null, ControllerView.Top, PartShape.Stick, R(44, 84, 36, 36), StickWays),
                new("secondary", "B", "Y", ControllerView.Top, PartShape.Button, R(116, 78, 20, 20), PressTouch),
                new("primary", "A", "X", ControllerView.Top, PartShape.Button, R(104, 106, 20, 20), PressTouch),
                new("menu", "Menu", null, ControllerView.Top, PartShape.Button, R(68, 128, 13, 13), Press, ControllerPart.LeftHand),
                new("system", "Pico button", null, ControllerView.Top, PartShape.Button, R(68, 128, 13, 13), Press, ControllerPart.RightHand, SystemButton),
                TriggerOf(PressTouch),
                GripOf(),
            }),
        new("frame", "Steam Frame", "Steam Frame controllers (for all their buttons, tick the box on the right)", "frame",
            new[]
            {
                new BodyPiece(ControllerView.Top, BodyShape.Ellipse, R(28, 14, 146, 132)),
                new BodyPiece(ControllerView.Top, BodyShape.RoundRect, R(66, 118, 52, 136), Turn: 4),
                new BodyPiece(ControllerView.Side, BodyShape.Ellipse, R(14, 12, 104, 92), Turn: -12),
                new BodyPiece(ControllerView.Side, BodyShape.RoundRect, R(48, 84, 50, 166), Turn: -6),
            },
            new ControllerPart[]
            {
                new("thumbstick", "Thumbstick", null, ControllerView.Top, PartShape.Stick, R(44, 62, 42, 42), StickWays),
                new("button4", "Y", null, ControllerView.Top, PartShape.Button, R(120, 38, 19, 19), Press, ControllerPart.RightHand),
                new("button3", "X", null, ControllerView.Top, PartShape.Button, R(102, 58, 19, 19), Press, ControllerPart.RightHand),
                new("secondary", "B", null, ControllerView.Top, PartShape.Button, R(138, 58, 19, 19), PressTouch, ControllerPart.RightHand),
                new("primary", "A", null, ControllerView.Top, PartShape.Button, R(120, 78, 19, 19), PressTouch, ControllerPart.RightHand),
                new("dpad_up", "D-pad up", null, ControllerView.Top, PartShape.Button, R(121, 38, 16, 20), Press, ControllerPart.LeftHand),
                new("dpad_down", "D-pad down", null, ControllerView.Top, PartShape.Button, R(121, 76, 16, 20), Press, ControllerPart.LeftHand),
                new("dpad_left", "D-pad left", null, ControllerView.Top, PartShape.Button, R(136, 59, 20, 16), Press, ControllerPart.LeftHand),
                new("dpad_right", "D-pad right", null, ControllerView.Top, PartShape.Button, R(102, 59, 20, 16), Press, ControllerPart.LeftHand),
                new("menu", "Menu", "View", ControllerView.Top, PartShape.Pill, R(74, 42, 20, 8), Press),
                new("system", "Steam button", null, ControllerView.Top, PartShape.Button, R(108, 104, 15, 15), Press, NotUsable: SystemButton),
                new("bumper", "Bumper", null, ControllerView.Side, PartShape.Pill, R(12, 40, 30, 12), Press, Turn: -30),
                new("trigger", "Trigger", null, ControllerView.Side, PartShape.Trigger, R(8, 64, 26, 44), PressTouch),
                new("grip", "Grip", null, ControllerView.Side, PartShape.Grip, R(32, 128, 24, 80), Press, Turn: -6),
            }),
        new("index", "Valve Index (and Index-style)", "Valve Index; Bigscreen, Pimax with Index-style controllers; Steam Frame without the Steam Frame setting", "index",
            new[]
            {
                new BodyPiece(ControllerView.Top, BodyShape.RoundRect, R(60, 12, 80, 156)),
                new BodyPiece(ControllerView.Top, BodyShape.RoundRect, R(70, 140, 60, 116), Turn: 3),
                new BodyPiece(ControllerView.Top, BodyShape.RoundRect, R(138, 36, 24, 210), IsStrap: true),
                new BodyPiece(ControllerView.Side, BodyShape.Ellipse, R(24, 10, 88, 80)),
                new BodyPiece(ControllerView.Side, BodyShape.RoundRect, R(44, 60, 48, 192), Turn: -6),
                new BodyPiece(ControllerView.Side, BodyShape.RoundRect, R(84, 50, 22, 196), Turn: -6, IsStrap: true),
            },
            new ControllerPart[]
            {
                new("thumbstick", "Thumbstick", null, ControllerView.Top, PartShape.Stick, R(80, 24, 36, 36), StickWays),
                new("secondary", "B", null, ControllerView.Top, PartShape.Button, R(70, 72, 17, 17), PressTouch),
                new("primary", "A", null, ControllerView.Top, PartShape.Button, R(76, 96, 17, 17), PressTouch),
                new("trackpad", "Trackpad", null, ControllerView.Top, PartShape.Pad, R(98, 68, 28, 66), Press,
                    NotUsable: "OpenXR Companion doesn't read the Index trackpad yet. Games still get it as usual."),
                new("system", "System", null, ControllerView.Top, PartShape.Button, R(93, 144, 13, 13), Press, NotUsable: SystemButton),
                new("trigger", "Trigger", null, ControllerView.Side, PartShape.Trigger, R(8, 52, 26, 42), PressTouch),
                new("grip", "Grip (squeeze)", null, ControllerView.Side, PartShape.Grip, R(26, 106, 24, 132), Press, Turn: -6),
            }),
        new("vive", "HTC Vive wands", "HTC Vive, Vive Pro (wand controllers)", "vive",
            new[]
            {
                new BodyPiece(ControllerView.Top, BodyShape.Ring, R(38, 4, 124, 104), Thickness: 22),
                new BodyPiece(ControllerView.Top, BodyShape.RoundRect, R(66, 92, 68, 164)),
                new BodyPiece(ControllerView.Side, BodyShape.Ellipse, R(16, 6, 104, 74)),
                new BodyPiece(ControllerView.Side, BodyShape.RoundRect, R(44, 58, 54, 196), Turn: -4),
            },
            new ControllerPart[]
            {
                new("menu", "Menu", null, ControllerView.Top, PartShape.Button, R(91, 104, 18, 18), Press),
                new("thumbstick", "Trackpad", null, ControllerView.Top, PartShape.Pad, R(70, 128, 60, 60), StickWays),
                new("system", "System", null, ControllerView.Top, PartShape.Button, R(93, 200, 14, 14), Press, NotUsable: SystemButton),
                new("trigger", "Trigger", null, ControllerView.Side, PartShape.Trigger, R(18, 80, 26, 46), Press),
                new("grip", "Grip buttons", null, ControllerView.Side, PartShape.Grip, R(48, 160, 20, 46), Press, Turn: -4),
            }),
        new("wmr", "Windows Mixed Reality", "Samsung Odyssey, HP Reverb G1, Acer, Dell, Lenovo (WMR controllers)", "wmr",
            new[]
            {
                new BodyPiece(ControllerView.Top, BodyShape.Ring, R(12, 6, 176, 112), Thickness: 20),
                new BodyPiece(ControllerView.Top, BodyShape.RoundRect, R(68, 96, 64, 160)),
                new BodyPiece(ControllerView.Side, BodyShape.Ellipse, R(20, 8, 100, 84)),
                new BodyPiece(ControllerView.Side, BodyShape.RoundRect, R(46, 68, 52, 186), Turn: -8),
            },
            new ControllerPart[]
            {
                new("thumbstick", "Thumbstick", null, ControllerView.Top, PartShape.Stick, R(70, 108, 32, 32), StickNoTouch),
                new("touchpad", "Touchpad", null, ControllerView.Top, PartShape.Pad, R(104, 110, 28, 28), Press,
                    NotUsable: "OpenXR Companion doesn't read the WMR touchpad yet. Games still get it as usual."),
                new("menu", "Menu", null, ControllerView.Top, PartShape.Button, R(92, 156, 16, 16), Press),
                new("system", "Windows button", null, ControllerView.Top, PartShape.Button, R(92, 184, 16, 16), Press, NotUsable: SystemButton),
                new("trigger", "Trigger", null, ControllerView.Side, PartShape.Trigger, R(10, 76, 26, 44), Press),
                new("grip", "Grab (grip)", null, ControllerView.Side, PartShape.Grip, R(30, 136, 24, 70), Press, Turn: -8),
            }),
        new("reverbg2", "HP Reverb G2", "HP Reverb G2", "wmr",
            new[]
            {
                new BodyPiece(ControllerView.Top, BodyShape.Ring, R(16, 6, 168, 110), Turn: -8, Thickness: 16),
                new BodyPiece(ControllerView.Top, BodyShape.Ellipse, R(38, 82, 128, 84), Turn: -10),
                new BodyPiece(ControllerView.Top, BodyShape.RoundRect, R(76, 140, 52, 114), Turn: 6),
            }.Concat(SideOfTouch()).ToArray(),
            new ControllerPart[]
            {
                new("thumbstick", "Thumbstick", null, ControllerView.Top, PartShape.Stick, R(52, 98, 34, 34), StickNoTouch),
                new("secondary", "B", "Y", ControllerView.Top, PartShape.Button, R(124, 92, 20, 20), Press),
                new("primary", "A", "X", ControllerView.Top, PartShape.Button, R(110, 118, 20, 20), Press),
                new("menu", "Menu", null, ControllerView.Top, PartShape.Button, R(76, 140, 13, 13), Press),
                new("system", "Windows button", null, ControllerView.Top, PartShape.Button, R(96, 146, 13, 13), Press, NotUsable: SystemButton),
                TriggerOf(Press),
                GripOf(),
            }),
    };

    public static ControllerLayout ById(string id) => All.FirstOrDefault(l => l.Id == id) ?? All[0];

    // The picture that fits a choice made in "Your controllers" (touch / index / wmr / vive).
    public static string ForSetupKind(string kind) => All.FirstOrDefault(l => l.SetupKind == kind)?.Id ?? All[0].Id;

    // The input text of one way to use a part: "right.a.touch", "left.thumbstick.up", "right.trigger".
    public static string InputOf(int hand, ControllerPart part, string way)
        => InputText.Join(hand == 0 ? "left" : "right", way.Length == 0 ? part.Comp : part.Comp + "." + way);

    // "Press", "Touch (no press)", "Push up"... in words that fit the part.
    public static string WayName(ControllerPart part, string way)
    {
        bool isPad = part.Shape == PartShape.Pad;
        return way switch
        {
            "" => part.Comp switch
            {
                "trigger" => "Pull",
                "grip" => "Squeeze",
                "thumbstick" => isPad ? "Click (press the pad)" : "Click (press the stick in)",
                "thumbrest" => "Touch",
                _ => "Press",
            },
            "touch" => part.Comp == "thumbrest" ? "Touch" : "Touch (resting a finger, no press)",
            "up" => isPad ? "Thumb on the top of the pad" : "Push up",
            "down" => isPad ? "Thumb on the bottom of the pad" : "Push down",
            "left" => isPad ? "Thumb on the left of the pad" : "Push left",
            "right" => isPad ? "Thumb on the right of the pad" : "Push right",
            _ => way,
        };
    }
}
