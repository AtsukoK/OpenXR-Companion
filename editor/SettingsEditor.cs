namespace OpenXRCompanionEditor;

public class SettingsEditor : EditorBase
{
    static readonly Choice[] BlockOpen =
    {
        new("hand", "Everything on the hand(s) using the wheel"), new("minimal", "Only the stick and the confirm / back buttons"),
        new("all", "Both controllers completely"), new("none", "Nothing, the game still sees every button"),
    };
    internal static readonly Choice[] Anchors =
    {
        new("hand_locked", "Above my hand, stays where it opened"), new("hand", "Above my hand, follows my hand"),
        new("head_locked", "In front of me, stays where it opened"), new("head", "In front of me, follows my view"),
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

    readonly ProfileModel p;

    public readonly string Page;
    // every word on the page (labels, tick boxes, help), for the search box
    public string SearchText { get; private set; } = "";

    readonly int wristHand; // -1 = a Settings page, 0 / 1 = the left / right wrist panel page

    // Only the shown page is built.
    bool Want(string key) => key == "wrist" ? wristHand >= 0 : wristHand < 0 && SettingsPage.ByKey(Page).Key == key;

    public SettingsEditor(ProfileModel model, int wristHand = -1, string page = "general", string highlight = null)
    {
        Page = page ?? "general";
        p = model;
        p.Thresholds ??= new();
        p.Style ??= new();
        p.Selection ??= new();
        p.Keyboard ??= new();
        p.Pointer ??= new();
        p.WristLeft ??= new();
        p.WristRight ??= new();
        p.Style.Colors ??= new();
        this.wristHand = wristHand;

        p.Screen ??= new();
        p.Offsets ??= new();
        p.Offsets.Left ??= new();
        p.Offsets.Right ??= new();
        p.Offsets.Left.Grip ??= new();
        p.Offsets.Left.Aim ??= new();
        p.Offsets.Right.Grip ??= new();
        p.Offsets.Right.Aim ??= new();

        var general = BuildGeneral();
        var look = BuildWheelLook();
        var pick = BuildPicking();
        var aim = BuildPointing();
        var scr = BuildScreen();
        var offs = BuildOffsets();
        var mir = BuildHeadsetMirror();
        var exp = BuildExperimental();
        var keys = BuildKeyboard();
        var wrist = BuildWrist();
        var adv = BuildAdvanced();
        const string savedTip = "Saved with this profile: changes take effect in the game when you press Save (Ctrl+S).";
        const string liveTip = "Applies straight away, in every game and profile. Nothing to save.";
        Card C(string title, string glyph, bool live, params Control[] body) =>
            new(title, glyph, live ? "Applies straight away" : "Saved with profile", live ? Theme.Live : Theme.Saved, live ? liveTip : savedTip, body);
        BackColor = Theme.Page;
        var pageControls = new List<Control>();
        string pageTitle, pageHelp;
        if (wristHand >= 0)
        {
            pageTitle = wristHand == 1 ? "Right wrist panel" : "Left wrist panel";
            pageHelp = "Buttons on your forearm. They appear when you turn the forearm toward your eyes and look at it. Its pages are listed under this panel in the list on the left; select a page to add buttons.";
            pageControls.Add(C("Panel", "\uE8A9", false, wrist));
        }
        else
        {
            var sp = SettingsPage.ByKey(Page);
            pageTitle = sp.Title;
            pageHelp = null;
            switch (sp.Key)
            {
                case "wheel":
                    pageHelp = "Where the radial wheel appears, how it looks, and how you pick an item on it.";
                    pageControls.Add(C("Position and look", "\uE790", false, look));
                    pageControls.Add(C("Picking items", "\uE7C9", false, pick));
                    break;
                case "pointing":
                    pageHelp = "The laser you point at wheels, wrist panels and the mirrored screen with.";
                    pageControls.Add(C("Laser and fingertip", "\uE7C9", false, aim));
                    break;
                case "keyboard":
                    pageControls.Add(C("VR keyboard", "\uE765", false, keys));
                    break;
                case "screen":
                    pageHelp = "A desktop monitor or one window, shown in VR on your wrist or in front of you.";
                    pageControls.Add(C("Screen mirror", "\uE7F4", false, scr));
                    break;
                case "offsets":
                    pageHelp = "Turns and moves the controllers the GAME sees, for when its hands or guns point too low or too high.";
                    pageControls.Add(C("Controller offsets", "\uE7FC", false, offs));
                    break;
                case "mirror":
                    pageHelp = "A desktop window showing what the headset shows, including the wheel, panels and lasers. Good for OBS.";
                    pageControls.Add(C("Headset mirror", "\uE890", true, mir));
                    break;
                case "experimental":
                    pageHelp = "Features that can cause motion sickness. All off by default.";
                    pageControls.Add(C("Head smoothing", "\uE7BA", true, exp));
                    break;
                case "advanced":
                    adv.Visible = true;
                    pageControls.Add(C("Advanced", "\uE90F", false, adv));
                    break;
                default:
                    pageHelp = "How the overlay behaves in this game.";
                    pageControls.Add(C("General", "\uE713", false, general));
                    break;
            }
        }
        var all = new List<Control> { Title(pageTitle) };
        if (pageHelp != null)
            all.Add(FormPanel.Header(pageHelp));
        foreach (var c in pageControls)
        { all.Add(c); all.Add(new Panel { Height = 14, Dock = DockStyle.Top, BackColor = Theme.Page }); }
        Stack(all.ToArray());

        // search text and highlighted matches
        var sb = new System.Text.StringBuilder(pageTitle + " " + pageHelp);
        var hits = new List<Control>();
        string q = string.IsNullOrWhiteSpace(highlight) ? null : highlight.Trim();
        void Walk(Control c)
        {
            if (c is FormPanel fp)
                foreach (var (l, ctl, hint) in fp.Rows)
                {
                    string words = l.Text + " " + (ctl is CheckBox cb ? cb.Text : "") + " " + hint;
                    sb.Append(' ').Append(words);
                    if (q != null && words.Contains(q, StringComparison.OrdinalIgnoreCase))
                    {
                        var mark = string.IsNullOrEmpty(l.Text) && ctl is CheckBox ? ctl : (Control)l;
                        mark.BackColor = Theme.Highlight;
                        hits.Add(mark);
                    }
                }
            foreach (Control k in c.Controls)
                Walk(k);
        }
        foreach (var c in pageControls)
            Walk(c);
        SearchText = sb.ToString();
        if (hits.Count > 0)
            HandleCreated += (o, e) => BeginInvoke(new Action(() => ScrollControlIntoView(hits[0])));
    }

    // ---------------- general
    FormPanel BuildGeneral()
    {
        var general = new FormPanel();
        if (Want("general"))
        { // only the shown page is built
            general.AddRow("", Check("Overlay is on for this game", p.Enabled, v => p.Enabled = v));
            general.AddRow("While a wheel is open, hide from the game", Pick(BlockOpen, p.BlockWhileOpen, v => p.BlockWhileOpen = v),
                "So pressing buttons to use the wheel doesn't also do things in the game.");
            general.AddRow("", Check("Vibrate when pointing at and picking items", p.Haptics, v => p.Haptics = v));
            general.AddRow("Vibration strength", Num(0, 1, 0.05m, 2, p.HapticStrength, v => p.HapticStrength = v));
            {   // app-wide, not part of the profile, so it doesn't mark the profile as changed
                var upd = new CheckBox { Text = "Check GitHub for a newer version when the editor starts", AutoSize = true, Checked = MainForm.UpdateCheckOn };
                upd.CheckedChanged += (o, e) => MainForm.UpdateCheckOn = upd.Checked;
                general.AddRow("Updates", upd, "Only reads the public release list on GitHub; nothing about you is sent, and nothing is downloaded or installed by itself.");
            }
            general.AddRow("", Check("Show messages in the headset (layer on/off, profile reloaded, errors)", p.Notifications, v => p.Notifications = v));

        }
        return general;
    }

    // ---------------- wheel position & look
    FormPanel BuildWheelLook()
    {
        var s = p.Style;
        var col = p.Style.Colors;
        var look = new FormPanel();
        if (Want("wheel"))
        { // only the shown page is built
            Label lHandUp = null, lHandToward = null, lHeadDist = null, lHeadDown = null;
            void SyncAnchor()
            {
                bool hand = s.Anchor is "hand" or "hand_locked";
                FormPanel.Show(lHandUp, hand);
                FormPanel.Show(lHandToward, hand);
                FormPanel.Show(lHeadDist, !hand);
                FormPanel.Show(lHeadDown, !hand);
            }
            look.AddRow("Where the wheel appears", Pick(Anchors, s.Anchor, v => { s.Anchor = v; SyncAnchor(); }));
            lHandUp = look.AddRow("Height above hand (m)", Num(0, 0.5m, 0.01m, 2, s.HandUp, v => s.HandUp = v));
            lHandToward = look.AddRow("Closer to my face by (m)", Num(-0.3m, 0.3m, 0.01m, 2, s.HandToward, v => s.HandToward = v));
            lHeadDist = look.AddRow("Distance in front (m)", Num(0.2m, 2, 0.05m, 2, s.HeadDistance, v => s.HeadDistance = v));
            lHeadDown = look.AddRow("Below eye level (m)", Num(-0.5m, 0.8m, 0.01m, 2, s.HeadDown, v => s.HeadDown = v));
            look.AddRow("Placed by hand", ResetPose(() => s.Poses is { Count: > 0 }, () => s.Poses = null),
                "Adjust positions in the headset: grab the wheel with a grip and put it anywhere, at any angle. That spot replaces the distance and height numbers here.");
            look.AddRow("Wheel size (radius, m)", Num(0.05m, 0.5m, 0.01m, 2, s.Radius, v => s.Radius = v), "0.12 m makes a wheel about 24 cm across.");
            look.AddRow("Text size", Num(0.5m, 2.5m, 0.1m, 1, s.FontScale, v => s.FontScale = v));
            look.AddRow("Font", TextField(s.Font, v => s.Font = v));
            var colors = new FlowLayoutPanel { AutoSize = true, WrapContents = true, MaximumSize = new Size(480, 0), Margin = Padding.Empty };
            void AddColor(string label, Func<string> get, Action<string> set) => colors.Controls.Add(ColorPick(label, get, set));
            AddColor("Background", () => col.Background, v => col.Background = v);
            AddColor("Segments", () => col.Segment, v => col.Segment = v);
            AddColor("Pointed at", () => col.Hover, v => col.Hover = v);
            AddColor("Text", () => col.Text, v => col.Text = v);
            AddColor("Accent", () => col.Accent, v => col.Accent = v);
            AddColor("Outline", () => col.Border, v => col.Border = v);
            AddColor("Toggled on", () => col.ToggledOn, v => col.ToggledOn = v);
            look.AddRow("Colours", colors, "Click a colour to change it. Transparency is kept.");
            SyncAnchor();

        }
        return look;
    }

    // ---------------- selecting
    FormPanel BuildPicking()
    {
        var sel = p.Selection;
        var pick = new FormPanel();
        if (Want("wheel"))
        { // only the shown page is built
            Label lDead = null;
            pick.AddRow("Pick items by", Pick(Methods, sel.Method, v => { sel.Method = v; FormPanel.Show(lDead, v != "point"); }));
            pick.AddRow("Selecting hand", Pick(SelHands, sel.Hand, v => sel.Hand = v));
            lDead = pick.AddRow("Stick deadzone", Num(0.1m, 0.95m, 0.05m, 2, sel.Deadzone, v => sel.Deadzone = v), "How far the stick must be pushed before an item is chosen.");
            FormPanel.Show(lDead, sel.Method != "point");
            pick.AddRow("Confirm button", Pick(Comps, sel.ConfirmButton, v => sel.ConfirmButton = v), "Always picks the item you point at, whatever the wheel's pick mode.");
            pick.AddRow("Back / close button", Pick(Comps, sel.CancelButton, v => sel.CancelButton = v));
            pick.AddRow("", Check("Close the wheel after picking an item", sel.CloseAfterSelect, v => sel.CloseAfterSelect = v));
            pick.AddRow("Open submenus by resting (ms)", Int(0, 3000, 50, sel.SubmenuDwellMs, v => sel.SubmenuDwellMs = v), "For 'let go of the button' wheels. 0 = off.");
            pick.AddRow("Close by itself after (ms)", Int(0, 60000, 500, sel.AutoCloseMs, v => sel.AutoCloseMs = v), "0 = never.");

        }
        return pick;
    }

    // ---------------- pointing
    FormPanel BuildPointing()
    {
        var ptr = p.Pointer;
        var aim = new FormPanel();
        if (Want("pointing"))
        { // only the shown page is built
            Label lPitch = null, lDown = null, lFwd = null;
            void SyncCorr()
            { bool on = ptr.Correction != "off"; FormPanel.Show(lPitch, on); FormPanel.Show(lDown, on); FormPanel.Show(lFwd, on); }
            aim.AddRow("Correct the laser direction", Pick(new Choice[] { new("auto", "Only on Quest (Touch) controllers"), new("always", "On every controller"), new("off", "Off, use the game's controller ray") }, ptr.Correction, v => { ptr.Correction = v; SyncCorr(); }),
                "Quest controllers aim from above your index finger and about 15 degrees upward. This makes the laser go where a pointing finger goes.");
            lPitch = aim.AddRow("Turn the laser down by (deg)", Num(-45, 45, 1, 0, ptr.PitchDownDeg, v => ptr.PitchDownDeg = v), "15 matches most people on Quest. Saved changes apply in the headset within a second.");
            lDown = aim.AddRow("Move the laser start down (m)", Num(-0.1m, 0.1m, 0.005m, 3, ptr.DownM, v => ptr.DownM = v), "Toward your index finger. Also moves the poking fingertip.");
            lFwd = aim.AddRow("Move the laser start forward (m)", Num(-0.1m, 0.1m, 0.005m, 3, ptr.ForwardM, v => ptr.ForwardM = v));
            aim.AddRow("", Check("Show a fingertip near wrist panels (where your poke lands and the laser starts)", ptr.Finger, v => ptr.Finger = v));
            aim.AddRow("Fingertip forward (m)", Num(-0.1m, 0.15m, 0.005m, 3, ptr.FingertipForwardM, v => ptr.FingertipForwardM = v),
                "Where your poke lands on wrist panels, measured from the laser start. Raise it if buttons press before your finger reaches them, lower it if you have to push through. Saved changes apply in the headset within a second.");
            aim.AddRow("Fingertip up (m)", Num(-0.1m, 0.1m, 0.005m, 3, ptr.FingertipUpM, v => ptr.FingertipUpM = v));
            aim.AddRow("Fingertip toward the middle (m)", Num(-0.1m, 0.1m, 0.005m, 3, ptr.FingertipInwardM, v => ptr.FingertipInwardM = v),
                "Sideways, mirrored for each hand: positive moves both fingertips toward your body's middle.");
            aim.AddRow("Steady the laser", Num(0, 1, 0.05m, 2, ptr.Smoothing, v => ptr.Smoothing = v),
                "For shaky hands: smooths the laser (and the mouse on the mirrored screen) while you hold still or move slowly; quick movements still follow straight away. 0 = off, 0.5 = a good start, 1 = strongest.");
            aim.AddRow("", Check("Keep a short laser showing while a wrist panel is up", ptr.GuideLaser, v => ptr.GuideLaser = v),
                "So you can find your pointer before it reaches the panel. It hands over to the touch ring when your fingertip gets close.");
            SyncCorr();

        }
        return aim;
    }

    // ---------------- screen mirror
    FormPanel BuildScreen()
    {
        var sc = p.Screen;
        var scr = new FormPanel();
        if (Want("screen"))
        { // only the shown page is built
            Label lWW = null, lTE = null, lLift = null, lSide = null, lRot2 = null, lFace = null, lFW = null, lFD = null, lFB = null;
            void SyncPlace()
            {
                bool wr = sc.Place is "wrist_left" or "wrist_right";
                foreach (var l in new[] { lWW, lTE, lLift, lSide, lRot2, lFace })
                    FormPanel.Show(l, wr);
                foreach (var l in new[] { lFW, lFD, lFB })
                    FormPanel.Show(l, !wr);
            }
            scr.AddRow("", Check("Show it as soon as the game starts", sc.StartOn, v => sc.StartOn = v),
                "Otherwise add a Screen button to a wheel or wrist panel (the default left wrist panel has them under More > Screen).");
            Label lMon = null, lWin = null;
            scr.AddRow("Show", Pick(new Choice[] { new("monitor", "A whole monitor"), new("window", "One window (a browser, Discord, a video...)") }, sc.Source ?? "monitor",
                v => { sc.Source = v; FormPanel.Show(lMon, v != "window"); FormPanel.Show(lWin, v == "window"); }),
                "Whatever you pick in the headset (Choose... on the screen's bar) is saved here and becomes the default. A window is shown even when other windows cover it, but not while it's minimised.");
            lMon = scr.AddRow("Monitor", Pick(Monitors.Choices(false), sc.Monitor.ToString(), v => sc.Monitor = int.TryParse(v, out var m) ? m : 0),
                "Monitors are numbered from left to right.");
            var win = new ComboBox { Width = 260, Text = sc.Window ?? "" };
            OpenWindows.FillOnOpen(win);
            win.TextChanged += (o, e) => { sc.Window = string.IsNullOrWhiteSpace(win.Text) ? null : win.Text.Trim(); sc.WindowTitle = null; Fire(); };
            lWin = scr.AddRow("Window", win, "Part of its title or the program name, e.g. chrome, firefox, Discord, vlc. Pick one that's open now or type it. Empty = the first open window.");
            FormPanel.Show(lMon, sc.Source != "window");
            FormPanel.Show(lWin, sc.Source == "window");
            scr.AddRow("Frame rate cap (fps)", Int(0, 240, 5, sc.Fps, v => sc.Fps = v),
                "How often the picture updates at most. 0 = as fast as the headset. Lower uses less CPU and GPU; 15-30 is plenty for reading.");
            var steps = new TextBox { Width = 200, Text = string.Join(", ", sc.FpsSteps ?? new()) };
            steps.TextChanged += (o, e) =>
            {
                var vals = steps.Text.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).Select(x => int.TryParse(x, out var n) ? n : -1).ToList();
                bool ok = vals.Count > 0 && vals.All(n => n >= 0 && n <= 240);
                steps.BackColor = ok ? SystemColors.Window : Color.MistyRose;
                if (ok)
                { sc.FpsSteps = vals; Fire(); }
            };
            scr.AddRow("Frame rates to switch between", steps, "For the 'next frame rate' button, e.g. 10, 15, 30, 60, 0  (0 = as fast as the headset).");
            scr.AddRow("Where it shows", Pick(Choices.ScreenPlaces.Skip(1).ToArray(), sc.Place, v => { sc.Place = v; SyncPlace(); }),
                "A Screen button can pick its own place; this is used when it doesn't. Easiest to place by hand: Adjust positions in the headset shows the screen; grab it with a grip, stick to resize.");
            lWW = scr.AddRow("Width on the wrist (m)", Num(0.04m, 0.6m, 0.01m, 2, sc.WristWidth, v => sc.WristWidth = v), "0.16 m is about a small tablet.");
            lFace = scr.AddRow("Side of the arm", Pick(new Choice[] { new("back", "Back of the wrist (like a watch)"), new("inner", "Inner side (palm side)") }, sc.Face, v => sc.Face = v),
                "Wrist panels with buttons use the inner side by default, so the back keeps them apart.");
            lTE = scr.AddRow("Toward the elbow (m)", Num(-0.1m, 0.4m, 0.01m, 2, sc.TowardElbow, v => sc.TowardElbow = v));
            lLift = scr.AddRow("Off the arm (m)", Num(-0.1m, 0.2m, 0.005m, 3, sc.Lift, v => sc.Lift = v));
            lSide = scr.AddRow("Sideways (m)", Num(-0.3m, 0.3m, 0.005m, 3, sc.Side, v => sc.Side = v));
            lRot2 = scr.AddRow("Turn (deg)", Num(-180, 180, 5, 0, sc.RotateDeg, v => sc.RotateDeg = v), "0 = the picture runs along the forearm, read with the arm held across your body.");
            lFW = scr.AddRow("Width in front (m)", Num(0.1m, 4, 0.05m, 2, sc.Width, v => sc.Width = v));
            lFD = scr.AddRow("Distance in front (m)", Num(0.2m, 5, 0.05m, 2, sc.Distance, v => sc.Distance = v));
            lFB = scr.AddRow("Below eye level (m)", Num(-1, 1, 0.01m, 2, sc.Below, v => sc.Below = v));
            scr.AddRow("Opacity", Num(0.1m, 1, 0.05m, 2, sc.Opacity, v => sc.Opacity = v));
            scr.AddRow("", Check("Show the mouse pointer", sc.Cursor, v => sc.Cursor = v));
            scr.AddRow("", Check("Use the desktop by pointing at it", sc.Interact, v => sc.Interact = v),
                "Point at the screen: the mouse follows. Trigger = left click (hold to drag), A / X = right click, stick up/down = scroll. While pointing, those buttons don't reach the game.");
            scr.AddRow("Steady the screen", Num(0, 1, 0.05m, 2, sc.Smoothing, v => sc.Smoothing = v),
                "For shaky hands: keeps a screen on your wrist (or following your view) still while you look at it, but still follows when you really move. 0 = off, 0.5 = a good start, 1 = strongest.");
            scr.AddRow("Button bar size", Num(0.3m, 2, 0.05m, 2, sc.BarScale, v => sc.BarScale = v),
                "The bar under the screen (Choose, Frame rate, Move, Keyboard, Adjust, Hide). 1 = as wide as the screen, 0.7 = the default.");
            scr.AddRow("", Check("Give the game its window back after clicking", sc.RefocusGame, v => sc.RefocusGame = v),
                "Clicking on the desktop takes the focus from the game; this hands it back when you point away.");
            scr.AddRow("Placed by hand", ResetPose(() => sc.Poses is { Count: > 0 }, () => sc.Poses = null),
                "Adjust positions shows the screen; grab it and put it anywhere, at any angle. Each place (left wrist, right wrist, in front, following view) keeps its own spot.");
            scr.AddRow("Sharpness limit (pixels wide)", Int(0, 7680, 160, sc.MaxWidthPx, v => sc.MaxWidthPx = v),
                "0 = automatic (1920). A bigger picture is halved while it is more than 1.5 times this.");
            SyncPlace();

        }
        return scr;
    }

