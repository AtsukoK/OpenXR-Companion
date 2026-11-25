using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// One controller binding: 1 How, 2 Button, 3 What happens, then Details (foldable). Port of the WinForms BindingEditor.
public class BindingPage : UserControl
{
    readonly BindingModel b;
    bool loading;
    static bool detailsOpen = true; // kept between bindings while the editor runs

    static readonly Choice[] Hows =
    {
        new(GestureKind.Press, "Press it"), new(GestureKind.Hold, "Hold it"), new(GestureKind.DoubleTap, "Double-tap it"),
        new(GestureKind.DoubleTapHold, "Double-tap and hold  (stick: double flick)"),
        new(GestureKind.Tap, "More: quick tap only"), new(GestureKind.WhileHeld, "More: while held (on until you let go)"),
    };

    readonly TextBlock sentence = new() { FontSize = 22, FontWeight = FontWeight.SemiBold, TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 0, 0, 10) };
    readonly TextBlock howHint = new() { Classes = { "desc" }, MaxWidth = 380 };
    readonly ChoiceCombo gesture = new(Hows, 380);
    readonly ChordPicker chord = new();
    readonly ActionPanel action = new("binding");
    readonly TextBox name = new() { Width = 380 };
    readonly CheckBox enabled = new() { Content = "Enabled" };
    readonly ChoiceCombo block = new(Choices.Blocks, 380);
    readonly CheckBox passthrough = new() { Content = "If I let go early, the game still gets a normal tap" };
    readonly CheckBox tapOwn = new() { Content = "On" };
    // what the chosen "game also gets the button" means for this binding's button, in words
    readonly TextBlock blockSays = new() { TextWrapping = TextWrapping.Wrap, MaxWidth = 380, FontSize = 12, Foreground = Brush.Parse("#C8CDD6") };
    readonly ActionPanel tapAction = new("wheel"); // one quick press: nothing stays held after a short press
    readonly NumericUpDown hold = Ms(), tap = Ms(), dbl = Ms();
    readonly NumericUpDown pressAt = new() { Minimum = 0, Maximum = 95, Increment = 5, Width = 130, FormatString = "0" };
    readonly NumericUpDown releaseAt = new() { Minimum = 0, Maximum = 90, Increment = 5, Width = 130, FormatString = "0" };
    readonly AutoCompleteBox layers = new() { Width = 380 }, notLayers = new() { Width = 380 };
    readonly Control lHold, lTap, lDbl, lPass, lTapOwn, lTapAction, lPressAt, lReleaseAt, lBlock;
    readonly Button fold = new() { Classes = { "link" }, FontWeight = FontWeight.Bold };
    readonly Border details;
    // other bindings on the same button that undo what this one says (e.g. a Hold letting short presses through)
    readonly StackPanel warnings = new() { Spacing = 6, Margin = new Thickness(0, 0, 0, 10) };

    static NumericUpDown Ms() => new() { Minimum = 50, Maximum = 5000, Increment = 50, Width = 130, FormatString = "0" };

    // Duplicate (a copy right after it, switched off: two of the same would both fire) and Delete, like WinForms.
    Control ActionBar()
    {
        var p = Shell.Store.Profile;
        var dup = new Button { Content = "Duplicate binding", Classes = { "pill" } };
        var del = new Button { Content = "Delete binding", Classes = { "pill", "danger" } };
        dup.Click += (s, e) =>
        {
            var c = ItemLists.Clone(b);
            c.Name += " copy";
            c.Enabled = false;
            p.Bindings.Insert(p.Bindings.IndexOf(b) + 1, c);
            Shell.Store.MarkDirty();
            Shell.Go("binding:" + p.Bindings.IndexOf(c));
            Shell.SetStatus("Made a copy (switched off, so the two don't both fire). Switch it on under Details.");
        };
        del.Click += async (s, e) =>
        {
            if (!await Dialogs.Confirm(this, "Delete binding", $"Delete '{b.Name}'?", "Delete")) return;
            p.Bindings.Remove(b);
            Shell.Store.MarkDirty();
            Shell.Go("bindings");
            Shell.SetStatus("Deleted '" + b.Name + "'.");
        };
        return new StackPanel { Orientation = Avalonia.Layout.Orientation.Horizontal, Spacing = 8, Margin = new Thickness(0, 0, 0, 10), Children = { dup, del } };
    }

    public BindingPage(BindingModel model)
    {
        b = model;
        b.Action ??= new ActionModel();

        var howPanel = new StackPanel { Spacing = 6, Children = { gesture, howHint } };
        var form = new Form();
        form.AddRow("Name", name, null, "Shown in lists here and in the headset.");
        form.AddRow("Switched on", enabled, null, "Off = kept in the profile but does nothing.");
        lBlock = form.AddRow("Game also gets the button", new StackPanel { Spacing = 4, Children = { block, blockSays } },
                             "Set for you when you pick How. Change it if the game reacting to the same button gets in the way.",
                             "Whether the game still sees this button when you use it for this binding.");
        lPass = form.AddRow("Short press to the game", passthrough, null, "Let go before the hold time: the game gets a normal tap of the button.");
        lTapOwn = form.AddRow("Short press", tapOwn,
                              "Hold: let go before the hold time and this happens instead (a key, another button...); the hold still does the action above.\n" +
                              "Double-tap: a single tap does this, a moment late: it waits out 'Second tap within' to see if a second tap comes.",
                              "");
        lTapAction = form.AddRow("Short press does", tapAction);
        lHold = form.AddRow("Hold time (ms)", hold, "400 is the default.", "How long you hold before it counts as a hold.");
        lTap = form.AddRow("Tap shorter than (ms)", tap, null, "A press shorter than this counts as a quick tap.");
        lDbl = form.AddRow("Second tap within (ms)", dbl, "350 is the default.", "How quick the second tap (or flick) must come. A single tap waits this long too.");
        lPressAt = form.AddRow("Press point (%)", pressAt, "For an Index full squeeze: how hard.", "How far the trigger or grip must go. 0 = usual, low = hair trigger, high = a long pull.");
        lReleaseAt = form.AddRow("Keeps going until (%)", releaseAt, "30 = until you mostly let go. 0 = it stops as soon as the stick leaves the direction.",
                                 "Double flick: keeps going while you steer, until the stick comes back below this much from the middle.");
        form.AddRow("Only while layer", layers, "Layers are switched on by other bindings (What happens: Switch a layer).", "Works only while this layer is on. Empty = always.");
        form.AddRow("Not while layer", notLayers, null, "Does nothing while this layer is on. Empty = never off.");
        details = Cards.Make("Details", form);
        details.IsVisible = detailsOpen;
        fold.Click += (s, e) => { detailsOpen = !detailsOpen; details.IsVisible = detailsOpen; ShowFold(); };

        var cards = new WrapPanel
        {
            Children = { Cards.Make("1   How", howPanel), Cards.Make("2   Button", chord), Cards.Make("3   What happens", action) },
        };
        Content = new ScrollViewer
        {
            Content = new StackPanel
            {
                Margin = new Thickness(16, 12), MaxWidth = 1500,
                Children = { sentence, warnings, ActionBar(), cards, fold, details },
            },
        };

        // fill in, then listen
        loading = true;
        name.Text = b.Name;
        enabled.IsChecked = b.Enabled;
        chord.Value = b.Input;
        gesture.Code = b.Gesture;
        hold.Value = Math.Clamp(b.HoldMs, 50, 5000);
        tap.Value = Math.Clamp(b.TapMs, 50, 5000);
        dbl.Value = Math.Clamp(b.DoubleTapMs, 50, 5000);
        var layerNames = EditorApi.LayerNames(Shell.Store.Profile).ToList();
        layers.ItemsSource = layerNames;
        notLayers.ItemsSource = layerNames;
        layers.Text = b.Layers ?? "";
        notLayers.Text = b.NotLayers ?? "";
        block.Code = b.Block;
        passthrough.IsChecked = b.TapPassthrough;
        pressAt.Value = (decimal)Math.Clamp(Math.Round(b.PressAt * 100), 0, 95);
        releaseAt.Value = (decimal)Math.Clamp(Math.Round(b.ReleaseAt * 100), 0, 90);
        action.OwnInput = tapAction.OwnInput = () => b.Input;
        action.Bind(b.Action);
        tapOwn.IsChecked = b.TapAction != null;
        tapAction.Bind(b.TapAction ?? new ActionModel());
        loading = false;
        Sync();

        name.TextChanged += (s, e) => { if (Same(b.Name, name.Text)) return; b.Name = name.Text; Fire(); };
        enabled.IsCheckedChanged += (s, e) => { if (b.Enabled == (enabled.IsChecked == true)) return; b.Enabled = enabled.IsChecked == true; ShowWarnings(); Fire(); };
        chord.Changed += () => { if (b.Input == chord.Value) return; b.Input = chord.Value; Sync(); Fire(); };
        gesture.SelectionChanged += (s, e) => { if (loading || gesture.Code == null || b.Gesture == gesture.Code) return; b.Gesture = gesture.Code; DefaultsForHow(); Sync(); Fire(); };
        hold.ValueChanged += (s, e) => { if (loading || hold.Value == null || b.HoldMs == (int)hold.Value) return; b.HoldMs = (int)hold.Value; Fire(); };
        tap.ValueChanged += (s, e) => { if (loading || tap.Value == null || b.TapMs == (int)tap.Value) return; b.TapMs = (int)tap.Value; Fire(); };
        dbl.ValueChanged += (s, e) => { if (loading || dbl.Value == null || b.DoubleTapMs == (int)dbl.Value) return; b.DoubleTapMs = (int)dbl.Value; Fire(); };
        pressAt.ValueChanged += (s, e) => { if (loading || pressAt.Value == null) return; double v = pressAt.Value <= 1 ? 0 : (double)pressAt.Value / 100; if (Math.Round(b.PressAt * 100) == Math.Round(v * 100)) return; b.PressAt = v; Fire(); };
        releaseAt.ValueChanged += (s, e) => { if (loading || releaseAt.Value == null) return; double v = releaseAt.Value < 5 ? 0 : (double)releaseAt.Value / 100; if (Math.Round(b.ReleaseAt * 100) == Math.Round(v * 100)) return; b.ReleaseAt = v; Fire(); };
        layers.TextChanged += (s, e) => { if (Same(b.Layers, layers.Text)) return; b.Layers = (layers.Text ?? "").Trim(); Fire(); };
        notLayers.TextChanged += (s, e) => { if (Same(b.NotLayers, notLayers.Text)) return; b.NotLayers = (notLayers.Text ?? "").Trim(); Fire(); };
        block.SelectionChanged += (s, e) => { if (loading || block.Code == null || b.Block == block.Code) return; b.Block = block.Code; Sync(); Fire(); };
        passthrough.IsCheckedChanged += (s, e) => { if (loading || b.TapPassthrough == (passthrough.IsChecked == true)) return; b.TapPassthrough = passthrough.IsChecked == true; ShowWarnings(); Fire(); };
        action.Changed += Fire;
        tapOwn.IsCheckedChanged += (s, e) =>
        {
            if (loading || (b.TapAction != null) == (tapOwn.IsChecked == true)) return;
            if (tapOwn.IsChecked == true)
            {
                b.TapAction = new ActionModel();
                tapAction.Bind(b.TapAction);
                b.TapPassthrough = false; // the short press is taken by this now
                b.Block = "trigger";
                loading = true; passthrough.IsChecked = false; block.Code = b.Block; loading = false;
            }
            else
                b.TapAction = null;
            Sync();
            Fire();
        };
        tapAction.Changed += Fire;
    }

    // Plain single-button bindings on the same button as this one (combos only count while their other buttons are held).
    IEnumerable<BindingModel> SameButton()
    {
        var parts = (b.Input ?? "").Split('+', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length == 0) return Array.Empty<BindingModel>();
        var mine = InputText.Split(parts[^1]);
        return Shell.Store.Profile.Bindings.Where(o => !ReferenceEquals(o, b) && !(o.Input ?? "").Contains('+')
                                                       && !string.IsNullOrWhiteSpace(o.Input) && InputText.Split(o.Input) == mine);
    }

    // Bindings on the same button that disagree about the game getting it, with a one-click fix each.
    void ShowWarnings()
    {
        warnings.Children.Clear();
        if (!b.Enabled) return;
        string btn = InputText.Friendly((b.Input ?? "").Split('+').Last());
        bool keeps = (b.Block ?? "none") != "none";
        bool leaks = b.Gesture == GestureKind.Hold && b.TapPassthrough && b.TapAction == null;
        foreach (var o in SameButton().Where(o => o.Enabled))
        {
            bool oLeaks = o.Gesture == GestureKind.Hold && o.TapPassthrough && o.TapAction == null;
            bool oKeeps = (o.Block ?? "none") != "none";
            if (keeps && oLeaks)
                AddWarning($"'{o.Name}' (Hold) on {btn} sends short presses to the game, so single presses still reach it even though this binding keeps the button from the game.",
                           "Stop that binding's short presses", () => o.TapPassthrough = false);
            else if (leaks && oKeeps)
                AddWarning($"This binding sends short presses of {btn} to the game, but '{o.Name}' keeps the button from the game. Short presses will still reach it.",
                           "Stop this binding's short presses", () => { b.TapPassthrough = false; loading = true; passthrough.IsChecked = false; loading = false; });
        }
    }

    void AddWarning(string text, string fix, Action apply)
    {
        var button = new Button { Content = fix, Classes = { "pill" } };
        button.Click += (s, e) => { apply(); Shell.Store.MarkDirty(); Sync(); Shell.SetStatus("Fixed: " + fix.ToLowerInvariant() + "."); };
        warnings.Children.Add(new Border
        {
            Background = Brush.Parse("#3A3220"), BorderBrush = Brush.Parse("#F2C94C"), BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(6), Padding = new Thickness(12, 8), MaxWidth = 1100, HorizontalAlignment = HorizontalAlignment.Left,
            Child = new StackPanel
            {
                Orientation = Orientation.Horizontal, Spacing = 12,
                Children =
                {
                    new TextBlock { Text = "⚠  " + text, TextWrapping = TextWrapping.Wrap, MaxWidth = 820, VerticalAlignment = VerticalAlignment.Center, Foreground = Brush.Parse("#F7E3A1") },
                    button,
                },
            },
        });
    }

    // A double-tap's single tap with nothing of its own: the game gets it, unless the button is kept from the game.
    string SingleTapOff() => (b.Block ?? "none") == "none" ? "goes to the game as usual" : "does nothing";

    // The chosen "game also gets the button", for this binding's button and How, in words.
    string BlockSays()
    {
        string btn = InputText.Friendly((b.Input ?? "").Split('+').Last());
        bool dbl = b.Gesture == GestureKind.DoubleTap, hold = b.Gesture == GestureKind.Hold;
        return (b.Block ?? "none") switch
        {
            "none" => $"The game sees every press of {btn} as usual, and this binding runs as well.",
            "all" => $"The game doesn't see {btn}, or the combo's other buttons while you hold them for it. Only what this binding sends reaches the game.",
            _ => dbl ? $"The game doesn't see {btn} at all: a single tap does nothing in the game, a double tap only does what's under What happens."
                 : hold ? $"The game doesn't see {btn}: a hold only does what's under What happens (a short press: see Short press below)."
                 : $"The game doesn't see {btn}: only what this binding sends reaches the game.",
        };
    }

    // a field showing what the model already has (empty and null count as the same) is no change
    static bool Same(string model, string shown) => (model ?? "").Trim() == (shown ?? "").Trim();

    void Fire()
    {
        if (loading)
            return;
        Shell.Store.MarkDirty();
        ShowSentence();
        if (!detailsOpen)
            ShowFold();
    }

    // the sensible 'does the game still get it' for a How (Details can change it again)
    void DefaultsForHow()
    {
        string input = (b.Input ?? "").Split('+')[^1].Trim();
        bool touch = input.EndsWith(".touch") || input.Contains(".thumbrest");
        b.TapPassthrough = false;
        if (touch || b.Gesture is GestureKind.DoubleTap or GestureKind.DoubleTapHold or GestureKind.Tap)
            b.Block = "none";
        else if (b.Gesture == GestureKind.Hold)
        {
            b.Block = "trigger";
            // short presses go to the game, unless another binding on this button keeps it from the game
            b.TapPassthrough = !SameButton().Any(o => o.Enabled && (o.Block ?? "none") != "none");
        }
        else
            b.Block = "trigger";
        bool was = loading;
        loading = true;
        block.Code = b.Block;
        passthrough.IsChecked = b.TapPassthrough;
        loading = was;
    }

    static string HowHint(string g) => g switch
    {
        GestureKind.Press => "Starts the moment you press it.",
        GestureKind.Hold => "Hold it a moment (Details: how long, and whether a quick tap still reaches the game).",
        GestureKind.DoubleTap => "Two quick presses. Details: whether a single press still reaches the game, or does something of its own.",
        GestureKind.DoubleTapHold => "Press twice and keep it down the second time; it lasts until you let go. On a stick direction: a double flick (sprint).",
        GestureKind.Tap => "Only a short press counts, so a Hold on the same button can do something else.",
        GestureKind.WhileHeld => "On while you hold it, off when you let go.",
        _ => "",
    };

    void ShowSentence()
    {
        string how = b.Gesture switch
        {
            GestureKind.DoubleTap => "Double-tap", GestureKind.Hold or GestureKind.WhileHeld => "Hold", GestureKind.Tap => "Quick tap",
            GestureKind.DoubleTapHold => "Double-tap and hold", _ => "Press",
        };
        sentence.Text = how + " " + InputText.Friendly(b.Input) + "   →   " + EditorApi.Plain(b.Action)
            + (b.Gesture == GestureKind.Hold && b.TapAction != null ? "     ·     short press   →   " + EditorApi.Plain(b.TapAction) : "")
            + (b.Gesture == GestureKind.DoubleTap ? "     ·     single tap   →   " + (b.TapAction != null ? EditorApi.Plain(b.TapAction) : SingleTapOff()) : "")
            + (b.Enabled ? "" : "   (off)");
        sentence.Foreground = b.Enabled ? Brushes.White : Brush.Parse("#7F8896");
    }

    string DetailsSummary()
    {
        var parts = new List<string> { b.Block == "none" ? "the game also gets the button" : "the game doesn't get the button" };
        if (b.Gesture == GestureKind.Hold) parts.Add($"hold {b.HoldMs} ms");
        if (b.Gesture is GestureKind.DoubleTap or GestureKind.DoubleTapHold) parts.Add($"second tap within {b.DoubleTapMs} ms");
        parts.Add(string.IsNullOrWhiteSpace(b.Layers) ? "always" : "only in layer " + b.Layers);
        if (!b.Enabled) parts.Add("off");
        return string.Join("  ·  ", parts);
    }

    void ShowFold() => fold.Content = detailsOpen ? "Details ▾   (all set for you: change any)" : "Details ▸   " + DetailsSummary();

    void Sync()
    {
        howHint.Text = HowHint(b.Gesture);
        ShowFold();
        ShowSentence();
        Form.Show(lHold, b.Gesture == GestureKind.Hold);
        Form.Show(lPass, b.Gesture == GestureKind.Hold && b.TapAction == null);
        bool dbl = b.Gesture == GestureKind.DoubleTap;
        tapOwn.Content = b.TapAction != null ? "On" : "Off  (tick to set one)";
        Form.SetLabel(lTapOwn, dbl ? "Single tap" : "Short press action",
                      dbl ? "One tap with no second tap in time. Off by default (then it " + SingleTapOff() + "). When on, it comes a moment late."
                          : "Its own action when you let go before the hold time (instead of the tap above). Off: nothing of its own.");
        Form.SetLabel(lTapAction, dbl ? "Single tap does" : "Short press does");
        blockSays.Text = BlockSays();
        ShowWarnings();
        Form.Show(lTapOwn, b.Gesture == GestureKind.Hold || dbl);
        Form.Show(lTapAction, (b.Gesture == GestureKind.Hold || dbl) && b.TapAction != null);
        Form.Show(lTap, b.Gesture == GestureKind.Tap);
        Form.Show(lDbl, b.Gesture is GestureKind.DoubleTap or GestureKind.DoubleTapHold);
        Form.Show(lPressAt, IsAnalog(b.Input));
        Form.Show(lReleaseAt, b.Gesture == GestureKind.DoubleTapHold && IsDirection(b.Input));
    }

    static bool IsDirection(string input)
    {
        var last = (input ?? "").Split('+')[^1].Trim().ToLowerInvariant();
        return last.EndsWith(".up") || last.EndsWith(".down") || last.EndsWith(".left") || last.EndsWith(".right");
    }

    static bool IsAnalog(string input)
    {
        var last = (input ?? "").Split('+')[^1].Trim().ToLowerInvariant();
        var parts = last.Split('.');
        if (parts.Length < 2) return false;
        string comp = parts[1], way = parts.Length > 2 ? parts[2] : "";
        return (comp is "trigger" or "grip" && way is "" or "full") || (comp == "trackpad" && way == "");
    }
}
