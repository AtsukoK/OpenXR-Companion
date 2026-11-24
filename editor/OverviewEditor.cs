namespace OpenXRCompanionEditor;

// Start page: one card per feature with a clear ON / MIRRORED / OFF status and the switches for it.
public class OverviewEditor : EditorBase
{
    static readonly Color On = Color.FromArgb(46, 158, 91), Off = Color.FromArgb(150, 150, 150), Mirrored = Color.FromArgb(60, 120, 220);
    readonly ProfileModel p;
    readonly Action<object> go;
    readonly FlowLayoutPanel cards = new() { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Dock = DockStyle.Top };

    public OverviewEditor(ProfileModel model, Action<object> goTo)
    {
        p = model;
        go = goTo;
        Stack(ProfileBar(), cards); // the tip bar above says how it works: no title or help text, so it all fits on one screen
        Build();
    }

    readonly Dictionary<string, CardPanel> made = new();
    bool cardsHooked;
    Action arrangeCards; // the current page's card layout, re-run when the page changes width

    // Two columns of matching pairs, left-hand things on the left: the window's width is used and the page is shorter.
    void PlaceInPairs()
    {
        var pairs = new[]
        {
            ("Master switch", "VR keyboard"),
            ("Left controller wheel", "Right controller wheel"),
            ("Left wrist panel", "Right wrist panel"),
            ("Other bindings", "Tools on your menus"),
        };
        var grid = new TableLayoutPanel { AutoSize = true, Dock = DockStyle.Top, Margin = Padding.Empty, Padding = Padding.Empty };
        var order = pairs.SelectMany(p => new[] { p.Item1, p.Item2 }).Where(made.ContainsKey).Select(t => made[t]).ToList();
        foreach (var c in order)
        {
            c.MinimumSize = new Size(460, 0);
            c.Dock = DockStyle.Fill;
        }
        cards.Controls.Add(grid);
        // Two columns when the page is wide enough for two cards, else one (narrow windows, high Windows scaling).
        int lastCols = 0;
        void Arrange()
        {
            int width = cards.ClientSize.Width - 8;
            int cols = width >= 940 * DeviceDpi / 96 ? 2 : 1;
            grid.MinimumSize = new Size(Math.Max(cols == 2 ? 940 : 460, width), 0); // AutoSize: a minimum, not a width
            if (cols == lastCols)
                return;
            lastCols = cols;
            grid.SuspendLayout();
            grid.Controls.Clear();
            grid.ColumnStyles.Clear();
            grid.RowStyles.Clear();
            grid.ColumnCount = cols;
            for (int k = 0; k < cols; k++)
                grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f / cols));
            for (int i = 0; i < order.Count; i++)
            {
                int col = i % cols, row = i / cols;
                if (col == 0)
                    grid.RowStyles.Add(new RowStyle(SizeType.AutoSize));
                order[i].Margin = cols == 1 ? new Padding(0, 0, 0, 8) : new Padding(col == 0 ? 0 : 6, 0, col == 0 ? 6 : 0, 8);
                grid.Controls.Add(order[i], col, row);
            }
            grid.ResumeLayout();
        }
        arrangeCards = Arrange;
        Arrange();
        if (!cardsHooked)
        { cardsHooked = true; cards.SizeChanged += (s, e) => arrangeCards?.Invoke(); }
    }

    // Top middle: a big button with the profile you are on (the one your games use); click it to switch.
    static Control ProfileBar()
    {
        var bar = new Panel { Dock = DockStyle.Top, Height = 54 };
        var main = MainForm.Current;
        string name = main?.CurrentProfileName ?? "";
        var b = new Button
        {
            Text = "Profile:  " + Path.GetFileNameWithoutExtension(name) + "   \u25BE", AutoSize = true, Height = 40, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand,
            Font = new Font("Segoe UI", 12.5f, FontStyle.Bold), BackColor = Color.FromArgb(40, 110, 200), ForeColor = Color.White,
            Padding = new Padding(18, 0, 18, 0), Tag = "swatch",
        };
        b.FlatAppearance.BorderSize = 0;
        FormPanel.Tips.SetToolTip(b, "The profile you are editing, and the one your games use now (" + name + "). Click to switch to another profile.");
        b.Click += (s, e) =>
        {
            var menu = new ContextMenuStrip { Font = new Font("Segoe UI", 11f) };
            foreach (var file in main?.ProfileFiles ?? Array.Empty<string>())
            {
                var f2 = file;
                var item = new ToolStripMenuItem(Path.GetFileNameWithoutExtension(f2) + "   (" + f2 + ")") { Checked = f2 == name };
                item.Click += (o, a) => main?.PickProfile(f2);
                menu.Items.Add(item);
            }
            if (Theme.IsDark)
                ThemeWalker.Apply(menu);
            menu.Show(b, new Point(0, b.Height));
        };
        bar.Controls.Add(b);
        void Center() { b.Location = new Point(Math.Max(0, (bar.ClientSize.Width - b.Width) / 2), 4); }
        bar.Resize += (s, e) => Center();
        b.SizeChanged += (s, e) => Center();
        return bar;
    }

    static string Hand(BindingModel b) => (b.Input ?? "").Split('+').Last().Trim().StartsWith("left") ? "left" : "right";

    void Build()
    {
        cards.SuspendLayout();
        cards.Controls.Clear();
        made.Clear();

        // Overlay master switch
        {
            var c = Card("Master switch", p.Enabled ? "ON" : "OFF", p.Enabled ? On : Off,
                "Turns the whole OpenXR Companion layer on or off for this profile. Everything below only works while it is on.", null, MiniPics.Headset(72, 54, p.Enabled));
            c.Body.Controls.Add(Switch("Layer on for this profile", p.Enabled, v => p.Enabled = v));
            c.Go("Settings (look, picking items)", () => go(p));
        }

        // Wheels per hand
        foreach (var hand in new[] { "right", "left" })
        {
            var openers = p.Bindings.Where(b => b.Action?.Type == ActionType.Wheel && Hand(b) == hand).ToList();
            bool any = openers.Any(b => b.Enabled);
            string title = hand == "right" ? "Right controller wheel" : "Left controller wheel";
            var c = Card(title, any ? "ON" : "OFF", any ? On : Off,
                openers.Count == 0 ? $"No binding opens a wheel from the {hand} controller. Add one under Bindings (action: Open a wheel)."
                                   : "How to open it in the headset:",
                openers.FirstOrDefault() ?? (object)"bindings",
                MiniPics.Wheel(72, 54, hand == "left" ? 0 : 1, openers.Select(b => p.Wheels.GetValueOrDefault(b.Action.Wheel ?? "")).FirstOrDefault(w => w != null), any));
            foreach (var b in openers)
                c.Body.Controls.Add(Switch(HowTo(b), b.Enabled, v => b.Enabled = v, $"opens wheel '{b.Action.Wheel}'"));
            foreach (var wn in openers.Select(b => b.Action.Wheel).Distinct())
                if (wn != null && p.Wheels.TryGetValue(wn, out var wheel))
                    c.Go($"Edit wheel '{wn}'", () => go(wheel));
            foreach (var b in openers)
                c.Go(openers.Count > 1 ? $"Edit binding '{b.Name}'" : "Edit the button that opens it", () => go(b));
            if (openers.Count == 0)
                c.Go("All bindings", () => go(MainForm.BindingsPage));
        }

        // Wrist panels
        for (int h = 0; h < 2; h++)
        {
            var wm = p.WristFor(h);
            string side = h == 0 ? "Left" : "Right", other = h == 0 ? "right" : "left";
            string state = !wm.Enabled ? "OFF" : wm.Mirror ? "MIRRORED" : "ON";
            Color col = !wm.Enabled ? Off : wm.Mirror ? Mirrored : On;
            var src = p.WristPages(h);
            int buttons = src.Pages.TryGetValue(src.Start ?? "", out var first) ? first.Items.Count(i => i.Enabled) : 0;
            string desc = !wm.Enabled ? $"Hidden. Turn it on to get buttons on your {side.ToLower()} forearm."
                : wm.Mirror ? $"Shows the {other} panel's pages ({buttons} buttons on its first page). Pressed with your {other} hand."
                : $"Own pages: {src.Pages.Count} page(s), {buttons} buttons on the first. Pressed with your {other} hand.";
            var c = Card($"{side} wrist panel", state, col, desc, h == 0 ? "wrist0" : "wrist1", MiniPics.Wrist(72, 54, h, first, state));
            var mode = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 300 };
            mode.Items.AddRange(new object[] { "Off", "On, own pages", $"On, mirror the {other} panel" });
            mode.SelectedIndex = !wm.Enabled ? 0 : wm.Mirror ? 2 : 1;
            mode.SelectedIndexChanged += (s, e) =>
            {
                wm.Enabled = mode.SelectedIndex != 0;
                wm.Mirror = mode.SelectedIndex == 2;
                if (wm.Mirror)
                    p.WristFor(1 - h).Mirror = false; // they can't both mirror
                Fire();
                BeginInvoke(Build);
            };
            c.Body.Controls.Add(mode);
            if (wm.Enabled)
                c.Body.Controls.Add(How($"How to open: turn your {side.ToLower()} forearm toward your face and look at it. Press with your {other} hand (poke, or point and pull the trigger)."));
            string tag = h == 0 ? "wrist0" : "wrist1";
            c.Go("Panel settings (size, side, text)", () => go(tag));
            if (src.Pages.TryGetValue(src.Start ?? "", out var fp))
                c.Go(wm.Mirror ? $"Edit the {other} panel's buttons" : "Edit its buttons", () => go(wm.Mirror ? (h == 0 ? "wrist1" : "wrist0") : tag));
        }

        // Keyboard
        {
            var openers = p.Bindings.Where(b => b.Action?.Type == ActionType.Keyboard).ToList();
            int fromMenus = p.Wheels.Values.Concat(p.WristLeft.Pages.Values).Concat(p.WristRight.Pages.Values)
                .SelectMany(w => w.Items).Count(i => i.Enabled && i.Action?.Type == ActionType.Keyboard);
            bool any = openers.Any(b => b.Enabled) || fromMenus > 0;
            var c = Card("VR keyboard", any ? "ON" : "OFF", any ? On : Off,
                openers.Count + fromMenus > 0 ? "How to open it in the headset:" : "Nothing opens it yet. Add a Keyboard button to a wheel or wrist panel, or a binding.",
                openers.FirstOrDefault() ?? (object)"bindings", MiniPics.Keyboard(72, 54, any));
            foreach (var b in openers)
                c.Body.Controls.Add(Switch(HowTo(b), b.Enabled, v => b.Enabled = v));
            var kb = MenuButtons("keyboard");
            if (kb.Count > 0)
                c.Body.Controls.Add(How("Also on: " + string.Join(" \u00B7 ", kb.Take(2)) + (kb.Count > 2 ? $"  (+{kb.Count - 2} more)" : "")));
            foreach (var b in openers)
                c.Go("Edit the button that opens it", () => go(b));
            c.Go("Keyboard size and position", () => go(p));
        }

        // Everything else
        {
            var rest = p.Bindings.Where(b => b.Action?.Type is not ("wheel" or "keyboard")).ToList();
            int on = rest.Count(b => b.Enabled);
            var c = Card("Other bindings", $"{on} of {rest.Count} on", on > 0 ? On : Off,
                "Keys, macros, layers and game inputs on controller buttons.", rest.FirstOrDefault() ?? (object)"bindings", MiniPics.Controllers(72, 54, on > 0));
            foreach (var b in rest.Take(3))
                c.Body.Controls.Add(Switch(HowTo(b), b.Enabled, v => b.Enabled = v));
            if (rest.Count > 3)
                c.Body.Controls.Add(How($"+{rest.Count - 3} more"));
            c.Go("All bindings", () => go(MainForm.BindingsPage));
        }
        // Tools on the menus (no switch: they are buttons on wheels and wrist panels)
        {
            var tools = new (string type, string name, Func<ActionModel, bool> match)[]
            {
                ("adjust", "Move things by hand (Adjust / Calibrate Positions)", a => true),
                ("screen", "Screen mirror (a monitor or window in VR)", a => a.Mode is null or "toggle" or "on"),
                ("controller_offsets", "Controller offsets", a => true),
                ("settings_menu", "In-game settings menu (bindings, sticks)", a => true),
                ("headset_mirror", "Headset mirror (desktop window)", a => true),
            };
            var c = Card("Tools on your menus", "", On, "Where to find them in the headset:", null, null);
            foreach (var (type, name, match) in tools)
            {
                var where = MenuButtons(type, match);
                string at = where.Count == 0 ? "not on a wheel or wrist panel yet" : where[0] + (where.Count > 1 ? $"  (+{where.Count - 1})" : "");
                var line = How(name + ":  " + at);
                line.MaximumSize = new Size(820, 0);
                c.Body.Controls.Add(line);
            }
        }
        PlaceInPairs();
        cards.ResumeLayout();
        if (Theme.IsDark)
            ThemeWalker.Apply(cards); // rebuilt cards start in light colours
    }

    // "Double-tap Right Thumbrest" (+ the layer it needs)
    static string HowTo(BindingModel b)
    {
        string verb = b.Gesture switch { "double_tap" => "Double-tap", "double_tap_hold" => "Double-tap and hold", "hold" => "Hold", "while_held" => "Hold", "tap" => "Tap", _ => "Press" };
        string s = verb + " " + InputText.Friendly(b.Input);
        if (!string.IsNullOrWhiteSpace(b.Layers))
            s += " (while layer " + b.Layers + " is on)";
        return s;
    }

    // Where buttons with this action sit: "Left wrist panel > More: Controller offsets", "Wheel 'right' > More...: Keyboard"
    List<string> MenuButtons(string type, Func<ActionModel, bool> match = null)
    {
        var list = new List<string>();
        void Scan(string where, IEnumerable<WheelModel> pages)
        {
            foreach (var w in pages)
                foreach (var i in w.Items)
                    if (i.Enabled && i.Action?.Type == type && (match == null || match(i.Action)))
                        list.Add($"{where}, page '{w.Title ?? w.Name}': {i.Label}");
        }
        for (int h = 0; h < 2; h++)
        {
            var wm = p.WristFor(h);
            if (wm.Enabled && !wm.Mirror)
                Scan(h == 0 ? "Left wrist panel" : "Right wrist panel", wm.Pages.Values);
        }
        foreach (var w in p.Wheels.Values)
            foreach (var i in w.Items)
                if (i.Enabled && i.Action?.Type == type && (match == null || match(i.Action)))
                    list.Add($"Wheel '{w.Title ?? w.Name}': {i.Label}");
        return list;
    }

    static Label How(string text) => new() { Text = text, AutoSize = true, MaximumSize = new Size(560, 0), Margin = new Padding(2, 2, 2, 2) };

    static string GestureWord(BindingModel b) =>
        Choices.Gestures.FirstOrDefault(c => c.Code == b.Gesture)?.Text.Split(' ')[0].ToLowerInvariant() ?? b.Gesture;

    static readonly Color SwitchOn = Color.FromArgb(46, 158, 91), SwitchOff = Color.FromArgb(200, 60, 55);
    Control Switch(string text, bool value, Action<bool> set, string detail = null)
    {
        var row = new FlowLayoutPanel { FlowDirection = FlowDirection.LeftToRight, WrapContents = false, AutoSize = true, Margin = new Padding(0, 1, 0, 1) };
        var b = new Button
        {
            Text = value ? "Turn off" : "Turn on",
            AutoSize = true, // fits the text at any Windows scaling
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            MinimumSize = new Size(76, 24),
            Padding = new Padding(6, 2, 6, 2),
            FlatStyle = FlatStyle.Flat,
            Cursor = Cursors.Hand,
            Tag = "swatch", // the theme leaves its colours alone
            BackColor = value ? SwitchOff : SwitchOn, // the colour of what a click does: red = turn off, green = turn on
            ForeColor = Color.White, Font = new Font("Segoe UI", 9f, FontStyle.Bold), Margin = new Padding(0, 0, 10, 0),
        };
        b.FlatAppearance.BorderSize = 0;
        b.Click += (s, e) => { set(!value); Fire(); BeginInvoke(Build); };
        new ToolTip().SetToolTip(b, value ? "It is on: click to turn it off" : "It is off: click to turn it on");
        row.Controls.Add(b);
        var words = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Margin = new Padding(0, 0, 0, 0) };
        words.Controls.Add(new Label { Text = text, AutoSize = true, MaximumSize = new Size(480, 0), Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), Margin = new Padding(0, detail == null ? 5 : 0, 0, 0) });
        if (detail != null)
            words.Controls.Add(new Label { Text = detail, AutoSize = true, MaximumSize = new Size(480, 0), Margin = new Padding(0, 0, 0, 0) });
        row.Controls.Add(words);
        return row;
    }

    class CardPanel : Panel
    {
        public FlowLayoutPanel Body, Buttons;
        public void Go(string text, Action click)
        {
            var b = new Button { Text = text + "  \u25B6", AutoSize = true, Height = 26, FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(40, 90, 170), ForeColor = Color.White, Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), Margin = new Padding(0, 0, 8, 4), Padding = new Padding(6, 0, 6, 0), Cursor = Cursors.Hand };
            b.FlatAppearance.BorderSize = 0;
            b.Click += (s, e) => click();
            Buttons.Controls.Add(b);
        }
    }

    CardPanel Card(string title, string state, Color color, string desc, object editTarget, Image pic = null)
    {
        var card = new CardPanel { Width = 640, MinimumSize = new Size(640, 0), AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, BackColor = Color.White, Margin = new Padding(0, 0, 0, 6), Padding = new Padding(10, 4, 8, 4) };
        var bar = new Panel { Dock = DockStyle.Left, Width = 6, BackColor = color };
        var inner = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Dock = DockStyle.Fill, Padding = new Padding(10, 0, 0, 0) };
        var head = new FlowLayoutPanel { FlowDirection = FlowDirection.LeftToRight, WrapContents = false, AutoSize = true, Margin = Padding.Empty };
        head.Controls.Add(new Label { Text = title, AutoSize = true, Font = new Font("Segoe UI", 12f, FontStyle.Bold), Margin = new Padding(0, 2, 10, 0) });
        if (state != "")
            head.Controls.Add(new Label { Text = " " + state + " ", AutoSize = true, Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), ForeColor = Color.White, BackColor = color, Margin = new Padding(0, 5, 10, 0), Padding = new Padding(4, 2, 4, 2) });
        _ = editTarget; // buttons are added per card below
        inner.Controls.Add(head);
        inner.Controls.Add(new Label { Text = desc, AutoSize = true, MaximumSize = new Size(580, 0), ForeColor = Color.FromArgb(80, 80, 80), Margin = new Padding(0, 1, 0, 2) });
        var body = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Margin = Padding.Empty };
        inner.Controls.Add(body);
        card.Controls.Add(inner);
        if (pic != null)
            card.Controls.Add(new PictureBox { Image = pic, Dock = DockStyle.Left, Width = pic.Width + 14, SizeMode = PictureBoxSizeMode.Normal, Padding = new Padding(10, 4, 0, 0) });
        card.Controls.Add(bar);
        card.Body = body;
        card.Buttons = new FlowLayoutPanel { FlowDirection = FlowDirection.LeftToRight, WrapContents = true, AutoSize = true, MaximumSize = new Size(600, 0), Margin = new Padding(0, 3, 0, 0) };
        inner.Controls.Add(card.Buttons);
        made[title] = card;
        return card;
    }
}