    // ---------------- controller offsets (for the game)
    FormPanel BuildOffsets()
    {
        var co = p.Offsets;
        var offs = new FormPanel();
        if (Want("offsets"))
        { // only the shown page is built
            offs.AddRow("", Check("Use controller offsets in this game", co.Enabled, v => co.Enabled = v),
                "Turns and moves the controllers the GAME sees: its hands, guns and its own laser. Untick to give the game the plain controllers (your numbers are kept). Each game can have its own profile with its own offsets.");
            var offRows = new List<(Func<OffsetModel, float> get, Action<OffsetModel, float> set, TrackBar bar, NumericUpDown num)>();
            bool offLoading = false;
            int selHand = co.LinkHands ? 2 : 1, selKind = co.AimSame ? 2 : 0;
            OffsetModel Target()
            {
                var h = co.LinkHands || selHand == 1 ? co.Right : co.Left;
                return co.AimSame || selKind == 0 ? h.Grip : h.Aim;
            }
            void RefreshOffsets()
            {
                offLoading = true;
                var t = Target();
                foreach (var r in offRows)
                {
                    decimal v = Math.Clamp((decimal)r.get(t), r.num.Minimum, r.num.Maximum);
                    r.num.Value = v;
                    r.bar.Value = Math.Clamp((int)Math.Round(v * 10), r.bar.Minimum, r.bar.Maximum);
                }
                offLoading = false;
            }
            Mirror Copy(OffsetModel a) => new(a);
            offs.AddRow("Hand", Pick(new Choice[] { new("2", "Both hands, mirrored (the left copies the right)"), new("1", "Right hand on its own"), new("0", "Left hand on its own") }, selHand.ToString(), v =>
            {
                int n = int.Parse(v);
                if (co.LinkHands && n != 2)
                { Copy(co.Right.Grip).To(co.Left.Grip); Copy(co.Right.Aim).To(co.Left.Aim); } // nothing jumps when unlinking
                co.LinkHands = n == 2;
                selHand = n;
                RefreshOffsets();
            }), "Mirrored: yaw, roll and left/right are flipped for the left hand, so both feel the same.");
            offs.AddRow("Pose", Pick(new Choice[] { new("2", "Grip and aim together"), new("0", "Grip only (the hand, held guns and swords)"), new("1", "Aim only (the game's own laser / pointer ray)") }, selKind.ToString(), v =>
            {
                int n = int.Parse(v);
                if (co.AimSame && n != 2)
                { co.Right.Aim = Clone(co.Right.Grip); co.Left.Aim = Clone(co.Left.Grip); }
                co.AimSame = n == 2;
                selKind = n;
                RefreshOffsets();
            }), "Most games put hands and weapons on the grip pose. Together is the safe choice: whatever the game uses gets the same fix.");
            void OffRow(string label, string unit, int min, int max, Func<OffsetModel, float> get, Action<OffsetModel, float> set, string hint)
            {
                var f = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = Padding.Empty };
                var bar = new TrackBar { Minimum = min * 10, Maximum = max * 10, TickFrequency = (max - min) * 10 / 8, Width = 210, SmallChange = 5, LargeChange = 50, Margin = new Padding(4, 6, 4, 0) };
                var num = new NumericUpDown { Minimum = min, Maximum = max, Increment = 0.5m, DecimalPlaces = 1, Width = 70, Margin = new Padding(6, 9, 4, 0) };
                var minus = new Button { Text = "\u2212", Width = 32, Height = 27, Margin = new Padding(0, 8, 0, 0) };
                var plus = new Button { Text = "+", Width = 32, Height = 27, Margin = new Padding(0, 8, 0, 0) };
                var zero = new Button { Text = "0", Width = 32, Height = 27, Margin = new Padding(4, 8, 0, 0) };
                FormPanel.Tips.SetToolTip(zero, "Set back to 0");
                var u = new Label { Text = unit, AutoSize = false, Width = 34, Height = 27, TextAlign = ContentAlignment.MiddleLeft, Margin = new Padding(0, 8, 0, 0) };
                void Apply(decimal v)
                {
                    if (offLoading)
                        return;
                    set(Target(), (float)v);
                    RefreshOffsets();
                    Fire();
                }
                bar.ValueChanged += (o, e) => { if (!offLoading) Apply(bar.Value / 10m); };
                num.ValueChanged += (o, e) => { if (!offLoading) Apply(num.Value); };
                minus.Click += (o, e) => Apply(Math.Max(num.Minimum, num.Value - num.Increment));
                plus.Click += (o, e) => Apply(Math.Min(num.Maximum, num.Value + num.Increment));
                zero.Click += (o, e) => Apply(0);
                f.Controls.Add(minus);
                f.Controls.Add(bar);
                f.Controls.Add(plus);
                f.Controls.Add(num);
                f.Controls.Add(u);
                f.Controls.Add(zero);
                offRows.Add((get, set, bar, num));
                offs.AddRow(label, f, hint);
            }
            OffRow("Pitch (tip up +)", "deg", -60, 60, o => o.Pitch, (o, v) => o.Pitch = v,
                "If you have to point the controller down to aim down a gun's sights, raise this (Quest controllers often need +10 to +20).");
            OffRow("Yaw (turn left +)", "deg", -60, 60, o => o.Yaw, (o, v) => o.Yaw = v, null);
            OffRow("Roll (roll left +)", "deg", -90, 90, o => o.Roll, (o, v) => o.Roll = v, null);
            OffRow("Up / down", "cm", -20, 20, o => o.Y, (o, v) => o.Y = v, null);
            OffRow("Right / left", "cm", -20, 20, o => o.X, (o, v) => o.X = v, null);
            OffRow("Forward / back", "cm", -20, 20, o => -o.Z, (o, v) => o.Z = -v, "Positions move the hand in the controller's own directions.");
            {
                var f = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = Padding.Empty };
                var r1 = new Button { Text = "Reset this hand / pose", AutoSize = true };
                var r2 = new Button { Text = "Reset everything", AutoSize = true };
                r1.Click += (o, e) => { Target().Clear(); RefreshOffsets(); Fire(); };
                r2.Click += (o, e) => { foreach (var m in new[] { co.Left.Grip, co.Left.Aim, co.Right.Grip, co.Right.Aim }) m.Clear(); RefreshOffsets(); Fire(); };
                f.Controls.Add(r1);
                f.Controls.Add(r2);
                offs.AddRow("Reset", f, "Back to 0, the game's own controller angles.");
            }
            offs.AddRow("Steady the game's hands", Num(0, 1, 0.05m, 2, co.Smoothing, v => co.Smoothing = v),
                "For shaky hands: smooths the hands and guns in the game while you hold still; quick moves still follow straight away. 0 = off, 0.5 = a good start.");
            offs.AddRow("In the headset", new Label
            {
                AutoSize = true,
                MaximumSize = new Size(460, 0),
                Text =
                "Add a button with the action \"Controller offsets\" to a wheel or wrist panel (the default left wrist panel has it under More). It opens a panel with + / - buttons (hold to repeat, or point at a row and push the stick), Grab to set (hold grip, turn the controller to where it feels right, let go) and resets. Changes save by themselves."
            });
            RefreshOffsets();

        }
        return offs;
    }

    // ---------------- headset mirror (app-wide, its own file, saved straight away)
    FormPanel BuildHeadsetMirror()
    {
        var mir = new FormPanel();
        if (Want("mirror"))
        { // only the shown page is built
            {
                var on = new CheckBox { Text = "Show the headset mirror window", AutoSize = true, Checked = HeadsetMirrorFile.On };
                on.CheckedChanged += (o, e) => HeadsetMirrorFile.On = on.Checked;
                mir.AddRow("", on, "Same as the button on the toolbar. A desktop window with what the headset shows, including the wheel, wrist panels, screen and lasers. For OBS, capture that window. Closing it turns it off. D3D11 games (Bannerlord VR, Fallout VR mods...).");
                var top = new CheckBox { Text = "Always on top of other windows", AutoSize = true, Checked = HeadsetMirrorFile.OnTop };
                top.CheckedChanged += (o, e) => HeadsetMirrorFile.OnTop = top.Checked;
                mir.AddRow("", top, "Keeps the mirror above the game on your monitor, e.g. to record or show it. The mirror never takes focus from the game, even when you click or move it, so the game keeps getting your input.");
                // Saved as two values: "eye" (left / right) and "both_eyes" (the other eye fills the nose side).
                var eye = new ChoiceBox(new Choice[] {
                    new("left", "Left eye"), new("right", "Right eye"),
                    new("both_right", "Both eyes (right eye, left fills in)"), new("both_left", "Both eyes (left eye, right fills in)") }, 300)
                { Code = (HeadsetMirrorFile.BothEyes ? "both_" : "") + HeadsetMirrorFile.Eye };
                eye.SelectedIndexChanged += (o, e) =>
                {
                    string code = eye.Code ?? "right";
                    HeadsetMirrorFile.BothEyes = code.StartsWith("both_");
                    HeadsetMirrorFile.Eye = code.EndsWith("left") ? "left" : "right";
                };
                bool BothEyesShown() => (eye.Code ?? "").StartsWith("both_");
                mir.AddRow("Eye", eye, "One eye: exactly that eye's picture. Both eyes: centred on where that eye looks, and the other eye fills in the nose side, where one eye's picture ends in a black strip, so even zoom 1 stays centred. Far things line up; close things (your hands, the gun) can look a little doubled on that side.");
                var preset = new ChoiceBox(new Choice[] {
                    new("custom", "Custom"),
                    new("gaze", "Centred where you look (default)"),
                    new("gaze_room", "Centred, smoothing makes its own room"),
                    new("both", "Wide: both eyes, centred where you look"),
                    new("original", "Original: middle of the picture"),
                    new("exact", "Exact headset view (no zoom, no smoothing)") }, 300);
                mir.AddRow("Preset", preset, "Sets zoom, head camera smoothing, centre, edge margin, smoothing's own room and one / both eyes together (the side you picked stays). Change any of them afterwards and it shows Custom.");
                var zoom = new NumericUpDown { Minimum = 1, Maximum = 3, Increment = 0.05m, DecimalPlaces = 2, Width = 90, Value = (decimal)Math.Round(Math.Clamp(HeadsetMirrorFile.Zoom, 1, 3), 2) };
                zoom.ValueChanged += (o, e) => HeadsetMirrorFile.Zoom = (double)zoom.Value;
                mir.AddRow("Zoom", zoom, "1 = the whole eye picture (very wide, blurry edges). 1.3 looks like a normal camera. The same with smoothing on or off. It always fills the window; resize the window for a different shape.");
                var fps = new NumericUpDown { Minimum = 0, Maximum = 240, Increment = 5, Width = 90, Value = Math.Clamp(HeadsetMirrorFile.Fps, 0, 240) };
                fps.ValueChanged += (o, e) => HeadsetMirrorFile.Fps = (int)fps.Value;
                // Saved as one value, "smoothing" (0 = off); the strength is kept while it is off.
                bool smoothingWasOn = HeadsetMirrorFile.Smoothing > 0.001;
                var smoothOn = new CheckBox { Text = "Head camera smoothing", AutoSize = true, Checked = smoothingWasOn };
                var st = new NumericUpDown { Minimum = 0.05m, Maximum = 1, Increment = 0.05m, DecimalPlaces = 2, Width = 90,
                    Value = (decimal)Math.Round(Math.Clamp(smoothingWasOn ? HeadsetMirrorFile.Smoothing : HeadsetMirrorFile.SmoothingStrength, 0.05, 1), 2), Enabled = smoothingWasOn };
                smoothOn.CheckedChanged += (o, e) =>
                {
                    st.Enabled = smoothOn.Checked;
                    HeadsetMirrorFile.Smoothing = smoothOn.Checked ? (double)st.Value : 0;
                };
                st.ValueChanged += (o, e) =>
                {
                    HeadsetMirrorFile.SmoothingStrength = (double)st.Value;
                    if (smoothOn.Checked) HeadsetMirrorFile.Smoothing = (double)st.Value;
                };
                decimal SmoothingShown() => smoothOn.Checked ? st.Value : 0;
                mir.AddRow("", smoothOn, "Smooths small head shakes out of the window, like a camera stabiliser; real head turns still follow. Off = the exact headset view. It can only drift into the picture the zoom leaves out: at zoom 1 it barely works, from about 1.3 it works fully (or turn on \"Smoothing makes its own room\").");
                mir.AddRow("Smoothing strength", st, "How much head shake it takes out: 0.05 = a little, 0.5 = the default, 1 = the most (real turns follow later).");
                var centre = new ChoiceBox(new Choice[] { new("gaze", "Where the eye looks"), new("middle", "Middle of the picture") }, 200) { Code = HeadsetMirrorFile.Center };
                centre.SelectedIndexChanged += (o, e) => HeadsetMirrorFile.Center = centre.Code;
                mir.AddRow("Centre the view on", centre, "Where the eye looks: what you aim at sits in the middle of the window. Middle of the picture: the older way, a little off to one side on Quest, but further from the black strip on the nose side.");
                var margin = new NumericUpDown { Minimum = 0, Maximum = 0.3m, Increment = 0.01m, DecimalPlaces = 2, Width = 90, Value = (decimal)Math.Round(Math.Clamp(HeadsetMirrorFile.SideMargin, 0, 0.3), 2) };
                margin.ValueChanged += (o, e) => HeadsetMirrorFile.SideMargin = (double)margin.Value;
                mir.AddRow("Edge margin", margin, "How far the view stays from the eye picture's sides (0.12 = 12% of its width), to keep the black strip on the nose side out. Lower it if you see no strip, raise it if you do.");
                var room = new CheckBox { Text = "Smoothing makes its own room", AutoSize = true, Checked = HeadsetMirrorFile.SteadyRoom };
                room.CheckedChanged += (o, e) => HeadsetMirrorFile.SteadyRoom = room.Checked;
                mir.AddRow("", room, "Off: zoom means the same with smoothing on or off, and smoothing only uses the room the zoom leaves. On: smoothing crops in around the centre by itself so it always works fully, so low zoom looks closer.");
                mir.AddRow("Frame rate cap (fps)", fps, "0 = every headset frame. 60 is smooth for recording and costs little. 30 halves the cost.");
                var quality = new ChoiceBox(new Choice[] { new("light", "Light (cheaper, default)"), new("full", "Full (copy with mipmaps)") }, 200) { Code = HeadsetMirrorFile.Quality };
                quality.SelectedIndexChanged += (o, e) => HeadsetMirrorFile.Quality = quality.Code;
                mir.AddRow("Picture", quality, "Light draws straight from the game's picture: much cheaper, looks the same at normal window sizes. Full copies the picture and makes mipmaps first: smoother only when the window is much smaller than the headset picture (light switches to it by itself then).");
                // zoom, smoothing (0 = off), centre, edge margin, own room, both eyes
                var presets = new Dictionary<string, (decimal zoom, decimal steady, string centre, decimal margin, bool room, bool both)>
                {
                    ["gaze"] = (1.4m, 0.5m, "gaze", 0.12m, false, false),
                    ["gaze_room"] = (1.3m, 0.5m, "gaze", 0.12m, true, false),
                    ["both"] = (1.0m, 0.5m, "gaze", 0.12m, false, true),
                    ["original"] = (1.3m, 0.5m, "middle", 0.04m, true, false),
                    ["exact"] = (1.0m, 0m, "middle", 0m, false, false),
                };
                bool applying = false;
                string Matching()
                {
                    foreach (var (key, v) in presets)
                        if (zoom.Value == v.zoom && SmoothingShown() == v.steady && centre.Code == v.centre && margin.Value == v.margin &&
                            room.Checked == v.room && BothEyesShown() == v.both)
                            return key;
                    return "custom";
                }
                void ShowMatching() { applying = true; preset.Code = Matching(); applying = false; }
                ShowMatching();
                preset.SelectedIndexChanged += (o, e) =>
                {
                    if (applying || !presets.TryGetValue(preset.Code ?? "", out var v)) return;
                    applying = true;
                    zoom.Value = v.zoom;
                    if (v.steady > 0) st.Value = v.steady;
                    smoothOn.Checked = v.steady > 0;
                    centre.Code = v.centre;
                    margin.Value = v.margin;
                    room.Checked = v.room;
                    string side = (eye.Code ?? "right").EndsWith("left") ? "left" : "right";
                    eye.Code = (v.both ? "both_" : "") + side;
                    applying = false;
                };
                EventHandler changed = (o, e) => { if (!applying) ShowMatching(); };
                zoom.ValueChanged += changed;
                st.ValueChanged += changed;
                smoothOn.CheckedChanged += changed;
                centre.SelectedIndexChanged += changed;
                margin.ValueChanged += changed;
                room.CheckedChanged += changed;
                eye.SelectedIndexChanged += changed;
                mir.AddRow("", new Label { AutoSize = true, ForeColor = Color.DimGray, MaximumSize = new Size(460, 0), Text = "These apply straight away, in every game and profile (no need to save)." });
            }

        }
        return mir;
    }

    // ---------------- experimental (app-wide, its own file, saved straight away)
    FormPanel BuildExperimental()
    {
        var exp = new FormPanel();
        if (Want("experimental"))
        { // only the shown page is built
            {
                var warn = new Label
                {
                    AutoSize = true,
                    ForeColor = Color.FromArgb(0xD9, 0x48, 0x2B),
                    Font = new Font("Segoe UI", 10f, FontStyle.Bold),
                    MaximumSize = new Size(560, 0),
                    Text = "⚠ ⚠ MOTION SICKNESS WARNING ⚠ ⚠\n" +
                           "Head smoothing makes the game's view lag slightly behind your real head. In VR that mismatch can cause nausea, dizziness or headaches, quickly and without warning.\n" +
                           "• Start with \"For recording\" and a low strength.\n" +
                           "• Take the headset off at the first sign of feeling unwell, and don't use it if you are prone to motion sickness.\n" +
                           "• It switches itself off whenever a game starts (unless you tick Keep it on below)."
                };
                exp.AddRow("", warn);
                var on = new CheckBox { Text = "⚠ Smooth the head (off by default)", AutoSize = true, Checked = HeadSmoothingFile.On };
                on.CheckedChanged += (o, e) =>
                {
                    if (on.Checked && !HeadSmoothingFile.On && MessageBox.Show(FindForm(),
                            "Head smoothing can cause motion sickness: nausea, dizziness or headaches, sometimes quickly.\n\n" +
                            "Start with \"For recording\" and a low strength, and take the headset off at the first sign of feeling unwell.\n\nTurn it on?",
                            "⚠ Experimental: head smoothing", MessageBoxButtons.YesNo, MessageBoxIcon.Warning, MessageBoxDefaultButton.Button2) != DialogResult.Yes)
                    { on.Checked = false; return; }
                    HeadSmoothingFile.On = on.Checked;
                };
                exp.AddRow("", on, "Steadies the direction the GAME looks from, like a camera stabiliser, so small head shakes don't show. Real turns still follow. Position isn't changed. For using the headset as a camera.");
                var mode = new ChoiceBox(new Choice[] {
                new("recording", "For recording (the headset view stays normal)"),
                new("headset", "⚠ In the headset too (highest sickness risk)") }, 340)
                { Code = HeadSmoothingFile.Mode };
                mode.SelectedIndexChanged += (o, e) => HeadSmoothingFile.Mode = mode.Code;
                exp.AddRow("Where", mode, "For recording: the game's own desktop view and recordings are steadied; the headset corrects back to your real head, so the world still feels solid (the picture's edge can show briefly during shakes). In the headset too: you see the steadied picture yourself; the world moves a little with your head. Much more likely to make you feel sick.");
                var st = new NumericUpDown { Minimum = 0, Maximum = 100, Increment = 0.5m, DecimalPlaces = 2, Width = 90, Value = (decimal)Math.Clamp(HeadSmoothingFile.Strength, 0, 100) };
                st.ValueChanged += (o, e) => HeadSmoothingFile.Strength = (double)st.Value;
                var keep = new CheckBox { Text = "Keep it on when a game starts", AutoSize = true, Checked = HeadSmoothingFile.KeepOn };
                keep.CheckedChanged += (o, e) => HeadSmoothingFile.KeepOn = keep.Checked;
                exp.AddRow("", keep, "Off (the default): head smoothing switches itself off every time a game starts, so you never put the headset on with it already running. Turn it on again here while the game runs.");
                exp.AddRow("Strength", st, "How heavy the camera is. It smooths every movement the same way, easing into and out of turns like a weighted camera: about 0.12 s of lag per step, so 0.5 = light, 2 = smooth, 5 = heavy (0.6 s), 10 = very heavy (1.2 s), up to 100 (12 s, for slow drifting shots). ⚠ In the headset, anything above about 1 is very likely to make you sick; use For recording.");
                exp.AddRow("", new Label { AutoSize = true, ForeColor = Color.DimGray, MaximumSize = new Size(460, 0), Text = "These apply straight away, in every game and profile (no need to save)." });
            }
        }
        return exp;
    }

    // ---------------- keyboard
    FormPanel BuildKeyboard()
    {
        var kb = p.Keyboard;
        var keys = new FormPanel();
        if (Want("keyboard"))
        { // only the shown page is built
            keys.AddRow("Placed by hand", ResetPose(() => kb.Pose != null, () => kb.Pose = null),
                "Adjust positions in the headset (Calibrate Positions): open the keyboard, grab it with a grip and put it anywhere; the stick changes its width. That spot replaces the distance and height numbers below.");
            keys.AddRow("Width (m)", Num(0.2m, 1.5m, 0.02m, 2, kb.Width, v => kb.Width = v));
            keys.AddRow("Distance in front (m)", Num(0.2m, 1.5m, 0.02m, 2, kb.Distance, v => kb.Distance = v));
            keys.AddRow("Below eye level (m)", Num(-0.3m, 0.8m, 0.02m, 2, kb.Below, v => kb.Below = v));
            keys.AddRow("", Check("Close the keyboard after Enter (handy for chat boxes)", kb.CloseOnEnter, v => kb.CloseOnEnter = v));
            keys.AddRow("Screen docking", Pick(new Choice[] { new("none", "Apart (each in its own place)"), new("screen_to_keyboard", "The screen sits above the keyboard"), new("keyboard_to_screen", "The keyboard sits under the screen") },
                p.Dock?.Mode ?? "none", v => { p.Dock ??= new DockModel(); p.Dock.Mode = v; }),
                "Easier typing into a mirrored window: the two move together. Also on the bar under the keyboard in the headset (Dock screen above keys / Dock keys under screen), where 'Adjust dock' lets you grab the docked one and put it where you like.");
            keys.AddRow("Dock placed by hand", ResetPose(() => p.Dock?.Pose != null, () => { if (p.Dock != null) { p.Dock.Pose = null; p.Dock.Width = 0; } }, "No, the default spot (screen just above the keys)", "Back to the default spot"));

        }
        return keys;
    }

    // ---------------- wrist panel
    FormPanel BuildWrist()
    {
        var wp = p.WristFor(Math.Max(0, wristHand));
        var wrist = new FormPanel();
        if (Want("wrist"))
        { // only the shown page is built
            string armName = wristHand == 1 ? "right" : "left", otherHand = wristHand == 1 ? "left" : "right";
            var pageChoices = wp.Pages.Keys.Select(k => new Choice(k, k)).ToArray();
            if (!wp.Pages.ContainsKey(wp.Start ?? ""))
                pageChoices = pageChoices.Append(new Choice(wp.Start ?? "", (wp.Start ?? "") + "  (missing: add a page with this name)")).ToArray();
            wrist.AddRow("", Check($"Show buttons on my {armName} forearm", wp.Enabled, v => wp.Enabled = v),
                $"Appears only when you turn the {armName} forearm toward your eyes AND look at it. Your {otherHand} hand presses the buttons.");
            Label lFirst = null, lRot = null;
            var other = p.WristFor(wristHand == 1 ? 0 : 1);
            if (wristHand >= 0)
                wrist.AddRow("", Check($"Mirror the {otherHand} panel (same pages and buttons)", wp.Mirror, v => { wp.Mirror = v; if (v) other.Mirror = false; FormPanel.Show(lFirst, !v); }),
                    $"Untick to give the {armName} forearm its own pages. Ticking this unticks it on the {otherHand} panel (they can't both mirror). Size, side and the settings below stay separate either way.");
            lFirst = wrist.AddRow("First page", Pick(pageChoices, wp.Start, v => wp.Start = v),
                "The page shown when the panel appears. Its pages are listed under it on the left.");
            FormPanel.Show(lFirst, !(wristHand >= 0 && wp.Mirror));
            wrist.AddRow("Side of the arm", Pick(new Choice[] { new("inner", "Inner side / underside (palm side)"), new("back", "Back of the wrist (like a watch)") }, wp.Face, v => wp.Face = v));
            var pose = new ArmPosePreview { Mode = wp.TextUp ?? "fingers", RightArm = wristHand == 1, Profile = p };
            Changed += () => pose.Invalidate(); // size, offsets, columns, side and mirror all show in the picture
            wrist.AddRow("Text direction", Pick(new Choice[] { new("fingers", "Top toward my fingers (hand held up, palm to face)"), new("between", "In between (arm half raised)"), new("along_arm", "Along the forearm (arm held across my body)"), new("custom", "Custom angle (set below, or by turning it in adjust mode)") }, wp.TextUp ?? "fingers", v => { wp.TextUp = v; pose.Mode = v; FormPanel.Show(lRot, v == "custom"); }));
            lRot = wrist.AddRow("Custom angle (deg)", Num(-180, 180, 5, 0, wp.RotateDeg, v => { wp.RotateDeg = v; pose.Invalidate(); }),
                "0 = text along the forearm. Or place the panel by hand in the headset (Adjust positions): grab it and turn your hand.");
            FormPanel.Show(lRot, wp.TextUp == "custom");
            wrist.AddRow("", pose);
            wrist.AddRow("Press buttons by", Pick(new Choice[] { new("both", "Poking or pointing + trigger"), new("poke", "Poking with the other controller"), new("point", "Pointing + trigger") }, wp.Press, v => wp.Press = v));
            wrist.AddRow("Columns", Int(1, 4, 1, wp.Columns, v => wp.Columns = v));
            wrist.AddRow("Placed by hand", ResetPose(() => wp.Pose != null, () => wp.Pose = null),
                "Adjust positions in the headset: grab the panel with your other hand's grip and put it anywhere on the arm, at any angle. That spot replaces side, text direction, toward the elbow, off the arm and sideways.");
            wrist.AddRow("Panel size (m)", Num(0.05m, 0.4m, 0.01m, 2, wp.Size, v => wp.Size = v), "0.11 m is about the width of a large phone.");
            wrist.AddRow("Toward the elbow (m)", Num(-0.1m, 0.3m, 0.01m, 2, wp.TowardElbow, v => wp.TowardElbow = v), "Distance back along the forearm from the controller.");
            wrist.AddRow("Off the arm (m)", Num(-0.1m, 0.15m, 0.005m, 3, wp.Lift, v => wp.Lift = v));
            wrist.AddRow("Sideways (m)", Num(-0.2m, 0.2m, 0.005m, 3, wp.Side, v => wp.Side = v), "Across the forearm. Easiest to set by hand: Adjust positions in the headset.");
            wrist.AddRow("Arm must face me within (deg)", Num(5, 80, 1, 0, wp.FacingDeg, v => wp.FacingDeg = v), "Smaller = you must turn the arm more deliberately.");
            wrist.AddRow("Must look within (deg)", Num(5, 60, 1, 0, wp.GazeDeg, v => wp.GazeDeg = v), "How close to the centre of your view the panel must be.");
            wrist.AddRow("Show after looking (ms)", Int(0, 2000, 50, wp.ShowMs, v => wp.ShowMs = v));
            wrist.AddRow("Hide after looking away (ms)", Int(0, 3000, 50, wp.HideMs, v => wp.HideMs = v));

        }
        return wrist;
    }

    // ---------------- advanced
    FormPanel BuildAdvanced()
    {
        var s = p.Style;
        var th = p.Thresholds;
        var adv = new FormPanel();
        if (Want("advanced"))
        { // only the shown page is built
            adv.AddRow("", Check("Bring the game window to the front before sending keys", p.FocusGameWindow, v => p.FocusGameWindow = v),
                "Turn on if keys end up in the wrong window.");
            adv.AddRow("Trigger / grip counts as pressed at", Num(0.05m, 1, 0.05m, 2, th.PressOn, v => th.PressOn = v));
            adv.AddRow("... and released at", Num(0, 1, 0.05m, 2, th.PressOff, v => th.PressOff = v));
            adv.AddRow("Stick direction counts at", Num(0.1m, 1, 0.05m, 2, th.StickOn, v => th.StickOn = v));
            adv.AddRow("... and released at", Num(0, 1, 0.05m, 2, th.StickOff, v => th.StickOff = v));
            adv.AddRow("Centre circle size", Num(0.15m, 0.7m, 0.02m, 2, s.InnerRatio, v => s.InnerRatio = v));
            adv.AddRow("Gap between segments (deg)", Num(0, 10, 0.5m, 1, s.GapDeg, v => s.GapDeg = v));
            adv.AddRow("Sharpness (pixels)", Int(256, 2048, 128, s.TexturePx, v => s.TexturePx = v), "Higher is sharper but uses more memory.");
            var showExp = new CheckBox { Text = "Show experimental features in the list (⚠ Experimental page)", AutoSize = true, Checked = HeadSmoothingFile.ShowPage };
            showExp.CheckedChanged += (o, e) =>
            {
                if (showExp.Checked && MessageBox.Show(FindForm(), "Experimental features are unfinished and some can cause motion sickness. They are all off until you switch them on.\n\nShow them?",
                        "⚠ Experimental features", MessageBoxButtons.YesNo, MessageBoxIcon.Warning, MessageBoxDefaultButton.Button2) != DialogResult.Yes)
                { showExp.Checked = false; return; }
                HeadSmoothingFile.ShowPage = showExp.Checked;
                (FindForm() as MainForm)?.RebuildList();
            };
            adv.AddRow("", showExp, "Adds the ⚠ Experimental page to Settings. The page stays listed while one of its features is on, so you can always switch it off.");

        }
        return adv;
    }

    // Content stays a comfortable width, centred, however wide the window is.
    protected override void OnResize(EventArgs e)
    {
        base.OnResize(e);
        int side = Math.Max(12, (ClientSize.Width - 940) / 2);
        if (Padding.Left != side)
            Padding = new Padding(side, 8, side, 8);
    }

    // Words on every Settings page, for the search box (built once).
    static Dictionary<string, string> pageWords;
    public static string WordsOf(ProfileModel p, string key)
    {
        if (pageWords == null)
        {
            pageWords = new();
            foreach (var sp in SettingsPage.All)
            {
                try
                { using var e = new SettingsEditor(p, -1, sp.Key); pageWords[sp.Key] = e.SearchText; }
                catch { pageWords[sp.Key] = sp.Title; }
            }
        }
        return pageWords.TryGetValue(key, out var w) ? w : "";
    }

    static OffsetModel Clone(OffsetModel a) => new() { Pitch = a.Pitch, Yaw = a.Yaw, Roll = a.Roll, X = a.X, Y = a.Y, Z = a.Z };
    // the right hand's values flipped for the left hand
    readonly struct Mirror
    {
        readonly OffsetModel a;
        public Mirror(OffsetModel a) { this.a = a; }
        public void To(OffsetModel d) { d.Pitch = a.Pitch; d.Yaw = -a.Yaw; d.Roll = -a.Roll; d.X = -a.X; d.Y = a.Y; d.Z = a.Z; }
    }

    // ---------------- small builders
    Control ResetPose(Func<bool> isSet, Action clear, string notSet = "No, using the numbers below", string back = "Use the numbers below again")
    {
        var f = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = Padding.Empty };
        var l = new Label { AutoSize = true, Padding = new Padding(0, 6, 8, 0) };
        var b = new Button { Text = back, AutoSize = true };
        void Show()
        { bool on = isSet(); l.Text = on ? "Yes, set in the headset" : notSet; b.Enabled = on; }
        b.Click += (o, e) => { clear(); Show(); Fire(); };
        f.Controls.Add(l);
        f.Controls.Add(b);
        Show();
        return f;
    }

    static Label Section(string t) => new()
    {
        Text = t,
        AutoSize = true,
        Dock = DockStyle.Top,
        Font = new Font("Segoe UI", 11f, FontStyle.Bold),
        ForeColor = Color.FromArgb(40, 90, 170),
        Padding = new Padding(0, 10, 0, 4)
    };

    CheckBox Check(string text, bool value, Action<bool> set)
    {
        var c = new CheckBox { Text = text, AutoSize = true, Checked = value };
        c.CheckedChanged += (o, e) => { set(c.Checked); Fire(); };
        return c;
    }
    ChoiceBox Pick(Choice[] items, string value, Action<string> set)
    {
        var c = new ChoiceBox(items, 360) { Code = value };
        c.SelectedIndexChanged += (o, e) => { set(c.Code); Fire(); };
        return c;
    }
    NumericUpDown Num(decimal min, decimal max, decimal step, int dec, float value, Action<float> set)
    {
        var n = new NumericUpDown { Minimum = min, Maximum = max, Increment = step, DecimalPlaces = dec, Width = 90 };
        n.Value = Math.Clamp((decimal)value, min, max);
        n.ValueChanged += (o, e) => { set((float)n.Value); Fire(); };
        return n;
    }
    NumericUpDown Int(int min, int max, int step, int value, Action<int> set)
    {
        var n = new NumericUpDown { Minimum = min, Maximum = max, Increment = step, Width = 90 };
        n.Value = Math.Clamp(value, min, max);
        n.ValueChanged += (o, e) => { set((int)n.Value); Fire(); };
        return n;
    }
    TextBox TextField(string value, Action<string> set)
    {
        var t = new TextBox { Width = 200, Text = value ?? "" };
        t.TextChanged += (o, e) => { set(t.Text); Fire(); };
        return t;
    }
    Control ColorPick(string label, Func<string> get, Action<string> set)
    {
        var b = new Button { Width = 150, Height = 28, TextAlign = ContentAlignment.MiddleCenter, FlatStyle = FlatStyle.Flat, Tag = "swatch" };
        void Show()
        {
            WheelPreview.TryColor(get(), out var c);
            b.Text = label;
            b.BackColor = Color.FromArgb(255, c);
            b.ForeColor = c.GetBrightness() > 0.55f ? Color.Black : Color.White;
        }
        Show();
        b.Click += (o, e) =>
        {
            WheelPreview.TryColor(get(), out var c);
            using var d = new ColorDialog { FullOpen = true, Color = Color.FromArgb(255, c) };
            if (d.ShowDialog() != DialogResult.OK)
                return;
            int alpha = c.IsEmpty ? 255 : c.A;
            set($"#{d.Color.R:X2}{d.Color.G:X2}{d.Color.B:X2}{alpha:X2}");
            Show();
            Fire();
        };
        return b;
    }
}
