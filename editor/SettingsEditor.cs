namespace OpenXRHandMenusEditor;

public class SettingsEditor : EditorBase
{
    static readonly Choice[] BlockOpen =
    {
        new("hand", "Everything on the hand(s) using the wheel"), new("minimal", "Only the stick and the confirm / back buttons"),
        new("all", "Both controllers completely"), new("none", "Nothing, the game still sees every button"),
    };
    static readonly Choice[] Anchors =
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

    public SettingsEditor(ProfileModel model, int wristHand = -1)
    {
        p = model;
        p.Thresholds ??= new(); p.Style ??= new(); p.Selection ??= new(); p.Keyboard ??= new(); p.Pointer ??= new(); p.WristLeft ??= new(); p.WristRight ??= new(); p.Style.Colors ??= new();
        var s = p.Style; var sel = p.Selection; var kb = p.Keyboard; var th = p.Thresholds; var col = s.Colors;

        // ---------------- general
        var general = new FormPanel();
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

        // ---------------- wheel position & look
        var look = new FormPanel();
        Label lHandUp = null, lHandToward = null, lHeadDist = null, lHeadDown = null;
        void SyncAnchor()
        {
            bool hand = s.Anchor is "hand" or "hand_locked";
            FormPanel.Show(lHandUp, hand); FormPanel.Show(lHandToward, hand);
            FormPanel.Show(lHeadDist, !hand); FormPanel.Show(lHeadDown, !hand);
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
        look.AddRow("Font", Text(s.Font, v => s.Font = v));
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

        // ---------------- selecting
        var pick = new FormPanel();
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

        // ---------------- pointing
        var ptr = p.Pointer; var aim = new FormPanel();
        Label lPitch = null, lDown = null, lFwd = null;
        void SyncCorr() { bool on = ptr.Correction != "off"; FormPanel.Show(lPitch, on); FormPanel.Show(lDown, on); FormPanel.Show(lFwd, on); }
        aim.AddRow("Correct the laser direction", Pick(new Choice[] { new("auto", "Only on Quest (Touch) controllers"), new("always", "On every controller"), new("off", "Off, use the game's controller ray") }, ptr.Correction, v => { ptr.Correction = v; SyncCorr(); }),
            "Quest controllers aim from above your index finger and about 15 degrees upward. This makes the laser go where a pointing finger goes.");
        lPitch = aim.AddRow("Turn the laser down by (deg)", Num(-45, 45, 1, 0, ptr.PitchDownDeg, v => ptr.PitchDownDeg = v), "15 matches most people on Quest. Saved changes apply in the headset within a second.");
        lDown = aim.AddRow("Move the laser start down (m)", Num(-0.1m, 0.1m, 0.005m, 3, ptr.DownM, v => ptr.DownM = v), "Toward your index finger. Also moves the poking fingertip.");
        lFwd = aim.AddRow("Move the laser start forward (m)", Num(-0.1m, 0.1m, 0.005m, 3, ptr.ForwardM, v => ptr.ForwardM = v));
        aim.AddRow("", Check("Show a fingertip near wrist panels (where your poke lands and the laser starts)", ptr.Finger, v => ptr.Finger = v));
        aim.AddRow("", Check("Keep a short laser showing while a wrist panel is up", ptr.GuideLaser, v => ptr.GuideLaser = v),
            "So you can find your pointer before it reaches the panel. It hands over to the touch ring when your fingertip gets close.");
        SyncCorr();

        // ---------------- screen mirror
        p.Screen ??= new(); var sc = p.Screen; var scr = new FormPanel();
        Label lWW = null, lTE = null, lLift = null, lSide = null, lRot2 = null, lFace = null, lFW = null, lFD = null, lFB = null;
        void SyncPlace()
        {
            bool wr = sc.Place is "wrist_left" or "wrist_right";
            foreach (var l in new[] { lWW, lTE, lLift, lSide, lRot2, lFace }) FormPanel.Show(l, wr);
            foreach (var l in new[] { lFW, lFD, lFB }) FormPanel.Show(l, !wr);
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
        foreach (var n in OpenWindows.Names()) win.Items.Add(n);
        win.TextChanged += (o, e) => { sc.Window = string.IsNullOrWhiteSpace(win.Text) ? null : win.Text.Trim(); sc.WindowTitle = null; Fire(); };
        lWin = scr.AddRow("Window", win, "Part of its title or the program name, e.g. chrome, firefox, Discord, vlc. Pick one that's open now or type it. Empty = the first open window.");
        FormPanel.Show(lMon, sc.Source != "window"); FormPanel.Show(lWin, sc.Source == "window");
        scr.AddRow("Frame rate cap (fps)", Int(0, 240, 5, sc.Fps, v => sc.Fps = v),
            "How often the picture updates at most. 0 = as fast as the headset. Lower uses less CPU and GPU; 15-30 is plenty for reading.");
        var steps = new TextBox { Width = 200, Text = string.Join(", ", sc.FpsSteps ?? new()) };
        steps.TextChanged += (o, e) =>
        {
            var vals = steps.Text.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).Select(x => int.TryParse(x, out var n) ? n : -1).ToList();
            bool ok = vals.Count > 0 && vals.All(n => n >= 0 && n <= 240);
            steps.BackColor = ok ? SystemColors.Window : Color.MistyRose;
            if (ok) { sc.FpsSteps = vals; Fire(); }
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
        scr.AddRow("Button bar size", Num(0.3m, 2, 0.05m, 2, sc.BarScale, v => sc.BarScale = v),
            "The bar under the screen (Choose, Frame rate, Move, Keyboard, Adjust, Hide). 1 = as wide as the screen, 0.7 = the default.");
        scr.AddRow("", Check("Give the game its window back after clicking", sc.RefocusGame, v => sc.RefocusGame = v),
            "Clicking on the desktop takes the focus from the game; this hands it back when you point away.");
        scr.AddRow("Placed by hand", ResetPose(() => sc.Poses is { Count: > 0 }, () => sc.Poses = null),
            "Adjust positions shows the screen; grab it and put it anywhere, at any angle. Each place (left wrist, right wrist, in front, following view) keeps its own spot.");
        scr.AddRow("Sharpness limit (pixels wide)", Int(0, 7680, 160, sc.MaxWidthPx, v => sc.MaxWidthPx = v),
            "0 = automatic: 1920 in 64-bit games, 1280 in 32-bit games (they have little memory to spare). The picture is halved until it fits.");
        SyncPlace();

        // ---------------- keyboard
        var keys = new FormPanel();
        keys.AddRow("Width (m)", Num(0.2m, 1.5m, 0.02m, 2, kb.Width, v => kb.Width = v));
        keys.AddRow("Distance in front (m)", Num(0.2m, 1.5m, 0.02m, 2, kb.Distance, v => kb.Distance = v));
        keys.AddRow("Below eye level (m)", Num(-0.3m, 0.8m, 0.02m, 2, kb.Below, v => kb.Below = v));
        keys.AddRow("", Check("Close the keyboard after Enter (handy for chat boxes)", kb.CloseOnEnter, v => kb.CloseOnEnter = v));

        // ---------------- wrist panel
        var wp = p.WristFor(Math.Max(0, wristHand)); var wrist = new FormPanel();
        string armName = wristHand == 1 ? "right" : "left", otherHand = wristHand == 1 ? "left" : "right";
        var pageChoices = wp.Pages.Keys.Select(k => new Choice(k, k)).ToArray();
        if (!wp.Pages.ContainsKey(wp.Start ?? "")) pageChoices = pageChoices.Append(new Choice(wp.Start ?? "", (wp.Start ?? "") + "  (missing: add a page with this name)")).ToArray();
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

        // ---------------- advanced
        var adv = new FormPanel();
        adv.AddRow("", Check("Bring the game window to the front before sending keys", p.FocusGameWindow, v => p.FocusGameWindow = v),
            "Turn on if keys end up in the wrong window.");
        adv.AddRow("Trigger / grip counts as pressed at", Num(0.05m, 1, 0.05m, 2, th.PressOn, v => th.PressOn = v));
        adv.AddRow("... and released at", Num(0, 1, 0.05m, 2, th.PressOff, v => th.PressOff = v));
        adv.AddRow("Stick direction counts at", Num(0.1m, 1, 0.05m, 2, th.StickOn, v => th.StickOn = v));
        adv.AddRow("... and released at", Num(0, 1, 0.05m, 2, th.StickOff, v => th.StickOff = v));
        adv.AddRow("Centre circle size", Num(0.15m, 0.7m, 0.02m, 2, s.InnerRatio, v => s.InnerRatio = v));
        adv.AddRow("Gap between segments (deg)", Num(0, 10, 0.5m, 1, s.GapDeg, v => s.GapDeg = v));
        adv.AddRow("Sharpness (pixels)", Int(256, 2048, 128, s.TexturePx, v => s.TexturePx = v), "Higher is sharper but uses more memory.");
        var advToggle = new CheckBox { Text = "Show advanced settings", AutoSize = true, Dock = DockStyle.Top, Padding = new Padding(0, 10, 0, 4) };
        adv.Visible = false;
        advToggle.CheckedChanged += (o, e) => adv.Visible = advToggle.Checked;

        if (wristHand >= 0)
        {
            Stack(Title(wristHand == 1 ? "Right wrist panel" : "Left wrist panel"),
                  FormPanel.Header("Buttons on your forearm. They appear only when you turn the forearm toward your eyes and look at it. Its pages are listed under this panel's bar in the list on the left (unfold it with the arrow); select a page to add buttons."),
                  wrist);
            return;
        }
        Stack(Title("Settings"),
              FormPanel.Header("How the overlay behaves in this game. Changes apply when you save."),
              Section("General"), general,
              Section("Wheel position and look"), look,
              Section("Picking items"), pick,
              Section("Pointing (laser and fingertip)"), aim,
              Section("VR keyboard"), keys,
              Section("Screen mirror (a monitor in VR)"), scr,
              advToggle, adv);
    }

    // ---------------- small builders
    Control ResetPose(Func<bool> isSet, Action clear)
    {
        var f = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = Padding.Empty };
        var l = new Label { AutoSize = true, Padding = new Padding(0, 6, 8, 0) };
        var b = new Button { Text = "Use the numbers below again", AutoSize = true };
        void Show() { bool on = isSet(); l.Text = on ? "Yes, set in the headset" : "No, using the numbers below"; b.Enabled = on; }
        b.Click += (o, e) => { clear(); Show(); Fire(); };
        f.Controls.Add(l); f.Controls.Add(b); Show();
        return f;
    }

    static Label Section(string t) => new()
    {
        Text = t, AutoSize = true, Dock = DockStyle.Top, Font = new Font("Segoe UI", 11f, FontStyle.Bold),
        ForeColor = Color.FromArgb(40, 90, 170), Padding = new Padding(0, 10, 0, 4)
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
    TextBox Text(string value, Action<string> set)
    {
        var t = new TextBox { Width = 200, Text = value ?? "" };
        t.TextChanged += (o, e) => { set(t.Text); Fire(); };
        return t;
    }
    Control ColorPick(string label, Func<string> get, Action<string> set)
    {
        var b = new Button { Width = 150, Height = 28, TextAlign = ContentAlignment.MiddleCenter, FlatStyle = FlatStyle.Flat };
        void Show()
        {
            WheelPreview.TryColor(get(), out var c);
            b.Text = label; b.BackColor = Color.FromArgb(255, c);
            b.ForeColor = c.GetBrightness() > 0.55f ? Color.Black : Color.White;
        }
        Show();
        b.Click += (o, e) =>
        {
            WheelPreview.TryColor(get(), out var c);
            using var d = new ColorDialog { FullOpen = true, Color = Color.FromArgb(255, c) };
            if (d.ShowDialog() != DialogResult.OK) return;
            int alpha = c.IsEmpty ? 255 : c.A;
            set($"#{d.Color.R:X2}{d.Color.G:X2}{d.Color.B:X2}{alpha:X2}");
            Show(); Fire();
        };
        return b;
    }
}
