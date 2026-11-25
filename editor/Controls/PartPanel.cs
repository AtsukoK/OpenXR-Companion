using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// One controller button, SteamVR style: for each way of using it (press, touch, each stick direction) what it does now,
// with + Add, Swap with..., Reset to default and its bindings (on/off, duplicate, delete, click to edit); a stick also
// gets its options. Port of the drawer part of the WinForms ControllerButtonsPage (with the stick zones picture).
public class PartPanel : StackPanel
{
    readonly ProfileModel profile = Shell.Store.Profile;
    readonly ControllerLayout layout;
    readonly int hand;
    readonly ControllerPart part;
    readonly Action changed; // the pictures' tags follow

    public PartPanel(ControllerLayout layout, int hand, ControllerPart part, Action changed)
    {
        this.layout = layout;
        this.hand = hand;
        this.part = part;
        this.changed = changed;
        Spacing = 6;
        Build();
    }

    void Build()
    {
        Children.Clear();
        Children.Add(new TextBlock { Text = (hand == 0 ? "Left " : "Right ") + part.LabelFor(hand), FontSize = 18, FontWeight = FontWeight.SemiBold });
        if (!part.IsUsable)
        {
            Children.Add(Note(part.NotUsable));
            return;
        }
        if (part.Comp == "thumbstick")
            Children.Add(StickOptions());
        foreach (var way in part.Ways)
            Children.Add(WayCard(way));
    }

    void Changed()
    {
        Shell.Store.MarkDirty();
        changed?.Invoke();
        Build();
    }

    static TextBlock Note(string text) => new() { Text = text, Classes = { "desc" }, MaxWidth = 720 };

    static Border CardBox(Control body) => new()
    {
        Background = Brush.Parse("#23272E"), CornerRadius = new CornerRadius(6), Padding = new Thickness(12, 6), MinWidth = 720, Child = body,
    };

    // ---- one way of using the part
    Control WayCard(string way)
    {
        string input = ControllerLayouts.InputOf(hand, part, way);
        string what = QuickBindings.NameOf(layout, hand, part, way);
        var boundAll = profile.Bindings.Where(b => QuickBindings.FiredBy(b, input)).ToList();
        var body = new StackPanel { Spacing = 2 };
        var head = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        head.Children.Add(new TextBlock { Text = ControllerLayouts.WayName(part, way), MinWidth = 250, FontWeight = FontWeight.SemiBold, VerticalAlignment = VerticalAlignment.Center });
        head.Children.Add(new TextBlock { Text = boundAll.Count == 0 ? "Default (the game's own)" : "", MinWidth = 200, Foreground = Brush.Parse("#AAB2BF"), VerticalAlignment = VerticalAlignment.Center });
        var add = new Button { Content = "+ Add…" };
        add.Click += (s, e) =>
        {
            // a starter binding that does nothing yet, opened in the binding sheet (as the WinForms AddFor does now)
            var made = QuickBindings.Blank(input, what);
            profile.Bindings.Add(made);
            Shell.Store.MarkDirty();
            Shell.Go("binding:" + profile.Bindings.IndexOf(made));
        };
        head.Children.Add(add);
        if (!(way == "touch" || part.Comp == "thumbrest"))
        {
            var swap = new Button { Content = "⇄ Swap with..." };
            ToolTip.SetTip(swap, "This button and the one you pick trade places in the game (two bindings, one on each).");
            swap.Click += async (s, e) => await Swap(input, what);
            head.Children.Add(swap);
        }
        if (boundAll.Count > 0)
        {
            var reset = new Button { Content = "↺ Reset to default" };
            ToolTip.SetTip(reset, "Removes everything set on " + what + ", so the game gets it as usual.");
            reset.Click += async (s, e) =>
            {
                if (!await Dialogs.Confirm(this, "Reset to default", $"Remove the {boundAll.Count} binding(s) on {what}?\n\nThe game then gets it as usual.", "Remove"))
                    return;
                foreach (var b in boundAll)
                    profile.Bindings.Remove(b);
                Changed();
            };
            head.Children.Add(reset);
        }
        body.Children.Add(head);
        if (IsAnalog(input) && boundAll.Count > 0)
            body.Children.Add(PressPointRow(input, boundAll));
        // what the button normally does first; bindings that only work in a layer after, then switched-off ones
        var bound = boundAll.OrderBy(b => !string.IsNullOrWhiteSpace(b.Layers)).ThenBy(b => !b.Enabled).ToList();
        string shownLayer = null;
        foreach (var bd in bound)
        {
            if (!string.IsNullOrWhiteSpace(bd.Layers) && bd.Layers != shownLayer)
            {
                shownLayer = bd.Layers;
                body.Children.Add(new TextBlock { Text = "In " + bd.Layers + " mode", FontSize = 11, FontWeight = FontWeight.Bold, Foreground = Brush.Parse("#AAB2BF"), Margin = new Thickness(0, 6, 0, 0) });
            }
            body.Children.Add(BindingRow(bd));
        }
        return CardBox(body);
    }

