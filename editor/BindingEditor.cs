using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

// ------------------------------------------------------------ binding editor
public class BindingEditor : EditorBase
{
    readonly BindingModel b;
    readonly TextBox name = new() { Width = 360 };
    readonly CheckBox enabled = new() { Text = "Enabled", AutoSize = true };
    readonly ChordEditor chord = new();
    // the common ways first, the rare ones last
    static readonly Choice[] Hows =
    {
        new(GestureKind.Press, "Press it"), new(GestureKind.Hold, "Hold it"), new(GestureKind.DoubleTap, "Double-tap it"),
        new(GestureKind.DoubleTapHold, "Double-tap and hold  (stick: double flick)"),
        new(GestureKind.Tap, "More: quick tap only"), new(GestureKind.WhileHeld, "More: while held (on until you let go)"),
    };
    readonly ChoiceBox gesture = new(Hows, 360);
    readonly NumericUpDown hold = Ms(), tap = Ms(), dbl = Ms();
    readonly ComboBox layers = new() { Width = 220 }, notLayers = new() { Width = 220 };
    readonly ChoiceBox block = new(Choices.Blocks, 360);
    readonly CheckBox passthrough = new() { Text = "If I let go early, the game still gets a normal tap", AutoSize = true };
    readonly ActionEditor action = new(false);
    readonly NumericUpDown pressAt = new() { Minimum = 0, Maximum = 95, Increment = 5, Width = 90 };
    readonly NumericUpDown releaseAt = new() { Minimum = 0, Maximum = 90, Increment = 5, Width = 90 };
    Label lReleaseAt;
    readonly Label lHold, lTap, lDbl, lPass, lPressAt;

    readonly Label sentence = new();
    readonly Label howHint = new();
    static bool detailsOpen = true; // kept between bindings while the editor runs
    Action ShowFoldNow;

    // the folded details in one line: "the game also gets the button . hold 400 ms . always"
    string DetailsSummary()
    {
        var parts = new List<string> { b.Block == "none" ? "the game also gets the button" : "the game doesn't get the button" };
        if (b.Gesture == GestureKind.Hold) parts.Add($"hold {b.HoldMs} ms");
        if (b.Gesture is GestureKind.DoubleTap or GestureKind.DoubleTapHold) parts.Add($"second tap within {b.DoubleTapMs} ms");
        parts.Add(string.IsNullOrWhiteSpace(b.Layers) ? "always" : "only in layer " + b.Layers);
        if (!b.Enabled) parts.Add("off");
        return string.Join("  \u00B7  ", parts);
    }

    // what the chosen How means, in a sentence
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

