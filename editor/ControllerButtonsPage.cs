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
        var boundAll = profile.Bindings.Where(b => QuickBindings.FiredBy(b, input)).ToList();
        if (BindingEditor.IsAnalog(input))
            card.Controls.Add(PressPointRow(input, boundAll));
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

    // How far the trigger / grip must go before this way's bindings fire (hair trigger ... long pull). The game reads the
    // analog value itself, so with nothing set there is nothing to change yet.
    Control PressPointRow(string input, List<BindingModel> bound)
    {
        bool full = input.EndsWith(".full");
        var row = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0, 2, 0, 4) };
        row.Controls.Add(new Label { Text = full ? "Squeeze strength (%)" : "Press point (%)", AutoSize = true, Margin = new Padding(0, 4, 6, 0) });
        double cur = bound.Count > 0 ? bound[0].PressAt : 0;
        var at = new NumericUpDown { Minimum = 0, Maximum = 95, Increment = 5, Width = 64, Value = (decimal)Math.Clamp(Math.Round(cur * 100), 0, 95), Enabled = bound.Count > 0 };
        at.ValueChanged += (s, e) =>
        {
            double v = at.Value <= 1 ? 0 : (double)at.Value / 100;
            foreach (var b in bound)
                b.PressAt = v;
            changed();
        };
        row.Controls.Add(at);
        bool mixed = bound.Select(b => b.PressAt).Distinct().Count() > 1;
        string hint = bound.Count == 0 ? "Add something below first; then set how far it must go."
                    : mixed ? "Your bindings here use different points; changing this sets them all."
                    : full ? "0 = usual. Higher = squeeze harder past the closed grip."
                    : "0 = usual. Low (10) = hair trigger, high (90) = a long pull.";
        row.Controls.Add(new Label { Text = hint, AutoSize = true, ForeColor = Theme.SubText, Margin = new Padding(8, 4, 0, 0) });
        return row;
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
        BindingModel Key() => QuickBindingDialogs.AskKey(FindForm(), what) is string keys ? QuickBindings.Key(input, what, keys) : null;
        BindingModel Remap() => QuickBindingDialogs.AskGameButton(FindForm(), what, layout, hand) is var (target, name) ? QuickBindings.GameButton(input, what, target, name) : null;
        var options = AddOptions(isTouchOnly, isStickDirection);
        add.Click += (s, e) =>
        {
            int pick = OptionPicker.Show(FindForm(), "Add to " + what, $"What should {what} do? Pick one; you can change it afterwards.", options.Select(o => o.option).ToList());
            if (pick < 0)
                return;
            var kind = options[pick].kind;
            BindingModel made = kind switch
            {
                AddKind.Key or AddKind.HoldKey => Key(),
                AddKind.Remap or AddKind.HoldRemap => Remap(),
                AddKind.Off => QuickBindings.OffInGame(input, what),
                AddKind.DoubleFlick => QuickBindingDialogs.AskKey(FindForm(), what) is string keys ? QuickBindings.DoubleFlickKey(input, what, keys) : null,
                _ => QuickBindings.Blank(input, what),
            };
            if (made == null)
                return;
            if (kind is AddKind.HoldKey or AddKind.HoldRemap)
                QuickBindings.OnlyWhenHeld(made);
            bool finish = kind == AddKind.Other; // finished in the binding editor
            profile.Bindings.Add(made);
            added(made, finish);
            if (!finish)
                ShowAll();
        };
        return add;
    }

    enum AddKind { Key, Remap, Off, HoldKey, HoldRemap, DoubleFlick, Other }

    // The choices of "+ Add...", each with what it does and what it is good for.
    static List<(PickOption option, AddKind kind)> AddOptions(bool isTouchOnly, bool isStickDirection)
    {
        var o = new List<(PickOption, AddKind)>();
        void Opt(string group, string title, string does, string example, AddKind kind) => o.Add((new PickOption(group, title, does, example), kind));
        Opt("When you use it", "Press a key",
            "Pressing it presses a keyboard key or mouse button of your choice, for as long as you hold it.",
            "game functions that only have a keyboard key, like a map, inventory, quick save (F5) or push-to-talk.", AddKind.Key);
        if (!isTouchOnly) // touching can't stand in for another button or be hidden without hiding its presses
        {
            Opt("When you use it", "Act as another button",
                "The game sees a different controller button instead of this one, so you can move a game control to where it feels better.",
                "swapping jump and crouch, or putting a hard-to-reach button on your other hand.", AddKind.Remap);
            Opt("When you use it", "Turn off in the game",
                "The game no longer sees this button at all. Your own bindings on it still work.",
                "a button you keep pressing by accident, or one you want only for your own bindings.", AddKind.Off);
            Opt("Hold it", "Hold: press a key",
                "Holding it for a moment presses a key. A quick tap still does what the game normally does, so you get two functions on one button.",
                "keeping the game's tap action and adding a second one on hold, like hold B for the map.", AddKind.HoldKey);
            Opt("Hold it", "Hold: act as another button",
                "Holding it acts as another controller button. A quick tap still reaches the game as usual.",
                "reaching a second game control without giving up the first one.", AddKind.HoldRemap);
            Opt(isStickDirection ? "Flick it twice" : "Tap it twice",
                isStickDirection ? "Double flick: press a key" : "Double-tap and hold: press a key",
                isStickDirection ? "Flick the stick this way twice quickly, then keep pushing: the key is held while you push. The game still gets the stick as usual."
                                 : "Tap twice quickly and keep holding the second time: the key is held until you let go.",
                isStickDirection ? "sprinting with a double flick forward, or a dodge on a double flick sideways."
                                 : "a second hold action on a button that already has a tap and a hold.", AddKind.DoubleFlick);
        }
        Opt("More", "Something else",
            "Opens the full binding editor: radial wheels, key sequences (macros), layers, the screen mirror and every other action and timing setting.",
            "opening a radial menu, typing a sequence of keys, or a shift layer that changes what other buttons do.", AddKind.Other);
        return o;
    }

    // A picture of the "+ Add..." popup for a stick direction (editor --picker-shot).
    public static void AddPickerShot(string path)
        => OptionPicker.Shot(path, "Add to Left thumbstick: push up", "What should Left thumbstick: push up do? Pick one; you can change it afterwards.",
                             AddOptions(false, true).Select(x => x.option).ToList());
}
