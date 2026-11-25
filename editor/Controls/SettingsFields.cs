using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// The small builders of the WinForms SettingsEditor (Check, Pick, Num, Int, TextField, ResetPose, ColorPick), each bound
// to a profile value. Like WinForms, a field writes only when you change it: a value shown on load (clamped into the
// field's range, or a default shown for a missing value) is never written back by itself.
public static class Fields
{
    static void Changed() => Shell.Store.MarkDirty();

    public static CheckBox Check(string text, Func<bool> get, Action<bool> set)
    {
        var c = new CheckBox { Content = new TextBlock { Text = text, TextWrapping = TextWrapping.Wrap, MaxWidth = 520 }, IsChecked = get() };
        c.IsCheckedChanged += (s, e) =>
        {
            bool v = c.IsChecked == true;
            if (v == get()) return;
            set(v);
            Changed();
        };
        return c;
    }

    // get: the value as shown (with its default for a missing value), so an untouched field never writes.
    public static ChoiceCombo Pick(IEnumerable<Choice> items, Func<string> get, Action<string> set, double width = 380)
    {
        var c = new ChoiceCombo(items, width) { Code = get() };
        c.SelectionChanged += (s, e) =>
        {
            if (c.Code == null || c.Code == get()) return;
            set(c.Code);
            Changed();
        };
        return c;
    }

    public static NumericUpDown Num(decimal min, decimal max, decimal step, int dec, Func<float> get, Action<float> set)
    {
        decimal Shown() => Math.Round(Math.Clamp((decimal)get(), min, max), dec);
        var n = new NumericUpDown { Minimum = min, Maximum = max, Increment = step, Width = 140, FormatString = dec == 0 ? "0" : "0." + new string('0', dec), Value = Shown() };
        n.ValueChanged += (s, e) =>
        {
            if (n.Value == null || n.Value == Shown()) return;
            set((float)n.Value.Value);
            Changed();
        };
        return n;
    }

    public static NumericUpDown Int(int min, int max, int step, Func<int> get, Action<int> set)
    {
        int Shown() => Math.Clamp(get(), min, max);
        var n = new NumericUpDown { Minimum = min, Maximum = max, Increment = step, Width = 140, FormatString = "0", Value = Shown() };
        n.ValueChanged += (s, e) =>
        {
            if (n.Value == null || (int)n.Value.Value == Shown()) return;
            set((int)n.Value.Value);
            Changed();
        };
        return n;
    }

    public static TextBox Text(Func<string> get, Action<string> set, double width = 220)
    {
        var t = new TextBox { Width = width, Text = get() ?? "" };
        t.TextChanged += (s, e) =>
        {
            if ((t.Text ?? "") == (get() ?? "")) return;
            set(t.Text);
            Changed();
        };
        return t;
    }

    // "Placed by hand: Yes, set in the headset  [Use the numbers below again]"
    public static Control ResetPose(Func<bool> isSet, Action clear, Action after = null,
                                    string notSet = "No, using the numbers below", string back = "Use the numbers below again")
    {
        var l = new TextBlock { VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, 0, 8, 0) };
        var b = new Button { Content = back };
        void Show() { bool on = isSet(); l.Text = on ? "Yes, set in the headset" : notSet; b.IsEnabled = on; }
        b.Click += (s, e) => { clear(); Show(); Changed(); after?.Invoke(); };
        Show();
        return new StackPanel { Orientation = Orientation.Horizontal, Children = { l, b } };
    }

    // A colour as #RRGGBBAA: a swatch with its name, and the hex code to type. The transparency is kept.
    public static Control Colour(string label, Func<string> get, Action<string> set)
    {
        var swatch = new Border { Width = 26, Height = 26, CornerRadius = new CornerRadius(4), BorderBrush = Brush.Parse("#4A5260"), BorderThickness = new Thickness(1) };
        var hex = new TextBox { Width = 110, Text = get() ?? "" };
        void Show() => swatch.Background = WheelView.TryColor(get(), out var c) ? new SolidColorBrush(c) : Brushes.Transparent;
        hex.TextChanged += (s, e) =>
        {
            string v = (hex.Text ?? "").Trim();
            if (v == (get() ?? "") || !WheelView.TryColor(v, out var c))
                return; // unchanged, or not a colour yet (still typing)
            if (!v.StartsWith('#'))
                v = "#" + v;
            if (v.Length == 7) // keep the old transparency when only #RRGGBB is typed
                v += WheelView.TryColor(get(), out var old) ? old.A.ToString("X2") : "FF";
            set(v.ToUpperInvariant());
            Show();
            Changed();
        };
        Show();
        return new StackPanel
        {
            Orientation = Orientation.Horizontal, Spacing = 6, Margin = new Thickness(0, 0, 14, 6),
            Children = { ColourButton.Wrap(swatch, get, v => hex.Text = v), new TextBlock { Text = label, Width = 90, VerticalAlignment = VerticalAlignment.Center }, hex },
        };
    }
}