    // the sensible 'does the game still get it' for a How, set when the How changes (Details can change it again)
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
        passthrough.Checked = b.TapPassthrough;
        loading = was;
    }

    // "Double-tap Right Thumbrest  →  Opens wheel 'right'"
    void ShowSentence()
    {
        string how = b.Gesture switch
        {
            GestureKind.DoubleTap => "Double-tap", GestureKind.Hold or GestureKind.WhileHeld => "Hold", GestureKind.Tap => "Quick tap",
            GestureKind.DoubleTapHold => "Double-tap and hold", _ => "Press",
        };
        sentence.Text = how + " " + InputText.Friendly(b.Input) + "   →   " + MainForm.Plain(b.Action) + (b.Enabled ? "" : "   (off)");
        sentence.ForeColor = b.Enabled ? Theme.Text : Theme.OffText;
    }

    // a comfortable width, centred (the drawer is narrow: there it just uses its width)
    protected override void OnResize(EventArgs e)
    {
        base.OnResize(e);
        int side = Math.Max(12, (ClientSize.Width - 1500) / 2);
        if (Padding.Left != side)
            Padding = new Padding(side, 8, side, 8);
    }

    static NumericUpDown Ms() => new() { Minimum = 50, Maximum = 5000, Increment = 50, Width = 90 };

    public BindingEditor(BindingModel model)
    {
        b = model;
        // 1 How, 2 Button, 3 What happens; then the details, written as answers, open by default and foldable
        var howPanel = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Margin = Padding.Empty };
        gesture.Width = 360;
        howPanel.Controls.Add(gesture);
        howHint.AutoSize = true; howHint.MaximumSize = new Size(360, 0); howHint.ForeColor = Theme.SubText; howHint.Margin = new Padding(0, 6, 0, 0);
        howPanel.Controls.Add(howHint);
        var details = new FormPanel();
        details.AddRow("Name", name);
        details.AddRow("", enabled);
        details.AddRow("The game also gets the button", block, "Set for you when you pick How: a plain press is taken from the game; hold and double-tap let a quick tap through; a touch always reaches the game.");
        lPass = details.AddRow("", passthrough);
        lHold = details.AddRow("Hold time (ms)", hold, "How long counts as a hold. 400 is the default.");
        lTap = details.AddRow("Tap shorter than (ms)", tap);
        lDbl = details.AddRow("Second tap within (ms)", dbl, "How quick the second tap (or flick) must be.");
        lPressAt = details.AddRow("Press point (%)", pressAt, "How far the trigger or grip must go. 0 = usual, low = hair trigger, high = a long pull. For an Index full squeeze: how hard.");
        lReleaseAt = details.AddRow("Keeps going until (%)", releaseAt, "Double flick on a stick: once it has started it keeps going while you steer, until you let the stick come back below this much from the middle. 30 = until you mostly let go. 0 = it stops as soon as the stick leaves the direction.");
        details.AddRow("Only while layer", layers, "Leave empty to work always. With a layer name, this binding only works while that layer is on.");
        details.AddRow("Not while layer", notLayers);
        foreach (var (_, ctl, _) in details.Rows)
            if (ctl is ComboBox)
                ctl.Width = 400;

        sentence.Font = new Font("Segoe UI", 16f, FontStyle.Bold);
        sentence.AutoSize = true;
        sentence.Dock = DockStyle.Top;
        sentence.Padding = new Padding(0, 0, 0, 10);
        Card C(string t, Control body) => new(t, null, null, Color.Empty, null, body) { Dock = DockStyle.None, Margin = new Padding(0, 0, 14, 14) };
        var cards = new FlowLayoutPanel { AutoSize = true, WrapContents = true, Dock = DockStyle.Top, Margin = Padding.Empty };
        cards.Controls.Add(C("1   How", howPanel));
        cards.Controls.Add(C("2   Button", chord));
        cards.Controls.Add(C("3   What happens", action));
        var fold = new LinkLabel { AutoSize = true, Dock = DockStyle.Top, LinkColor = Theme.Link, Font = new Font("Segoe UI", 10.5f, FontStyle.Bold), Padding = new Padding(0, 4, 0, 6) };
        var detailsHost = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Top, Margin = Padding.Empty };
        detailsHost.Controls.Add(C("Details", details));
        detailsHost.Visible = detailsOpen;
        void ShowFold() => fold.Text = detailsOpen ? "Details \u25BE   (all set for you: change any)" : "Details \u25B8   " + DetailsSummary();
        fold.LinkClicked += (s, e) => { detailsOpen = !detailsOpen; detailsHost.Visible = detailsOpen; ShowFold(); };
        Stack(sentence, cards, fold, detailsHost);
        Changed += ShowSentence;
        Changed += () => { if (!detailsOpen) ShowFold(); };
        ShowFoldNow = ShowFold;
        ShowFold();

        loading = true;
        name.Text = b.Name;
        enabled.Checked = b.Enabled;
        chord.Value = b.Input;
        gesture.Code = b.Gesture;
        hold.Value = Math.Clamp(b.HoldMs, 50, 5000);
        tap.Value = Math.Clamp(b.TapMs, 50, 5000);
        dbl.Value = Math.Clamp(b.DoubleTapMs, 50, 5000);
        foreach (var l in new LayerConv().GetStandardValues(null))
        { layers.Items.Add(l); notLayers.Items.Add(l); }
        layers.Text = b.Layers ?? "";
        notLayers.Text = b.NotLayers ?? "";
        block.Code = b.Block;
        passthrough.Checked = b.TapPassthrough;
        pressAt.Value = (decimal)Math.Clamp(Math.Round(b.PressAt * 100), 0, 95);
        releaseAt.Value = (decimal)Math.Clamp(Math.Round(b.ReleaseAt * 100), 0, 90);
        action.Bind(b.Action ??= new ActionModel());
        loading = false;
        Sync();
        ShowSentence();

        name.TextChanged += (s, e) => { b.Name = name.Text; Fire(); };
        enabled.CheckedChanged += (s, e) => { b.Enabled = enabled.Checked; Fire(); };
        chord.Changed += () => { b.Input = chord.Value; Sync(); Fire(); };
        releaseAt.ValueChanged += (s, e) => { b.ReleaseAt = releaseAt.Value < 5 ? 0 : (double)releaseAt.Value / 100; Fire(); };
        pressAt.ValueChanged += (s, e) => { b.PressAt = pressAt.Value <= 1 ? 0 : (double)pressAt.Value / 100; Fire(); };
        gesture.SelectedIndexChanged += (s, e) => { if (loading) return; b.Gesture = gesture.Code; DefaultsForHow(); Sync(); Fire(); };
        hold.ValueChanged += (s, e) => { b.HoldMs = (int)hold.Value; Fire(); };
        tap.ValueChanged += (s, e) => { b.TapMs = (int)tap.Value; Fire(); };
        dbl.ValueChanged += (s, e) => { b.DoubleTapMs = (int)dbl.Value; Fire(); };
        layers.TextChanged += (s, e) => { b.Layers = layers.Text.Trim(); Fire(); };
        notLayers.TextChanged += (s, e) => { b.NotLayers = notLayers.Text.Trim(); Fire(); };
        block.SelectedIndexChanged += (s, e) => { if (loading) return; b.Block = block.Code; Fire(); };
        passthrough.CheckedChanged += (s, e) => { if (loading) return; b.TapPassthrough = passthrough.Checked; Fire(); };
        action.Changed += Fire;
    }

    void Sync()
    {
        howHint.Text = HowHint(b.Gesture);
        ShowFoldNow?.Invoke();
        FormPanel.Show(lHold, b.Gesture == GestureKind.Hold);
        FormPanel.Show(lPass, b.Gesture == GestureKind.Hold);
        FormPanel.Show(lTap, b.Gesture == GestureKind.Tap);
        FormPanel.Show(lDbl, b.Gesture is GestureKind.DoubleTap or GestureKind.DoubleTapHold);
        FormPanel.Show(lPressAt, IsAnalog(b.Input));
        FormPanel.Show(lReleaseAt, b.Gesture == GestureKind.DoubleTapHold && IsDirection(b.Input));
    }

    // The firing input (last of the chord) is a trigger / grip press, an Index trackpad press, or a full squeeze.
    // The firing input is a stick (or trackpad) direction: up / down / left / right.
    static bool IsDirection(string input)
    {
        var last = (input ?? "").Split('+')[^1].Trim().ToLowerInvariant();
        return last.EndsWith(".up") || last.EndsWith(".down") || last.EndsWith(".left") || last.EndsWith(".right");
    }

    internal static bool IsAnalog(string input)
    {
        var last = (input ?? "").Split('+')[^1].Trim().ToLowerInvariant();
        var parts = last.Split('.');
        if (parts.Length < 2) return false;
        string comp = parts[1], way = parts.Length > 2 ? parts[2] : "";
        return (comp is "trigger" or "grip" && way is "" or "full") || (comp == "trackpad" && way == "");
    }
}
