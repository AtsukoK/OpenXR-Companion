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
    readonly Label lHold, lTap, lDbl, lPass;

    static NumericUpDown Ms() => new() { Minimum = 50, Maximum = 5000, Increment = 50, Width = 90 };

    public BindingEditor(BindingModel model)
    {
        b = model;
        var when = new FormPanel();
        when.AddRow("Name", name);
        when.AddRow("", enabled);
        when.AddRow("Input", chord, "For a combo, the button that fires goes last; the others must already be held.");
        when.AddRow("Gesture", gesture);
        lHold = when.AddRow("Hold time (ms)", hold);
        lTap = when.AddRow("Tap shorter than (ms)", tap);
        lDbl = when.AddRow("Second tap within (ms)", dbl);
        when.AddRow("Only in layer", layers, "Leave empty to work always. With a layer name, this binding only works while that layer is on.");
        when.AddRow("Not in layer", notLayers);
        when.AddRow("Pass input to game", block);
        lPass = when.AddRow("", passthrough);
        foreach (var (_, ctl, _) in when.Rows)
            if (ctl is ComboBox)
                ctl.Width = 400; // same width as the action's dropdowns below

        Stack(Title("Binding"),
              FormPanel.Header("Do something when a controller button (or combo) is used."),
              when,
              FormPanel.Header("Action:"),
              action);

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
        action.Bind(b.Action ??= new ActionModel());
        loading = false;
        Sync();

        name.TextChanged += (s, e) => { b.Name = name.Text; Fire(); };
        enabled.CheckedChanged += (s, e) => { b.Enabled = enabled.Checked; Fire(); };
        chord.Changed += () => { b.Input = chord.Value; Fire(); };
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
    }
}
