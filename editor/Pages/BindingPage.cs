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
    readonly NumericUpDown hold = Ms(), tap = Ms(), dbl = Ms();
    readonly NumericUpDown pressAt = new() { Minimum = 0, Maximum = 95, Increment = 5, Width = 130, FormatString = "0" };
    readonly NumericUpDown releaseAt = new() { Minimum = 0, Maximum = 90, Increment = 5, Width = 130, FormatString = "0" };
    readonly AutoCompleteBox layers = new() { Width = 380 }, notLayers = new() { Width = 380 };
    readonly Control lHold, lTap, lDbl, lPass, lPressAt, lReleaseAt;
    readonly Button fold = new() { Classes = { "link" }, FontWeight = FontWeight.Bold };
    readonly Border details;

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
        form.AddRow("Name", name);
        form.AddRow("", enabled);
        form.AddRow("The game also gets the button", block, "Set for you when you pick How: a plain press is taken from the game; hold and double-tap let a quick tap through; a touch always reaches the game.");
        lPass = form.AddRow("", passthrough);
        lHold = form.AddRow("Hold time (ms)", hold, "How long counts as a hold. 400 is the default.");
        lTap = form.AddRow("Tap shorter than (ms)", tap);
        lDbl = form.AddRow("Second tap within (ms)", dbl, "How quick the second tap (or flick) must be.");
        lPressAt = form.AddRow("Press point (%)", pressAt, "How far the trigger or grip must go. 0 = usual, low = hair trigger, high = a long pull. For an Index full squeeze: how hard.");
        lReleaseAt = form.AddRow("Keeps going until (%)", releaseAt, "Double flick on a stick: once it has started it keeps going while you steer, until you let the stick come back below this much from the middle. 30 = until you mostly let go. 0 = it stops as soon as the stick leaves the direction.");
        form.AddRow("Only while layer", layers, "Leave empty to work always. With a layer name, this binding only works while that layer is on.");
        form.AddRow("Not while layer", notLayers);
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
                Children = { sentence, ActionBar(), cards, fold, details },
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
        action.Bind(b.Action);
        loading = false;
        Sync();

        name.TextChanged += (s, e) => { if (Same(b.Name, name.Text)) return; b.Name = name.Text; Fire(); };
        enabled.IsCheckedChanged += (s, e) => { if (b.Enabled == (enabled.IsChecked == true)) return; b.Enabled = enabled.IsChecked == true; Fire(); };
        chord.Changed += () => { if (b.Input == chord.Value) return; b.Input = chord.Value; Sync(); Fire(); };
        gesture.SelectionChanged += (s, e) => { if (loading || gesture.Code == null || b.Gesture == gesture.Code) return; b.Gesture = gesture.Code; DefaultsForHow(); Sync(); Fire(); };
        hold.ValueChanged += (s, e) => { if (loading || hold.Value == null || b.HoldMs == (int)hold.Value) return; b.HoldMs = (int)hold.Value; Fire(); };
        tap.ValueChanged += (s, e) => { if (loading || tap.Value == null || b.TapMs == (int)tap.Value) return; b.TapMs = (int)tap.Value; Fire(); };
        dbl.ValueChanged += (s, e) => { if (loading || dbl.Value == null || b.DoubleTapMs == (int)dbl.Value) return; b.DoubleTapMs = (int)dbl.Value; Fire(); };
        pressAt.ValueChanged += (s, e) => { if (loading || pressAt.Value == null) return; double v = pressAt.Value <= 1 ? 0 : (double)pressAt.Value / 100; if (Math.Round(b.PressAt * 100) == Math.Round(v * 100)) return; b.PressAt = v; Fire(); };
        releaseAt.ValueChanged += (s, e) => { if (loading || releaseAt.Value == null) return; double v = releaseAt.Value < 5 ? 0 : (double)releaseAt.Value / 100; if (Math.Round(b.ReleaseAt * 100) == Math.Round(v * 100)) return; b.ReleaseAt = v; Fire(); };
        layers.TextChanged += (s, e) => { if (Same(b.Layers, layers.Text)) return; b.Layers = (layers.Text ?? "").Trim(); Fire(); };
        notLayers.TextChanged += (s, e) => { if (Same(b.NotLayers, notLayers.Text)) return; b.NotLayers = (notLayers.Text ?? "").Trim(); Fire(); };
        block.SelectionChanged += (s, e) => { if (loading || block.Code == null || b.Block == block.Code) return; b.Block = block.Code; Fire(); };
        passthrough.IsCheckedChanged += (s, e) => { if (loading || b.TapPassthrough == (passthrough.IsChecked == true)) return; b.TapPassthrough = passthrough.IsChecked == true; Fire(); };
        action.Changed += Fire;
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
        { b.Block = "trigger"; b.TapPassthrough = true; }
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
        GestureKind.Hold => "Hold it a moment (Details: how long). A quick tap can still reach the game.",
        GestureKind.DoubleTap => "Two quick presses. A single press still reaches the game.",
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
        sentence.Text = how + " " + InputText.Friendly(b.Input) + "   →   " + EditorApi.Plain(b.Action) + (b.Enabled ? "" : "   (off)");
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
        Form.Show(lPass, b.Gesture == GestureKind.Hold);
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
