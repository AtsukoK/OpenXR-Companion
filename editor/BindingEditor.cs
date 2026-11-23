using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

// ------------------------------------------------------------ binding editor
public class BindingEditor : EditorBase
{
    readonly BindingModel b;
    readonly TextBox name = new() { Width = 360 };
    readonly CheckBox enabled = new() { Text = "Enabled", AutoSize = true };
    readonly ChordEditor chord = new();
    readonly ChoiceBox gesture = new(Choices.Gestures, 420);
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
        var when = new FormPanel();
        when.AddRow("Button", chord, "For a combo, the button that fires goes last; the others must already be held.");
        when.AddRow("How", gesture);
        lReleaseAt = when.AddRow("Keeps going until (%)", releaseAt, "Double flick on a stick: once it has started it keeps going while you steer, until you let the stick come back below this much from the middle. 30 = until you mostly let go. 0 = the old way: it stops as soon as the stick leaves the direction (the direction zone) or drops below the direction's release point.");
        lPressAt = when.AddRow("Press point (%)", pressAt, "How far the trigger or grip must go. 0 = usual, low = hair trigger, high = a long pull. For an Index full squeeze: how hard.");
        lHold = when.AddRow("Hold time (ms)", hold);
        lTap = when.AddRow("Tap shorter than (ms)", tap);
        lDbl = when.AddRow("Second tap within (ms)", dbl);
        when.AddRow("Only while layer", layers, "Leave empty to work always. With a layer name, this binding only works while that layer is on.");
        when.AddRow("Not while layer", notLayers);
        when.AddRow("Pass input to game", block);
        lPass = when.AddRow("", passthrough);
        foreach (var (_, ctl, _) in when.Rows)
            if (ctl is ComboBox)
                ctl.Width = 400; // same width as the action's dropdowns below

        // top: what it does in one sentence, then its name and the on/off switch
        sentence.Font = new Font("Segoe UI", 16f, FontStyle.Bold);
        sentence.AutoSize = true;
        sentence.Dock = DockStyle.Top;
        sentence.Padding = new Padding(0, 0, 0, 6);
        var top = new FlowLayoutPanel { AutoSize = true, WrapContents = true, Dock = DockStyle.Top, Padding = new Padding(0, 0, 0, 10) };
        top.Controls.Add(new Label { Text = "Name:", AutoSize = true, Margin = new Padding(0, 7, 6, 0) });
        top.Controls.Add(name);
        enabled.Margin = new Padding(16, 5, 0, 0);
        enabled.Font = new Font("Segoe UI", 10f, FontStyle.Bold);
        top.Controls.Add(enabled);
        Card C(string t, string glyph, Control body) =>
            new(t, glyph, "Saved with profile", Theme.Saved, "Saved with this profile: changes take effect in the game when you press Save (Ctrl+S).", body) { Dock = DockStyle.None, Margin = new Padding(0, 0, 14, 14) };
        var cards = new FlowLayoutPanel { AutoSize = true, WrapContents = true, Dock = DockStyle.Top, Margin = Padding.Empty };
        cards.Controls.Add(C("When you use the button", "", when));
        cards.Controls.Add(C("What it does", "", action));
        Stack(sentence, top, cards);
        Changed += ShowSentence;

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
        gesture.SelectedIndexChanged += (s, e) => { b.Gesture = gesture.Code; Sync(); Fire(); };
        hold.ValueChanged += (s, e) => { b.HoldMs = (int)hold.Value; Fire(); };
        tap.ValueChanged += (s, e) => { b.TapMs = (int)tap.Value; Fire(); };
        dbl.ValueChanged += (s, e) => { b.DoubleTapMs = (int)dbl.Value; Fire(); };
        layers.TextChanged += (s, e) => { b.Layers = layers.Text.Trim(); Fire(); };
        notLayers.TextChanged += (s, e) => { b.NotLayers = notLayers.Text.Trim(); Fire(); };
        block.SelectedIndexChanged += (s, e) => { b.Block = block.Code; Fire(); };
        passthrough.CheckedChanged += (s, e) => { b.TapPassthrough = passthrough.Checked; Fire(); };
        action.Changed += Fire;
    }

    void Sync()
    {
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