    static bool IsAnalog(string input)
    {
        var last = (input ?? "").Split('+')[^1].Trim().ToLowerInvariant();
        var parts = last.Split('.');
        if (parts.Length < 2) return false;
        string comp = parts[1], way = parts.Length > 2 ? parts[2] : "";
        return (comp is "trigger" or "grip" && way is "" or "full") || (comp == "trackpad" && way == "");
    }

    // How far the trigger / grip must go before this way's bindings fire.
    Control PressPointRow(string input, List<BindingModel> bound)
    {
        bool full = input.EndsWith(".full");
        decimal Shown() => (decimal)Math.Clamp(Math.Round(bound[0].PressAt * 100), 0, 95);
        var at = new NumericUpDown { Minimum = 0, Maximum = 95, Increment = 5, Width = 130, FormatString = "0", Value = Shown() };
        at.ValueChanged += (s, e) =>
        {
            if (at.Value == null || at.Value == Shown()) return; // only a real change
            double v = at.Value <= 1 ? 0 : (double)at.Value / 100;
            foreach (var b in bound)
                b.PressAt = v;
            Shell.Store.MarkDirty();
        };
        bool mixed = bound.Select(b => b.PressAt).Distinct().Count() > 1;
        string hint = mixed ? "Your bindings here use different points; changing this sets them all."
                    : full ? "0 = usual. Higher = squeeze harder past the closed grip."
                    : "0 = usual. Low (10) = hair trigger, high (90) = a long pull.";
        return new StackPanel
        {
            Orientation = Orientation.Horizontal, Spacing = 6, Margin = new Thickness(0, 2, 0, 4),
            Children = { new TextBlock { Text = full ? "Squeeze strength (%)" : "Press point (%)", VerticalAlignment = VerticalAlignment.Center }, at,
                         new TextBlock { Text = hint, Foreground = Brush.Parse("#AAB2BF"), VerticalAlignment = VerticalAlignment.Center } },
        };
    }

