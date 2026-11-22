namespace OpenXRCompanionEditor;

// The Right / Left controller page: a picture of the controller, and for the button clicked on it, what each way
// of using it does (press, touch, stick directions) with quick choices to change that, like SteamVR's controller
// bindings. Everything it makes is an ordinary binding in the profile.
public class ControllerButtonsPage : UserControl
{
    static readonly Dictionary<int, string> lastSelected = new(); // per hand, so the page keeps its button when rebuilt

    readonly ProfileModel profile;
    readonly int hand;
    readonly Action<BindingModel> open;
    readonly Action<BindingModel, bool> added;
    readonly Action changed; // a profile setting on this page changed (stick options)
    readonly Func<string> loadLayoutId;
    readonly Action<string> saveLayoutId;
    readonly ControllerPicture picture = new() { Dock = DockStyle.Top, Height = 360 };
    readonly FlowLayoutPanel details = new() { Dock = DockStyle.Top, AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false, Padding = new Padding(0, 6, 0, 12) };
    readonly Label models = new() { AutoSize = true, Margin = new Padding(10, 7, 0, 0) };
    readonly CheckBox frameNative = SteamFrameSetting.NewCheckBox();
    ControllerLayout layout;

    // open: show a binding in its editor. added: a new binding was put in the profile (true = show it in its editor).
    // The owner updates the list after this page's click is done, so the page may be rebuilt then.
    public ControllerButtonsPage(ProfileModel profile, int hand, Action<BindingModel> open, Action<BindingModel, bool> added,
                                 Action changed, Func<string> loadLayoutId, Action<string> saveLayoutId)
    {
        this.changed = changed;
        this.profile = profile;
        this.hand = hand;
        this.open = open;
        this.added = added;
        this.loadLayoutId = loadLayoutId;
        this.saveLayoutId = saveLayoutId;
        Dock = DockStyle.Fill;
        AutoScroll = true;
        Padding = new Padding(12, 8, 12, 8);
        BackColor = Theme.Page;
        layout = ControllerLayouts.ById(loadLayoutId());

        var chooser = new ChoiceBox(ControllerLayouts.All.Select(l => new Choice(l.Id, l.Name)), 330) { Code = layout.Id };
        chooser.SelectedIndexChanged += (s, e) =>
        {
            layout = ControllerLayouts.ById(chooser.Code);
            saveLayoutId(layout.Id);
            ShowAll();
        };
        var chooserRow = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, WrapContents = false, Padding = new Padding(0, 4, 0, 4) };
        chooserRow.Controls.Add(new Label { Text = "Controller:", AutoSize = true, Margin = new Padding(0, 7, 8, 0) });
        chooserRow.Controls.Add(chooser);
        chooserRow.Controls.Add(models);
        chooserRow.Controls.Add(frameNative);

        picture.BackColor = Theme.Card;
        picture.SizeChanged += (s, e) => FitPictureHeight();
        picture.PartClicked += part =>
        {
            lastSelected[hand] = part.Comp;
            ShowDetails();
        };

