using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Modes (previously called layers): what each mode changes, how you turn it on, its colour, and whether it may be on
// together with other modes. Underneath they are the profile's layers: a binding "only in mode" is a binding with
// layers = that name, and a wrist button / wheel item / controller button turns it on with a layer action.
public class ModesPage : UserControl
{
    readonly ProfileModel p = Shell.Store.Profile;
    static readonly string[] Palette = { "#E0583AFF", "#3A8EE0FF", "#3FB36BFF", "#C9A227FF", "#9B59D0FF", "#E06AA8FF" };

    public ModesPage() => Fill();

    void Fill()
    {
        var body = new StackPanel { Margin = new Thickness(16, 12), Spacing = 10, MaxWidth = 1300, HorizontalAlignment = HorizontalAlignment.Left };
        body.Children.Add(new TextBlock { Text = "Modes", FontSize = 24, FontWeight = FontWeight.SemiBold });
        body.Children.Add(new TextBlock
        {
            Foreground = Brush.Parse("#C8CDD6"), TextWrapping = TextWrapping.Wrap, MaxWidth = 900, HorizontalAlignment = HorizontalAlignment.Left,
            Text = "A mode changes what some buttons do while it's on. For example Combat mode: Y presses K instead of answering prompts. " +
                   "Turn a mode on with a wrist panel button, a wheel item or a controller button. Turning one on turns the others off, " +
                   "so only one mode is on at a time. In the game a small badge on your wrist panels and wheel shows which one is on.",
        });
        body.Children.Add(new Border
        {
            Background = Brush.Parse("#26324A"), CornerRadius = new CornerRadius(4), Padding = new Thickness(10, 6), HorizontalAlignment = HorizontalAlignment.Left,
            Child = new TextBlock { Text = "ⓘ  Modes were previously called layers. Old profiles keep working: their layers show up here as modes.", Foreground = Brush.Parse("#CFE0FF") },
        });
        var add = new Button { Content = "+ New mode", Classes = { "go" }, HorizontalAlignment = HorizontalAlignment.Left };
        add.Click += async (s, e) =>
        {
            var name = new TextBox { Width = 300, Watermark = "e.g. combat, looting, driving" };
            if (!await Dialogs.Ask(this, "New mode", "What's the mode called?", name)) return;
            string n = (name.Text ?? "").Trim().ToLowerInvariant().Replace(",", "");
            if (n.Length == 0) return;
            p.Modes ??= new();
            if (!p.Modes.ContainsKey(n))
                p.Modes[n] = new ModeModel { Color = Palette[p.Modes.Count % Palette.Length] };
            Shell.Store.MarkDirty();
            Fill();
        };
        body.Children.Add(add);

        var names = EditorApi.LayerNames(p).Select(x => x.ToLowerInvariant()).Concat(p.Modes?.Keys ?? Enumerable.Empty<string>())
                                           .Distinct().OrderBy(x => x).ToList();
        if (names.Count == 0)
            body.Children.Add(new TextBlock { Text = "No modes yet. '+ New mode' makes one.", Foreground = Brush.Parse("#AAB2BF") });
        foreach (var n in names)
            body.Children.Add(ModeCard(n));
        Content = new ScrollViewer { Content = body };
    }

