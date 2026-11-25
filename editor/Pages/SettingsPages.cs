using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;
using Avalonia.LogicalTree;
using static OverviewPrototype.Fields;

namespace OverviewPrototype;

// "Configure features": a tile per settings page (WinForms FeatureTiles), opening settings:<key>.
public class SettingsHubPage : UserControl
{
    public SettingsHubPage()
    {
        var body = new StackPanel { Margin = new Thickness(16, 12), Spacing = 6 };
        body.Children.Add(new TextBlock { Text = "Configure features", FontSize = 24, FontWeight = FontWeight.SemiBold });
        body.Children.Add(new TextBlock { Classes = { "desc" }, Text = "Pick a feature: only its settings open, using the whole window. Back returns here." });
        var tiles = new WrapPanel();
        foreach (var sp in SettingsPage.All.Where(x => x.Key != "experimental" || SettingsPageView.ExperimentalListed))
        {
            var t = new Button
            {
                Classes = { "option" }, Width = 300, Height = 172, Margin = new Thickness(0, 0, 12, 12), // one height for all (the longest has 2 lines + "Applies straight away")
                HorizontalContentAlignment = HorizontalAlignment.Left, VerticalContentAlignment = VerticalAlignment.Top,
                Content = new StackPanel
                {
                    Spacing = 6,
                    Children =
                    {
                        new TextBlock { Text = sp.Icon, FontFamily = "Segoe MDL2 Assets", FontSize = 28, Foreground = Brush.Parse("#4A90F0"), Margin = new Thickness(0, 0, 0, 10) },
                        new TextBlock { Text = sp.Title, FontSize = 18, FontWeight = FontWeight.SemiBold },
                        new TextBlock { Text = sp.Sub, Foreground = Brush.Parse("#AAB2BF"), TextWrapping = TextWrapping.Wrap },
                        new TextBlock { Text = sp.Live ? "Applies straight away" : null, IsVisible = sp.Live, FontWeight = FontWeight.SemiBold, Foreground = Brush.Parse("#AAB2BF") },
                    },
                },
            };
            string key = sp.Key;
            t.Click += (s, e) => Shell.Go("settings:" + key);
            tiles.Children.Add(t);
        }
        body.Children.Add(tiles);
        Content = new ScrollViewer { Content = body };
    }
}

// One settings page. Port of the WinForms SettingsEditor's Build* pages that are saved with the profile.
public partial class SettingsPageView : UserControl
{
    static readonly Choice[] BlockOpen =
    {
        new("hand", "Everything on the hand(s) using the wheel"), new("minimal", "Only the stick and the confirm / back buttons"),
        new("all", "Both controllers completely"), new("none", "Nothing, the game still sees every button"),
    };
    static readonly Choice[] Methods =
    {
        new("thumbstick", "Push the thumbstick toward an item"), new("point", "Point the controller at an item"),
        new("both", "Either: thumbstick or pointing"),
    };
    static readonly Choice[] SelHands = { new("opener", "The hand that opened the wheel"), new("left", "Always the left hand"), new("right", "Always the right hand") };
    static readonly Choice[] Comps =
    {
        new("trigger", "Trigger"), new("grip", "Grip"), new("primary", "A / X"), new("secondary", "B / Y"),
        new("thumbstick", "Thumbstick click"), new("menu", "Menu button"),
    };

    readonly ProfileModel p = Shell.Store.Profile;
    public static readonly string[] Ported = { "general", "wheel", "pointing", "keyboard", "screen", "offsets", "advanced", "mirror", "experimental" };