        // Docked to the top in reverse order: the last added sits highest.
        Controls.Add(details);
        Controls.Add(picture);
        Controls.Add(chooserRow);
        Controls.Add(new Label
        {
            Text = "Click a button on the picture to see what each way of using it does: pressing, touching, pushing the stick. " +
                   "Make it press a key, act as another of the game's buttons, or turn it off for the game. " +
                   "Buttons with a blue number already do something.",
            AutoSize = true,
            MaximumSize = new Size(760, 0),
            Dock = DockStyle.Top,
            ForeColor = Theme.SubText,
            Padding = new Padding(0, 0, 0, 6),
        });
        Controls.Add(new Label { Text = (hand == 0 ? "Left" : "Right") + " controller buttons", AutoSize = true, Dock = DockStyle.Top, Font = new Font("Segoe UI", 14f, FontStyle.Bold), Padding = new Padding(0, 0, 0, 4) });
        ShowAll();
    }

    // Which part a page for this hand starts with (screenshots; otherwise the last one clicked).
    internal static void StartWith(int hand, string comp) => lastSelected[hand] = comp;

    ControllerPart SelectedPart
    {
        get
        {
            var parts = layout.PartsFor(hand).ToList();
            string comp = lastSelected.GetValueOrDefault(hand);
            return parts.FirstOrDefault(p => p.Comp == comp) ?? parts.FirstOrDefault(p => p.IsUsable);
        }
    }

    void ShowAll()
    {
        models.Text = "For: " + layout.Models;
        frameNative.Visible = layout.Id == "frame";
        picture.ShowController(layout, hand, SelectedPart?.Comp, p => QuickBindings.UseOf(profile.Bindings, hand, p), p => TagLinesOf(profile.Bindings, hand, p));
        FitPictureHeight();
        ShowDetails();
    }

    void ShowDetails()
    {
        var part = SelectedPart;
        picture.Selected = part?.Comp;
        SuspendLayout();
        details.SuspendLayout();
        foreach (Control c in details.Controls.Cast<Control>().ToList())
            c.Dispose();
        details.Controls.Clear();
        if (part != null)
        {
            details.Controls.Add(new Label { Text = part.LabelFor(hand), AutoSize = true, Font = new Font("Segoe UI", 12.5f, FontStyle.Bold), Margin = new Padding(0, 4, 0, 4) });
            if (!part.IsUsable)
                details.Controls.Add(Note(part.NotUsable));
            else
            {
                if (part.Comp == "thumbstick")
                    details.Controls.Add(StickOptionsCard());
                foreach (var way in part.Ways)
                    details.Controls.Add(WayCard(part, way));
            }
        }
        if (Theme.IsDark)
            ThemeWalker.Apply(details);
        details.ResumeLayout();
        ResumeLayout();
    }

    // This hand's stick options, saved with the profile. All off at 0: the stick works as before.
    Control StickOptionsCard()
    {
        var card = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            AutoSize = true,
            BackColor = Theme.Card,
            Padding = new Padding(12, 8, 12, 8),
            Margin = new Padding(0, 0, 0, 8),
            MinimumSize = new Size(720, 0),
        };
        card.Controls.Add(new Label { Text = "Stick options (saved with the profile, 0 = off)", AutoSize = true, Font = new Font("Segoe UI", 10.5f, FontStyle.Bold) });
        var stick = StickOf(hand) ?? new StickModel();
        var form = new FormPanel { Dock = DockStyle.None };
        var preview = new StickZonesPreview { BackColor = Theme.Card, Margin = new Padding(24, 0, 0, 0) };
        void Redraw() => preview.ShowZones(stick.InnerDeadzone, stick.OuterDeadzone, stick.DirectionAngle, profile.Thresholds?.StickOn ?? 0.7f);
        NumericUpDown Number(decimal max, decimal value, Action<float> set)
        {
            var n = new NumericUpDown { Minimum = 0, Maximum = max, Increment = 1, Width = 80, Value = Math.Clamp(value, 0, max) };
            n.ValueChanged += (s, e) =>
            {
                set((float)n.Value);
                SaveStick(stick);
                Redraw();
            };
            return n;
        }
        form.AddRow("Inner deadzone (%)", Number(90, (decimal)Math.Round(stick.InnerDeadzone * 100), v => stick.InnerDeadzone = v / 100f),
                    "What the game sees: pushes smaller than this count as none (stops drift). The rest is stretched, so the stick still reaches full at the edge. 10 to 15 is typical.");
        form.AddRow("Outer deadzone (%)", Number(90, (decimal)Math.Round(stick.OuterDeadzone * 100), v => stick.OuterDeadzone = v / 100f),
                    "What the game sees: the last part toward the edge already counts as fully pushed, for sticks that never quite reach the edge. 5 to 10 is typical.");
        form.AddRow("Direction zone (°)", Number(45, (decimal)Math.Round(stick.DirectionAngle), v => stick.DirectionAngle = v),
                    "For your bindings on Up / Down / Left / Right: a direction only counts while the stick points within this many degrees of it, so pushing forward and leaning a little sideways doesn't also fire Left or Right. 45 = always the nearest direction. 30 leaves a gap between them.");
        Redraw();
        var row = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = Padding.Empty };
        row.Controls.Add(form);
        row.Controls.Add(preview);
        card.Controls.Add(row);
        return card;
    }

    StickModel StickOf(int h) => h == 0 ? profile.Sticks?.Left : profile.Sticks?.Right;

    // Stored only while something is set, so untouched profiles stay as they were.
    void SaveStick(StickModel stick)
    {
        var keep = stick.IsOff ? null : stick;
        profile.Sticks ??= new SticksModel();
        if (hand == 0)
            profile.Sticks.Left = keep;
        else
            profile.Sticks.Right = keep;
        if (profile.Sticks.Left == null && profile.Sticks.Right == null)
            profile.Sticks = null;
        changed();
    }

    static Label Note(string text) => new() { Text = text, AutoSize = true, MaximumSize = new Size(720, 0), ForeColor = Theme.SubText, Margin = new Padding(0, 2, 0, 8) };

    // One way of using the part: what it does now, and the quick choices.
    Control WayCard(ControllerPart part, string way)
    {
        string input = ControllerLayouts.InputOf(hand, part, way);
        string wayName = ControllerLayouts.WayName(part, way);
        string what = QuickBindingDialogs.NameOf(layout, hand, part, way);
        var card = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            AutoSize = true,
            BackColor = Theme.Card,
            Padding = new Padding(12, 8, 12, 8),
            Margin = new Padding(0, 0, 0, 8),
            MinimumSize = new Size(720, 0),
        };
        card.Controls.Add(new Label { Text = wayName, AutoSize = true, Font = new Font("Segoe UI", 10.5f, FontStyle.Bold), Margin = new Padding(0, 0, 0, 2) });
        // What the button normally does first; bindings that only work in a layer (shift...) after, then switched-off ones.
        var bound = profile.Bindings.Where(b => QuickBindings.FiredBy(b, input))
                                    .OrderBy(b => IsLayerOnly(b))
                                    .ThenBy(b => !b.Enabled)
                                    .ToList();
        if (bound.Count == 0)
            card.Controls.Add(Note("Nothing set: the game gets it as usual."));
        string shownLayer = null;
        foreach (var bd in bound)
        {
            if (IsLayerOnly(bd) && bd.Layers != shownLayer) // a small heading for each layer's group
            {
                shownLayer = bd.Layers;
                card.Controls.Add(new Label { Text = "In layer " + bd.Layers, AutoSize = true, ForeColor = Theme.SubText, Font = new Font("Segoe UI", 8.5f, FontStyle.Bold), Margin = new Padding(0, 6, 0, 0) });
            }
            card.Controls.Add(BindingRow(bd));
        }
        card.Controls.Add(AddMenuButton(input, what, IsTouchOnly(part, way), way is "up" or "down" or "left" or "right"));
        return card;
    }

    // The picture grows with its tags (a tag lists what its part does).
    void FitPictureHeight()
    {
        int h = picture.HeightFor(Math.Max(picture.Width, 1));
        if (picture.Width > 0 && picture.Height != h)
            picture.Height = h;
    }

    // What a part's tag lists, SteamVR style: every way of using it, with what it does or "Default" (the game's own control); then the bindings
    // that only work in a layer.
    public static IReadOnlyList<TagLine> TagLinesOf(IEnumerable<BindingModel> bindings, int hand, ControllerPart part)
    {
        var lines = new List<TagLine>();
        if (!part.IsUsable)
            return lines;
        foreach (bool layerOnes in new[] { false, true })
            foreach (var way in part.Ways)
            {
                string input = ControllerLayouts.InputOf(hand, part, way);
                var bound = bindings.Where(b => b.Enabled && IsLayerOnly(b) == layerOnes && QuickBindings.FiredBy(b, input)).ToList();
                foreach (var b in bound)
                    lines.Add(new TagLine(ShortWhen(part, way, b), DoesText(b.Action), layerOnes ? b.Layers : null));
                if (bound.Count == 0 && !layerOnes)
                    lines.Add(new TagLine(WayWord(part, way), "Default", IsUnused: true)); // what the game does with it
            }
        return lines;
    }

    // A way of using a part in one word: "Press", "Pull", "Squeeze", "Click", "Touch", "Up"...
    static string WayWord(ControllerPart part, string way) => way switch
    {
        "touch" => "Touch",
        "full" => part.Comp == "grip" ? "Full squeeze" : "Full pull",
        "up" => "Up",
        "down" => "Down",
        "left" => "Left",
        "right" => "Right",
        _ => part.Comp switch { "trigger" => "Pull", "grip" => "Squeeze", "thumbrest" => "Touch", "thumbstick" => "Click", _ => "Press" },
    };

    // "Press", "Hold", "Touch", "Up", "Up, hold", "Pull"...
    static string ShortWhen(ControllerPart part, string way, BindingModel b)
    {
        string gesture = WhenText(b);
        string wayWord = way switch
        {
            "touch" => "Touch",
            "full" => part.Comp == "grip" ? "Full squeeze" : "Full pull",
            "up" => "Up",
            "down" => "Down",
            "left" => "Left",
            "right" => "Right",
            _ => null,
        };
        if (wayWord == null && b.Gesture == GestureKind.Press)
            return WayWord(part, way);
        if (wayWord == null)
            return gesture;
        return b.Gesture == GestureKind.Press ? wayWord : wayWord + ", " + gesture.ToLowerInvariant();
    }

    static bool IsTouchOnly(ControllerPart part, string way) => way == "touch" || part.Comp == "thumbrest";

    static bool IsLayerOnly(BindingModel b) => !string.IsNullOrWhiteSpace(b.Layers);

    // How it is used, in two words: "Press", "Hold", "With Left grip: Double-tap".
    static string WhenText(BindingModel b)
    {
        string gesture = b.Gesture switch
        {
            GestureKind.Tap => "Quick tap",
            GestureKind.Hold => "Hold",
            GestureKind.DoubleTap => "Double-tap",
            GestureKind.WhileHeld => "While held",
            GestureKind.DoubleTapHold => "2× hold",
            _ => "Press",
        };
        var parts = (b.Input ?? "").Split('+').Select(p => p.Trim()).Where(p => p.Length > 0).ToList();
        return parts.Count > 1 ? "With " + InputText.Friendly(string.Join("+", parts.Take(parts.Count - 1))) + ": " + gesture : gesture;
    }

    // What it does, short: "Key: Space", "Key: Ctrl (hold)", "Controller: Right B", "Wheel 'right'".
    static string DoesText(ActionModel a)
    {
        string mode = a?.Mode switch { "hold" => " (hold)", "toggle" => " (toggle)", _ => "" };
        return a?.Type switch
        {
            ActionType.Key => "Key: " + (a.Keys ?? "?") + mode,
            ActionType.XrInput => "Controller: " + InputText.Friendly(a.Target) + mode,
            ActionType.None => "Disabled in the game",
            ActionType.Wheel => $"Wheel '{a.Wheel}'",
            ActionType.Keyboard => "VR keyboard",
            ActionType.Layer => $"Layer '{a.Layer}'" + mode,
            ActionType.Sequence => "Macro",
            _ => MainForm.Plain(a),
        };
    }

    // One binding: how it's used, what it does (click to edit), its name. Greyed when switched off.
    Control BindingRow(BindingModel b)
    {
        bool isOn = b.Enabled;
        var row = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(IsLayerOnly(b) ? 14 : 0, 1, 0, 1) };
        row.Controls.Add(new Label { Text = WhenText(b), AutoSize = false, Width = 150, Height = 20, ForeColor = isOn ? Theme.SubText : Theme.OffText, Margin = new Padding(0, 2, 6, 0) });
        var does = new LinkLabel
        {
            Text = DoesText(b.Action),
            AutoSize = true,
            MinimumSize = new Size(150, 0), // the names line up
            MaximumSize = new Size(330, 0),
            LinkColor = isOn ? Theme.Link : Theme.OffText,
            LinkBehavior = LinkBehavior.HoverUnderline,
            Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
            Margin = new Padding(0, 2, 8, 0),
        };
        does.LinkClicked += (s, e) => open(b);
        row.Controls.Add(does);
        string extra = (b.Block == "none" && b.Action?.Type != ActionType.None ? "game also gets the button · " : "") + (isOn ? "" : "off · ");
        row.Controls.Add(new Label { Text = extra + b.Name, AutoSize = true, MaximumSize = new Size(240, 0), ForeColor = Theme.OffText, Margin = new Padding(0, 3, 0, 0) });
        FormPanel.Tips.SetToolTip(does, "Click to edit '" + b.Name + "'");
        return row;
    }

    // One "+ Add" button per way of using a button; its menu holds the quick choices.
    Control AddMenuButton(string input, string what, bool isTouchOnly, bool isStickDirection)
    {
        var add = new Button { Text = "+ Add…", AutoSize = true, Height = 28, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, Margin = new Padding(0, 6, 0, 0), BackColor = Theme.Button, ForeColor = Theme.Text };
        add.FlatAppearance.BorderColor = Theme.CardBorder;
        var menu = new ContextMenuStrip();
        void Item(string text, Func<BindingModel> make, bool holdOnly = false)
            => menu.Items.Add(text, null, (s, e) =>
            {
                var made = make();
                if (made == null)
                    return;
                if (holdOnly)
                    QuickBindings.OnlyWhenHeld(made);
                profile.Bindings.Add(made);
                added(made, false);
                ShowAll();
            });
        BindingModel Key() => QuickBindingDialogs.AskKey(FindForm(), what) is string keys ? QuickBindings.Key(input, what, keys) : null;
        BindingModel Remap() => QuickBindingDialogs.AskGameButton(FindForm(), what, layout, hand) is var (target, name) ? QuickBindings.GameButton(input, what, target, name) : null;
        Item("Press a key…", Key);
        if (!isTouchOnly) // touching can't stand in for another button or be hidden without hiding its presses
        {
            Item("Act as another of the game's buttons…", Remap);
            Item("Turn off in the game", () => QuickBindings.OffInGame(input, what));
            menu.Items.Add(new ToolStripSeparator());
            Item("Hold: press a key (a quick tap still reaches the game)…", Key, holdOnly: true);
            Item("Hold: act as another button (a quick tap still reaches the game)…", Remap, holdOnly: true);
            menu.Items.Add(new ToolStripSeparator());
            Item(isStickDirection ? "Double flick: press a key, held while you keep pushing (sprint)…" : "Double-tap and hold: press a key, held until you let go…",
                 () => QuickBindingDialogs.AskKey(FindForm(), what) is string keys ? QuickBindings.DoubleFlickKey(input, what, keys) : null);
        }
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("Something else (wheel, macro, layer…)", null, (s, e) =>
        {
            var b = QuickBindings.Blank(input, what);
            profile.Bindings.Add(b);
            added(b, true); // finished in the binding editor
        });
        if (Theme.IsDark)
            ThemeWalker.Apply(menu);
        add.Click += (s, e) => menu.Show(add, new Point(0, add.Height));
        return add;
    }
}