    // ---- one binding: how it's used, what it does (click to edit), its name; on/off, duplicate, delete
    static string WhenText(BindingModel b)
    {
        string gesture = b.Gesture switch
        {
            GestureKind.Tap => "Quick tap", GestureKind.Hold => "Hold", GestureKind.DoubleTap => "Double-tap",
            GestureKind.WhileHeld => "While held", GestureKind.DoubleTapHold => "2× hold", _ => "Press",
        };
        var parts = (b.Input ?? "").Split('+').Select(p => p.Trim()).Where(p => p.Length > 0).ToList();
        return parts.Count > 1 ? "With " + InputText.Friendly(string.Join("+", parts.Take(parts.Count - 1))) + ": " + gesture : gesture;
    }

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
            _ => EditorApi.Plain(a),
        };
    }

    Control BindingRow(BindingModel b)
    {
        bool on = b.Enabled, layered = !string.IsNullOrWhiteSpace(b.Layers);
        var does = new Button { Content = DoesText(b.Action), Classes = { "link" }, FontWeight = FontWeight.SemiBold, MinWidth = 150, MaxWidth = 330 };
        if (!on) does.Foreground = Brush.Parse("#7F8896");
        ToolTip.SetTip(does, "Click to edit '" + b.Name + "'");
        does.Click += (s, e) => Shell.Go("binding:" + profile.Bindings.IndexOf(b));
        string extra = (b.Block == "none" && b.Action?.Type != ActionType.None ? "game also gets the button · " : "") + (on ? "" : "off · ");
        var sw = new Button { Content = on ? "On" : "Off", Classes = { "glyph", "onoff" } };
        sw.Classes.Set("on", on);
        ToolTip.SetTip(sw, on ? "On: click to switch it off (kept here)" : "Off: click to switch it on");
        sw.Click += (s, e) => { b.Enabled = !b.Enabled; Changed(); };
        var dup = new Button { Content = "⧉", Classes = { "glyph" } };
        ToolTip.SetTip(dup, "Duplicate (a copy right below it, switched off)");
        dup.Click += (s, e) =>
        {
            var c = ItemLists.Clone(b);
            c.Name += " copy";
            c.Enabled = false;
            profile.Bindings.Insert(profile.Bindings.IndexOf(b) + 1, c);
            Changed();
        };
        var del = new Button { Content = "✕", Classes = { "glyph", "del" } };
        ToolTip.SetTip(del, "Delete '" + b.Name + "'");
        del.Click += async (s, e) =>
        {
            if (!await Dialogs.Confirm(this, "Delete binding", $"Delete '{b.Name}'?", "Delete"))
                return;
            profile.Bindings.Remove(b);
            Changed();
        };
        return new StackPanel
        {
            Orientation = Orientation.Horizontal, Spacing = 4, Margin = new Thickness(layered ? 14 : 0, 1, 0, 1),
            Children =
            {
                new TextBlock { Text = WhenText(b), Width = 210, TextTrimming = TextTrimming.CharacterEllipsis, Foreground = Brush.Parse(on ? "#AAB2BF" : "#7F8896"), VerticalAlignment = VerticalAlignment.Center },
                does,
                new TextBlock { Text = extra + b.Name, MaxWidth = 240, TextTrimming = TextTrimming.CharacterEllipsis, Foreground = Brush.Parse("#7F8896"), VerticalAlignment = VerticalAlignment.Center },
                sw, dup, del,
            },
        };
    }

    // Swap: this button acts as the one picked, and that one acts as this one (two bindings).
    async Task Swap(string input, string what)
    {
        var choices = new List<Choice>();
        foreach (int h in new[] { hand, 1 - hand }) // this hand first
            foreach (var pt in layout.PartsFor(h).Where(x => x.IsUsable))
                foreach (var way in pt.Ways)
                    choices.Add(new Choice(ControllerLayouts.InputOf(h, pt, way), QuickBindings.NameOf(layout, h, pt, way)));
        var list = new ChoiceCombo(choices, 380) { SelectedIndex = 0 };
        if (!await Dialogs.Ask(this, "Swap with another button", $"{what}: which button should it trade places with? Each then does what the other one did in the game.", list))
            return;
        string target = list.Code;
        if (target == null || target == input)
            return; // swapping a button with itself does nothing
        string name = choices.First(c => c.Code == target).Text;
        var there = QuickBindings.GameButton(input, what, target, name);
        there.Name = $"{what} ⇄ {name}";
        var back = QuickBindings.GameButton(target, name, input, what);
        back.Name = $"{name} ⇄ {what}";
        profile.Bindings.Add(back);
        profile.Bindings.Add(there);
        Changed();
    }

    // ---- stick options: saved only while something is set, so untouched profiles stay as they were
    Control StickOptions()
    {
        var stick = (hand == 0 ? profile.Sticks?.Left : profile.Sticks?.Right) ?? new StickModel();
        var zones = new StickZonesView { Margin = new Thickness(24, 0, 0, 0) };
        void Redraw() => zones.ShowZones(stick.InnerDeadzone, stick.OuterDeadzone, stick.DirectionAngle, profile.Thresholds?.StickOn ?? 0.7f, stick.IgnoreOutside, stick.IgnoreCenter);
        void SaveStick()
        {
            var keep = stick.IsOff ? null : stick;
            profile.Sticks ??= new SticksModel();
            if (hand == 0) profile.Sticks.Left = keep; else profile.Sticks.Right = keep;
            if (profile.Sticks.Left == null && profile.Sticks.Right == null)
                profile.Sticks = null;
            Shell.Store.MarkDirty();
            changed?.Invoke();
        }
        NumericUpDown Number(decimal max, Func<float> get, Action<float> set)
        {
            decimal Shown() => Math.Clamp((decimal)Math.Round(get()), 0, max);
            var n = new NumericUpDown { Minimum = 0, Maximum = max, Increment = 1, Width = 130, FormatString = "0", Value = Shown() };
            n.ValueChanged += (s, e) =>
            {
                if (n.Value == null || n.Value == Shown()) return; // only a real change
                set((float)n.Value.Value);
                SaveStick();
                Redraw();
            };
            return n;
        }
        var f = new Form { LabelWidth = 200 };
        f.AddRow("Inner deadzone (%)", Number(90, () => stick.InnerDeadzone * 100, v => stick.InnerDeadzone = v / 100f),
            "What the game sees: pushes smaller than this count as none (stops drift). The rest is stretched, so the stick still reaches full at the edge. 10 to 15 is typical.");
        f.AddRow("Outer deadzone (%)", Number(90, () => stick.OuterDeadzone * 100, v => stick.OuterDeadzone = v / 100f),
            "What the game sees: the last part toward the edge already counts as fully pushed, for sticks that never quite reach the edge. 5 to 10 is typical.");
        f.AddRow("Direction zone (°)", Number(45, () => stick.DirectionAngle, v => stick.DirectionAngle = v),
            "A push within this many degrees of up, down, left or right counts as exactly that direction, for the game and for your bindings: up and a little right is plain up, so a jump on up doesn't also turn. Pushes in between (diagonals) are left as they are. 0 = off. 45 = every push snaps to the nearest of the four.");
        var ignore = new CheckBox { Content = new TextBlock { Text = "Ignore pushes outside the zones (only clean up / down / left / right)", TextWrapping = TextWrapping.Wrap, MaxWidth = 360 }, IsChecked = stick.IgnoreOutside };
        ignore.IsCheckedChanged += (s, e) =>
        {
            bool v = ignore.IsChecked == true;
            if (v == stick.IgnoreOutside) return; // only a real change
            stick.IgnoreOutside = v;
            SaveStick();
            Redraw();
        };
        f.AddRow("", ignore, "Only with a direction zone. On: pushes between the zones (diagonals) give the game nothing, so the stick only sends a clean up, down, left or right: pushing forward to jump never turns, however you lean. Off: diagonals pass through as they are.");
        f.AddRow("Game: ignore small pushes below (%)", Number(95, () => stick.IgnoreCenter * 100, v => stick.IgnoreCenter = v / 100f),
            "What the GAME gets, with 'Ignore pushes outside the zones' ticked: pushes smaller than this give the game nothing, so only firm, straight pushes reach it. 0 = off (the default).");
        var th = profile.Thresholds ??= new Thresholds();
        decimal PushShown() => (decimal)Math.Clamp(Math.Round(th.StickOn * 100), 30, 95);
        var pushAt = new NumericUpDown { Minimum = 30, Maximum = 95, Increment = 5, Width = 130, FormatString = "0", Value = PushShown() };
        pushAt.ValueChanged += (s, e) =>
        {
            if (pushAt.Value == null || pushAt.Value == PushShown()) return;
            th.StickOn = (float)pushAt.Value.Value / 100f;
            th.StickOff = Math.Max(0.1f, th.StickOn - 0.2f); // let go a little below it
            Shell.Store.MarkDirty();
            Redraw();
        };
        f.AddRow("Your bindings: direction counts at (%)", pushAt,
            "Only for bindings YOU added on this stick's Up / Down / Left / Right (for example 'push up = a key'): how far to push before they fire. The game's own stick controls are NOT affected by this; for those use 'Game: ignore small pushes below'. The same for both sticks. 70 is the default.");
        Redraw();
        return CardBox(new StackPanel { Spacing = 6, Children = { new TextBlock { Text = "Stick options (saved with the profile, 0 = off)", FontWeight = FontWeight.SemiBold },
            new StackPanel { Orientation = Orientation.Horizontal, Children = { f, zones } } } });
    }
}