    public SettingsPageView(string key)
    {
        EnsureParts(p);
        var sp = SettingsPage.ByKey(key);
        var body = new StackPanel { Margin = new Thickness(16, 12), Spacing = 10, MaxWidth = 1400, HorizontalAlignment = HorizontalAlignment.Left };
        body.Children.Add(new TextBlock { Text = sp.Title, FontSize = 24, FontWeight = FontWeight.SemiBold });
        string help = key switch
        {
            "wheel" => "Where the radial wheel appears, how it looks, and how you pick an item on it.",
            "pointing" => "The laser you point at wheels, wrist panels and the mirrored screen with.",
            "screen" => "A desktop monitor or one window, shown in VR on your wrist or in front of you.",
            "keyboard" or "advanced" => null,
            "experimental" => "Unfinished features, all off until you switch them on. For every game and profile.",
            "mirror" => "A window on your desktop that shows what the headset shows, wheels and panels included. For you, for OBS, or for people watching.",
            "offsets" => "Turns and moves the controllers the GAME sees, for when its hands or guns point too low or too high.",
            _ => "How the overlay behaves in this game.",
        };
        if (help != null)
            body.Children.Add(new TextBlock { Classes = { "desc" }, Text = help });
        switch (sp.Key)
        {
            case "general": body.Children.Add(Cards.Make("General", General())); break;
            case "wheel":
                body.Children.Add(new WrapPanel { Children = { Cards.Make("Position and look", WheelLook()), new CalibrationTip("wheel", "wheel", "bigger / smaller") { Margin = new Thickness(14, 0, 0, 0) } } });
                body.Children.Add(Cards.Make("Picking items", Picking()));
                break;
            case "pointing": body.Children.Add(Cards.Make("Laser and fingertip", Pointing())); break;
            case "keyboard": body.Children.Add(new WrapPanel { Children = { Cards.Make("VR keyboard", Keyboard()), new CalibrationTip("keyboard", "keyboard", "wider / narrower") { Margin = new Thickness(14, 0, 0, 0) } } }); break;
            case "screen": body.Children.Add(new WrapPanel { Children = { Cards.Make("Screen mirror", Screen()), new CalibrationTip("screen", "screen", "bigger / smaller") { Margin = new Thickness(14, 0, 0, 0) } } }); break;
            case "offsets": body.Children.Add(Cards.Make("Controller offsets", Offsets())); break;
            case "mirror": body.Children.Add(Cards.Make("Headset mirror", HeadsetMirror())); break;
            case "experimental": body.Children.Add(Cards.Make("⚠ Experimental", Experimental())); break;
            case "advanced": body.Children.Add(Cards.Make("Advanced", Advanced())); break;
            default:
                body.Children.Add(new TextBlock { Classes = { "desc" }, Text = "Not ported to Avalonia yet. Use the WinForms editor for this page." });
                break;
        }
        Content = new ScrollViewer { Content = body };
        // opened from a search: light up the rows with the word, and show the first one
        string q = Highlight;
        Highlight = null;
        if (!string.IsNullOrWhiteSpace(q))
        {
            var hits = body.GetLogicalDescendants().OfType<Grid>()
                .Where(g => g.Parent is Form && WordsIn(g).Contains(q, StringComparison.OrdinalIgnoreCase)).ToList();
            foreach (var g in hits)
                g.Background = Brush.Parse("#5A4A1A");
            if (hits.Count > 0)
                Avalonia.Threading.Dispatcher.UIThread.Post(() => hits[0].BringIntoView(), Avalonia.Threading.DispatcherPriority.Background);
        }
    }

    // ---- search: every word on a page (labels, tick boxes, help), like the WinForms SettingsEditor.WordsOf
    public static string Highlight; // set before opening a settings page from a search

    static string WordsIn(Avalonia.LogicalTree.ILogical root)
    {
        var sb = new System.Text.StringBuilder();
        foreach (var c in root.GetLogicalDescendants().OfType<Control>().Prepend(root as Control))
        {
            if (c is TextBlock t) sb.Append(' ').Append(t.Text);
            if (c is ContentControl { Content: string s }) sb.Append(' ').Append(s);
            switch (c != null ? ToolTip.GetTip(c) : null)
            {
                case string tip: sb.Append(' ').Append(tip); break;
                case TextBlock tb: sb.Append(' ').Append(tb.Text); break;
            }
        }
        return sb.ToString();
    }

    static readonly Dictionary<string, string> words = new();

    // Built once per page (for the profile shown when first asked).
    public static string WordsOf(string key)
    {
        if (!words.TryGetValue(key, out var w))
        {
            try { w = SettingsPage.ByKey(key).Title + " " + WordsIn(new SettingsPageView(key)); }
            catch (Exception) { w = SettingsPage.ByKey(key).Title; }
            words[key] = w;
        }
        return w;
    }