    static bool Has(string list, string mode) =>
        (list ?? "").Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).Any(l => string.Equals(l, mode, StringComparison.OrdinalIgnoreCase));
    static bool Turns(ActionModel a, string mode) => a?.Type == ActionType.Layer && string.Equals(a.Layer, mode, StringComparison.OrdinalIgnoreCase);

    ModeModel Info(string mode)
    {
        p.Modes ??= new();
        if (!p.Modes.TryGetValue(mode, out var m))
            p.Modes[mode] = m = new ModeModel { Color = Palette[Math.Abs(mode.GetHashCode()) % Palette.Length] };
        return m;
    }

    Control ModeCard(string mode)
    {
        var info = p.Modes != null && p.Modes.TryGetValue(mode, out var mi) ? mi : null;
        string colour = info?.Color ?? Palette[Math.Abs(mode.GetHashCode()) % Palette.Length];
        var card = new StackPanel { Spacing = 8 };

        // heading: colour, name, the badge as the game shows it, together
        var swatch = new Border { Width = 26, Height = 26, CornerRadius = new CornerRadius(13), BorderBrush = Brushes.White, BorderThickness = new Thickness(1),
                                  Background = WheelView.TryColor(colour, out var c) ? new SolidColorBrush(c) : Brushes.Orange };
        var pick = ColourButton.Wrap(swatch, () => colour, v => { Info(mode).Color = v; colour = v; swatch.Background = WheelView.TryColor(v, out var cc) ? new SolidColorBrush(cc) : swatch.Background; Shell.Store.MarkDirty(); });
        ToolTip.SetTip(pick, "The badge's colour in the game");
        var together = new CheckBox { Content = "Can be on together with other modes", IsChecked = info?.Together == true };
        ToolTip.SetTip(together, "Off (usual): turning this mode on turns the other modes off. On: it stays on alongside them.");
        together.IsCheckedChanged += (s, e) => { Info(mode).Together = together.IsChecked == true; Shell.Store.MarkDirty(); };
        card.Children.Add(new StackPanel
        {
            Orientation = Orientation.Horizontal, Spacing = 12,
            Children =
            {
                pick,
                new TextBlock { Text = char.ToUpperInvariant(mode[0]) + mode[1..] + " mode", FontSize = 20, FontWeight = FontWeight.SemiBold, VerticalAlignment = VerticalAlignment.Center },
                new Border { Background = swatch.Background, CornerRadius = new CornerRadius(10), Padding = new Thickness(10, 2), VerticalAlignment = VerticalAlignment.Center,
                             Child = new TextBlock { Text = "● " + mode.ToUpperInvariant(), FontWeight = FontWeight.Bold, FontSize = 12, Foreground = Brush.Parse("#101418") } },
                together,
            },
        });

        // how it's turned on
        var by = new List<(string text, string go)>();
        for (int i = 0; i < p.Bindings.Count; i++)
        {
            var b = p.Bindings[i];
            if (Turns(b.Action, mode) || Turns(b.TapAction, mode))
                by.Add(($"Controller: {AllBindings.GestureWord(b.Gesture)} {InputText.Friendly(b.Input)}{(b.Enabled ? "" : " (off)")}", "binding:" + i));
        }
        foreach (var w in p.Wheels.Values)
            foreach (var it in w.Items.Where(it => Turns(it.Action, mode)))
                by.Add(($"Wheel '{w.Title ?? w.Name}': {it.Label}", "wheel:" + w.Name));
        for (int h = 0; h < 2; h++)
            foreach (var w in p.WristFor(h).Pages.Values)
                foreach (var it in w.Items.Where(it => Turns(it.Action, mode)))
                    by.Add(($"{(h == 0 ? "Left" : "Right")} wrist panel: {it.Label}", "wrist" + h));
        card.Children.Add(new TextBlock { Text = "TURNED ON BY", FontWeight = FontWeight.Bold, FontSize = 12, Foreground = Brush.Parse("#7FA8F0") });
        if (by.Count == 0)
            card.Children.Add(new TextBlock { Text = "⚠ Nothing turns it on yet. Add a button for it below.", Foreground = Brush.Parse("#F2C94C") });
        foreach (var (text, go) in by)
        {
            var link = new Button { Content = text, Classes = { "link" } };
            link.Click += (s, e) => Shell.Go(go);
            card.Children.Add(link);
        }
        var addWrist = new Button { Content = "+ Button on a wrist panel", Classes = { "pill" } };
        addWrist.Click += async (s, e) =>
        {
            int h = await Dialogs.Choose(this, "Which wrist panel?", $"A '{Cap(mode)}' button is added to the first page of that panel. Pressing it turns {mode} mode on or off.", "Left", "Right");
            if (h < 0 || h > 1) return;
            var pages = p.WristPages(h).Pages;
            if (pages.Count == 0) return;
            pages.Values.First().Items.Add(new WheelItemModel { Label = Cap(mode), Color = colour, Action = new ActionModel { Type = ActionType.Layer, Layer = mode, Mode = "toggle" } });
            Shell.Store.MarkDirty();
            Shell.SetStatus($"Added '{Cap(mode)}' to the {(h == 0 ? "left" : "right")} wrist panel.");
            Fill();
        };
        var addWheel = new Button { Content = "+ Item on a wheel", Classes = { "pill" }, IsEnabled = p.Wheels.Count > 0 };
        addWheel.Click += async (s, e) =>
        {
            var pickWheel = new ComboBox { Width = 300, ItemsSource = p.Wheels.Keys.ToList(), SelectedIndex = 0 };
            if (!await Dialogs.Ask(this, "Which wheel?", $"A '{Cap(mode)}' item is added to that wheel. Picking it turns {mode} mode on or off.", pickWheel)) return;
            if (pickWheel.SelectedItem is not string wn) return;
            p.Wheels[wn].Items.Add(new WheelItemModel { Label = Cap(mode), Color = colour, Action = new ActionModel { Type = ActionType.Layer, Layer = mode, Mode = "toggle" } });
            Shell.Store.MarkDirty();
            Shell.SetStatus($"Added '{Cap(mode)}' to wheel '{wn}'.");
            Fill();
        };
        var addButton = new Button { Content = "+ Controller button", Classes = { "pill" } };
        addButton.Click += async (s, e) =>
        {
            var r = await ControllerButtonPicker.Show(this);
            if (r == null) return;
            var b = new BindingModel
            {
                Name = $"{Cap(mode)} mode on/off", Input = InputText.Join(r.Value.hand, r.Value.button), Gesture = GestureKind.DoubleTap, Block = "trigger",
                Action = new ActionModel { Type = ActionType.Layer, Layer = mode, Mode = "toggle" },
            };
            p.Bindings.Add(b);
            Shell.Store.MarkDirty();
            Shell.Go("binding:" + p.Bindings.IndexOf(b));
        };
        card.Children.Add(new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Children = { addWrist, addWheel, addButton } });

        // what changes
        card.Children.Add(new TextBlock { Text = $"WHAT CHANGES IN {mode.ToUpperInvariant()} MODE", FontWeight = FontWeight.Bold, FontSize = 12, Foreground = Brush.Parse("#7FA8F0"), Margin = new Thickness(0, 6, 0, 0) });
        var grid = new Grid { ColumnDefinitions = new ColumnDefinitions("260,320,320,Auto"), RowDefinitions = new RowDefinitions("Auto") };
        void Cell(int row, int col, Control ctl) { Grid.SetRow(ctl, row); Grid.SetColumn(ctl, col); grid.Children.Add(ctl); }
        TextBlock Head(string t) => new() { Text = t, FontWeight = FontWeight.SemiBold, Foreground = Brush.Parse("#AAB2BF"), Margin = new Thickness(0, 0, 12, 4) };
        Cell(0, 0, Head("Button"));
        Cell(0, 1, Head("Normally"));
        Cell(0, 2, Head($"In {mode} mode"));
        int rowN = 1;
        for (int i = 0; i < p.Bindings.Count; i++)
        {
            var b = p.Bindings[i];
            if (!Has(b.Layers, mode)) continue;
            var normal = p.Bindings.FirstOrDefault(o => !ReferenceEquals(o, b) && o.Enabled && string.IsNullOrWhiteSpace(o.Layers) && o.Input == b.Input && o.Gesture == b.Gesture);
            grid.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
            Cell(rowN, 0, new TextBlock { Text = $"{AllBindings.GestureWord(b.Gesture)} {InputText.Friendly(b.Input)}", Margin = new Thickness(0, 4, 12, 4), TextWrapping = TextWrapping.Wrap });
            Cell(rowN, 1, new TextBlock { Text = normal != null ? EditorApi.Plain(normal.Action) : "What the game itself does", Foreground = Brush.Parse("#AAB2BF"), Margin = new Thickness(0, 4, 12, 4), TextWrapping = TextWrapping.Wrap });
            Cell(rowN, 2, new TextBlock { Text = EditorApi.Plain(b.Action) + (b.Enabled ? "" : "  (switched off)"), FontWeight = FontWeight.SemiBold, Margin = new Thickness(0, 4, 12, 4), TextWrapping = TextWrapping.Wrap });
            int index = i;
            var open = new Button { Content = "Change…", Classes = { "pill" } };
            open.Click += (s, e) => Shell.Go("binding:" + index);
            Cell(rowN, 3, open);
            rowN++;
        }
        if (rowN == 1)
        {
            grid.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
            Cell(1, 0, new TextBlock { Text = "No buttons change yet.", Foreground = Brush.Parse("#AAB2BF") });
        }
        card.Children.Add(grid);
        var change = new Button { Content = $"+ Change a button in {mode} mode", Classes = { "go" }, HorizontalAlignment = HorizontalAlignment.Left };
        change.Click += async (s, e) =>
        {
            var r = await ControllerButtonPicker.Show(this);
            if (r == null) return;
            string input = InputText.Join(r.Value.hand, r.Value.button);
            var normal = p.Bindings.FirstOrDefault(o => o.Enabled && string.IsNullOrWhiteSpace(o.Layers) && InputText.Friendly(o.Input) == InputText.Friendly(input));
            BindingModel b;
            if (normal != null)
            { // the same button and How as now, so it takes over in the mode
                b = ItemLists.Clone(normal);
                b.Name = $"{normal.Name} ({mode} mode)";
                b.Layers = mode;
            }
            else
                b = new BindingModel { Name = $"{InputText.Friendly(input)} in {mode} mode", Input = input, Gesture = GestureKind.Press, Layers = mode,
                                       Action = new ActionModel { Type = ActionType.Key, Keys = "F1" } };
            p.Bindings.Add(b);
            Shell.Store.MarkDirty();
            Shell.SetStatus($"Now set what it does in {mode} mode (What happens).");
            Shell.Go("binding:" + p.Bindings.IndexOf(b));
        };
        card.Children.Add(change);
        return Cards.Make("", card);
    }

    static string Cap(string s) => s.Length == 0 ? s : char.ToUpperInvariant(s[0]) + s[1..];
}