    Form General()
    {
        var f = new Form { LabelWidth = 260 };
        f.AddRow("", Check("Overlay is on for this game", () => p.Enabled, v => p.Enabled = v));
        f.AddRow("While a wheel is open, hide from the game", Pick(BlockOpen, () => p.BlockWhileOpen, v => p.BlockWhileOpen = v),
            "So pressing buttons to use the wheel doesn't also do things in the game.");
        f.AddRow("", Check("Vibrate when pointing at and picking items", () => p.Haptics, v => p.Haptics = v));
        f.AddRow("Vibration strength", Num(0, 1, 0.05m, 2, () => p.HapticStrength, v => p.HapticStrength = v));
        // Updates: an app-wide setting of the WinForms editor (not in the profile); not shown in the port yet.
        f.AddRow("", Check("Show messages in the headset (layer on/off, profile reloaded, errors)", () => p.Notifications, v => p.Notifications = v));
        f.AddRow("", MCheck("Check GitHub for a newer version when the editor starts", () => UpdateCheckOn, v => UpdateCheckOn = v),
            "Only reads GitHub's public list of releases; nothing about you is sent. For every profile (not saved with this one).");
        return f;
    }

    Form WheelLook()
    {
        var s = p.Style;
        var col = s.Colors;
        var f = new Form { LabelWidth = 260 };
        Control lHandUp = null, lHandToward = null, lHeadDist = null, lHeadDown = null;
        void SyncAnchor()
        {
            bool hand = s.Anchor is "hand" or "hand_locked";
            Form.Show(lHandUp, hand);
            Form.Show(lHandToward, hand);
            Form.Show(lHeadDist, !hand);
            Form.Show(lHeadDown, !hand);
        }
        f.AddRow("Where the wheel appears", Pick(EditorApi.Anchors, () => s.Anchor, v => { s.Anchor = v; SyncAnchor(); }));
        lHandUp = f.AddRow("Height above hand (m)", Num(0, 0.5m, 0.01m, 2, () => s.HandUp, v => s.HandUp = v));
        lHandToward = f.AddRow("Closer to my face by (m)", Num(-0.3m, 0.3m, 0.01m, 2, () => s.HandToward, v => s.HandToward = v));
        lHeadDist = f.AddRow("Distance in front (m)", Num(0.2m, 2, 0.05m, 2, () => s.HeadDistance, v => s.HeadDistance = v));
        lHeadDown = f.AddRow("Below eye level (m)", Num(-0.5m, 0.8m, 0.01m, 2, () => s.HeadDown, v => s.HeadDown = v));
        f.AddRow("Placed by hand", ResetPose(() => s.Poses is { Count: > 0 }, () => s.Poses = null),
            "Adjust positions in the headset: grab the wheel with a grip and put it anywhere, at any angle. That spot replaces the distance and height numbers here.");
        f.AddRow("Wheel size (radius, m)", Num(0.05m, 0.5m, 0.01m, 2, () => s.Radius, v => s.Radius = v), "0.12 m makes a wheel about 24 cm across.");
        f.AddRow("Text size", Num(0.5m, 2.5m, 0.1m, 1, () => s.FontScale, v => s.FontScale = v));
        f.AddRow("Font", Text(() => s.Font, v => s.Font = v));
        var colours = new WrapPanel { MaxWidth = 560 };
        colours.Children.Add(Colour("Background", () => col.Background, v => col.Background = v));
        colours.Children.Add(Colour("Segments", () => col.Segment, v => col.Segment = v));
        colours.Children.Add(Colour("Pointed at", () => col.Hover, v => col.Hover = v));
        colours.Children.Add(Colour("Text", () => col.Text, v => col.Text = v));
        colours.Children.Add(Colour("Accent", () => col.Accent, v => col.Accent = v));
        colours.Children.Add(Colour("Outline", () => col.Border, v => col.Border = v));
        colours.Children.Add(Colour("Toggled on", () => col.ToggledOn, v => col.ToggledOn = v));
        f.AddRow("Colours", colours, "Click a colour to pick it, or type it as #RRGGBB (the transparency is kept) or #RRGGBBAA.");
        SyncAnchor();
        return f;
    }

    Form Picking()
    {
        var sel = p.Selection;
        var f = new Form { LabelWidth = 260 };
        Control lDead = null;
        f.AddRow("Pick items by", Pick(Methods, () => sel.Method, v => { sel.Method = v; Form.Show(lDead, v != "point"); }));
        f.AddRow("Selecting hand", Pick(SelHands, () => sel.Hand, v => sel.Hand = v));
        lDead = f.AddRow("Stick deadzone", Num(0.1m, 0.95m, 0.05m, 2, () => sel.Deadzone, v => sel.Deadzone = v), "How far the stick must be pushed before an item is chosen.");
        Form.Show(lDead, sel.Method != "point");
        f.AddRow("Confirm button", Pick(Comps, () => sel.ConfirmButton, v => sel.ConfirmButton = v), "Always picks the item you point at, whatever the wheel's pick mode.");
        f.AddRow("Back / close button", Pick(Comps, () => sel.CancelButton, v => sel.CancelButton = v));
        f.AddRow("", Check("Close the wheel after picking an item", () => sel.CloseAfterSelect, v => sel.CloseAfterSelect = v));
        f.AddRow("Open submenus by resting (ms)", Int(0, 3000, 50, () => sel.SubmenuDwellMs, v => sel.SubmenuDwellMs = v), "For 'let go of the button' wheels. 0 = off.");
        f.AddRow("Close by itself after (ms)", Int(0, 60000, 500, () => sel.AutoCloseMs, v => sel.AutoCloseMs = v), "0 = never.");
        return f;
    }

    Form Pointing()
    {
        var ptr = p.Pointer;
        var f = new Form { LabelWidth = 260 };
        Control lPitch = null, lDown = null, lFwd = null;
        void SyncCorr() { bool on = ptr.Correction != "off"; Form.Show(lPitch, on); Form.Show(lDown, on); Form.Show(lFwd, on); }
        f.AddRow("Correct the laser direction", Pick(new Choice[] { new("auto", "Only on Quest (Touch) controllers"), new("always", "On every controller"), new("off", "Off, use the game's controller ray") },
            () => ptr.Correction, v => { ptr.Correction = v; SyncCorr(); }),
            "Quest controllers aim from above your index finger and about 15 degrees upward. This makes the laser go where a pointing finger goes.");
        lPitch = f.AddRow("Turn the laser down by (deg)", Num(-45, 45, 1, 0, () => ptr.PitchDownDeg, v => ptr.PitchDownDeg = v), "15 matches most people on Quest. Saved changes apply in the headset within a second.");
        lDown = f.AddRow("Move the laser start down (m)", Num(-0.1m, 0.1m, 0.005m, 3, () => ptr.DownM, v => ptr.DownM = v), "Toward your index finger. Also moves the poking fingertip.");
        lFwd = f.AddRow("Move the laser start forward (m)", Num(-0.1m, 0.1m, 0.005m, 3, () => ptr.ForwardM, v => ptr.ForwardM = v));
        f.AddRow("", Check("Show a fingertip near wrist panels (where your poke lands and the laser starts)", () => ptr.Finger, v => ptr.Finger = v));
        f.AddRow("Fingertip forward (m)", Num(-0.1m, 0.15m, 0.005m, 3, () => ptr.FingertipForwardM, v => ptr.FingertipForwardM = v),
            "Where your poke lands on wrist panels, measured from the laser start. Raise it if buttons press before your finger reaches them, lower it if you have to push through. Saved changes apply in the headset within a second.");
        f.AddRow("Fingertip up (m)", Num(-0.1m, 0.1m, 0.005m, 3, () => ptr.FingertipUpM, v => ptr.FingertipUpM = v));
        f.AddRow("Fingertip toward the middle (m)", Num(-0.1m, 0.1m, 0.005m, 3, () => ptr.FingertipInwardM, v => ptr.FingertipInwardM = v),
            "Sideways, mirrored for each hand: positive moves both fingertips toward your body's middle.");
        f.AddRow("Steady the laser", Num(0, 1, 0.05m, 2, () => ptr.Smoothing, v => ptr.Smoothing = v),
            "For shaky hands: smooths the laser (and the mouse on the mirrored screen) while you hold still or move slowly; quick movements still follow straight away. 0 = off, 0.5 = a good start, 1 = strongest.");
        f.AddRow("", Check("Keep a short laser showing while a wrist panel is up", () => ptr.GuideLaser, v => ptr.GuideLaser = v),
            "So you can find your pointer before it reaches the panel. It hands over to the touch ring when your fingertip gets close.");
        SyncCorr();
        return f;
    }

    Form Keyboard()
    {
        var kb = p.Keyboard;
        var f = new Form { LabelWidth = 260 };
        f.AddRow("Placed by hand", ResetPose(() => kb.Pose != null, () => kb.Pose = null),
            "Adjust positions in the headset (Calibrate Positions): open the keyboard, grab it with a grip and put it anywhere; the stick changes its width. That spot replaces the distance and height numbers below.");
        f.AddRow("Width (m)", Num(0.2m, 1.5m, 0.02m, 2, () => kb.Width, v => kb.Width = v));
        f.AddRow("Distance in front (m)", Num(0.2m, 1.5m, 0.02m, 2, () => kb.Distance, v => kb.Distance = v));
        f.AddRow("Below eye level (m)", Num(-0.3m, 0.8m, 0.02m, 2, () => kb.Below, v => kb.Below = v));
        f.AddRow("", Check("Close the keyboard after Enter (handy for chat boxes)", () => kb.CloseOnEnter, v => kb.CloseOnEnter = v));
        f.AddRow("Screen docking", Pick(new Choice[] { new("none", "Apart (each in its own place)"), new("screen_to_keyboard", "The screen sits above the keyboard"), new("keyboard_to_screen", "The keyboard sits under the screen") },
            () => p.Dock?.Mode ?? "none", v => { p.Dock ??= new DockModel(); p.Dock.Mode = v; }),
            "Easier typing into a mirrored window: the two move together. Also on the bar under the keyboard in the headset (Dock screen above keys / Dock keys under screen), where 'Adjust dock' lets you grab the docked one and put it where you like.");
        f.AddRow("Dock placed by hand", ResetPose(() => p.Dock?.Pose != null, () => { if (p.Dock != null) { p.Dock.Pose = null; p.Dock.Width = 0; } }, null,
            "No, the default spot (screen just above the keys)", "Back to the default spot"));
        return f;
    }

    Form Screen()
    {
        var sc = p.Screen;
        var f = new Form { LabelWidth = 260 };
        Control lWW = null, lTE = null, lLift = null, lSide = null, lRot2 = null, lFace = null, lFW = null, lFD = null, lFB = null, lMon = null, lWin = null;
        void SyncPlace()
        {
            bool wr = sc.Place is "wrist_left" or "wrist_right";
            foreach (var l in new[] { lWW, lTE, lLift, lSide, lRot2, lFace })
                Form.Show(l, wr);
            foreach (var l in new[] { lFW, lFD, lFB })
                Form.Show(l, !wr);
        }
        f.AddRow("", Check("Show it as soon as the game starts", () => sc.StartOn, v => sc.StartOn = v),
            "Otherwise add a Screen button to a wheel or wrist panel (the default left wrist panel has a Monitor button).");
        f.AddRow("Show", Pick(new Choice[] { new("monitor", "A whole monitor"), new("window", "One window (a browser, Discord, a video...)") }, () => sc.Source ?? "monitor",
            v => { sc.Source = v; Form.Show(lMon, v != "window"); Form.Show(lWin, v == "window"); }),
            "Whatever you pick in the headset (Choose... on the screen's bar) is saved here and becomes the default. A window is shown even when other windows cover it, but not while it's minimised.");
        lMon = f.AddRow("Monitor", Pick(Monitors.Choices(false), () => sc.Monitor.ToString(), v => sc.Monitor = int.TryParse(v, out var m) ? m : 0),
            "Monitors are numbered from left to right.");
        // the open programs are listed when the box gets focus (listing them takes about half a second)
        var win = new AutoCompleteBox { Width = 300, Text = sc.Window ?? "", FilterMode = AutoCompleteFilterMode.Contains, MinimumPrefixLength = 0 };
        bool listed = false;
        win.GotFocus += (s, e) => { if (!listed) { listed = true; win.ItemsSource = OpenWindows.Names(); } };
        win.TextChanged += (s, e) =>
        {
            string v = string.IsNullOrWhiteSpace(win.Text) ? null : win.Text.Trim();
            if (v == sc.Window) return;
            sc.Window = v;
            sc.WindowTitle = null;
            Shell.Store.MarkDirty();
        };
        lWin = f.AddRow("Window", win, "Part of its title or the program name, e.g. chrome, firefox, Discord, vlc. Pick one that's open now or type it. Empty = the first open window.");
        Form.Show(lMon, sc.Source != "window");
        Form.Show(lWin, sc.Source == "window");
        f.AddRow("Frame rate cap (fps)", Int(0, 240, 5, () => sc.Fps, v => sc.Fps = v),
            "How often the picture updates at most. 0 = as fast as the headset. Lower uses less CPU and GPU; 15-30 is plenty for reading.");
        var steps = new TextBox { Width = 220, Text = string.Join(", ", sc.FpsSteps ?? new()) };
        steps.TextChanged += (s, e) =>
        {
            var vals = (steps.Text ?? "").Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).Select(x => int.TryParse(x, out var n) ? n : -1).ToList();
            bool ok = vals.Count > 0 && vals.All(n => n >= 0 && n <= 240);
            steps.Background = ok ? Brush.Parse("#2A2F37") : Brush.Parse("#5A2A2A");
            if (ok && !vals.SequenceEqual(sc.FpsSteps ?? new()))
            { sc.FpsSteps = vals; Shell.Store.MarkDirty(); }
        };
        f.AddRow("Frame rates to switch between", steps, "For the 'next frame rate' button, e.g. 10, 15, 30, 60, 0  (0 = as fast as the headset).");
        f.AddRow("Where it shows", Pick(Choices.ScreenPlaces.Skip(1).ToArray(), () => sc.Place, v => { sc.Place = v; SyncPlace(); }),
            "A Screen button can pick its own place; this is used when it doesn't. Easiest to place by hand: Adjust positions in the headset shows the screen; grab it with a grip, stick to resize.");
        lWW = f.AddRow("Width on the wrist (m)", Num(0.04m, 0.6m, 0.01m, 2, () => sc.WristWidth, v => sc.WristWidth = v), "0.16 m is about a small tablet.");
        lFace = f.AddRow("Side of the arm", Pick(new Choice[] { new("back", "Back of the wrist (like a watch)"), new("inner", "Inner side (palm side)") }, () => sc.Face, v => sc.Face = v),
            "Wrist panels with buttons use the inner side by default, so the back keeps them apart.");
        lTE = f.AddRow("Toward the elbow (m)", Num(-0.1m, 0.4m, 0.01m, 2, () => sc.TowardElbow, v => sc.TowardElbow = v));
        lLift = f.AddRow("Off the arm (m)", Num(-0.1m, 0.2m, 0.005m, 3, () => sc.Lift, v => sc.Lift = v));
        lSide = f.AddRow("Sideways (m)", Num(-0.3m, 0.3m, 0.005m, 3, () => sc.Side, v => sc.Side = v));
        lRot2 = f.AddRow("Turn (deg)", Num(-180, 180, 5, 0, () => sc.RotateDeg, v => sc.RotateDeg = v), "0 = the picture runs along the forearm, read with the arm held across your body.");
        lFW = f.AddRow("Width in front (m)", Num(0.1m, 4, 0.05m, 2, () => sc.Width, v => sc.Width = v));
        lFD = f.AddRow("Distance in front (m)", Num(0.2m, 5, 0.05m, 2, () => sc.Distance, v => sc.Distance = v));
        lFB = f.AddRow("Below eye level (m)", Num(-1, 1, 0.01m, 2, () => sc.Below, v => sc.Below = v));
        f.AddRow("Opacity", Num(0.1m, 1, 0.05m, 2, () => sc.Opacity, v => sc.Opacity = v));
        f.AddRow("", Check("Show the mouse pointer", () => sc.Cursor, v => sc.Cursor = v));
        f.AddRow("", Check("Use the desktop by pointing at it", () => sc.Interact, v => sc.Interact = v),
            "Point at the screen: the mouse follows. Trigger = left click (hold to drag), A / X = right click, stick up/down = scroll. While pointing, those buttons don't reach the game.");
        f.AddRow("Steady the screen", Num(0, 1, 0.05m, 2, () => sc.Smoothing, v => sc.Smoothing = v),
            "For shaky hands: keeps a screen on your wrist (or following your view) still while you look at it, but still follows when you really move. 0 = off, 0.5 = a good start, 1 = strongest.");
        f.AddRow("Button bar size", Num(0.3m, 2, 0.05m, 2, () => sc.BarScale, v => sc.BarScale = v),
            "The bar under the screen (Choose, Frame rate, Move, Keyboard, Adjust, Hide). 1 = as wide as the screen, 0.7 = the default.");
        f.AddRow("", Check("Give the game its window back after clicking", () => sc.RefocusGame, v => sc.RefocusGame = v),
            "Clicking on the desktop takes the focus from the game; this hands it back when you point away.");
        f.AddRow("Placed by hand", ResetPose(() => sc.Poses is { Count: > 0 }, () => sc.Poses = null),
            "Adjust positions shows the screen; grab it and put it anywhere, at any angle. Each place (left wrist, right wrist, in front, following view) keeps its own spot.");
        f.AddRow("Sharpness limit (pixels wide)", Int(0, 7680, 160, () => sc.MaxWidthPx, v => sc.MaxWidthPx = v),
            "0 = automatic (1920). A bigger picture is halved while it is more than 1.5 times this.");
        SyncPlace();
        return f;
    }
}
