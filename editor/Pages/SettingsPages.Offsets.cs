using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;
using static OverviewPrototype.Fields;

namespace OverviewPrototype;

// Settings > Controller offsets and Settings > Advanced (WinForms SettingsEditor.BuildOffsets / BuildAdvanced).
public partial class SettingsPageView
{
    Form Advanced()
    {
        var st = p.Style;
        var th = p.Thresholds;
        var f = new Form { LabelWidth = 260 };
        f.AddRow("", Check("Bring the game window to the front before sending keys", () => p.FocusGameWindow, v => p.FocusGameWindow = v),
            "Turn on if keys end up in the wrong window.");
        f.AddRow("Trigger / grip counts as pressed at", Num(0.05m, 1, 0.05m, 2, () => th.PressOn, v => th.PressOn = v));
        f.AddRow("... and released at", Num(0, 1, 0.05m, 2, () => th.PressOff, v => th.PressOff = v));
        f.AddRow("Stick direction counts at", Num(0.1m, 1, 0.05m, 2, () => th.StickOn, v => th.StickOn = v));
        f.AddRow("... and released at", Num(0, 1, 0.05m, 2, () => th.StickOff, v => th.StickOff = v));
        f.AddRow("Centre circle size", Num(0.15m, 0.7m, 0.02m, 2, () => st.InnerRatio, v => st.InnerRatio = v));
        f.AddRow("Gap between segments (deg)", Num(0, 10, 0.5m, 1, () => st.GapDeg, v => st.GapDeg = v));
        f.AddRow("Sharpness (pixels)", Int(256, 2048, 128, () => st.TexturePx, v => st.TexturePx = v), "Higher is sharper but uses more memory.");
        f.AddRow("", ShowExperimentalBox(), "Adds the ⚠ Experimental page to Settings. The page stays listed while one of its features is on, so you can always switch it off.");
        return f;
    }

    static OffsetModel CloneOffset(OffsetModel a) => new() { Pitch = a.Pitch, Yaw = a.Yaw, Roll = a.Roll, X = a.X, Y = a.Y, Z = a.Z };

    // the right hand's values flipped for the left hand
    static void MirrorTo(OffsetModel a, OffsetModel d) { d.Pitch = a.Pitch; d.Yaw = -a.Yaw; d.Roll = -a.Roll; d.X = -a.X; d.Y = a.Y; d.Z = a.Z; }

    Form Offsets()
    {
        var co = p.Offsets;
        var f = new Form { LabelWidth = 200 };
        f.AddRow("", Check("Use controller offsets in this game", () => co.Enabled, v => co.Enabled = v),
            "Turns and moves the controllers the GAME sees: its hands, guns and its own laser. Untick to give the game the plain controllers (your numbers are kept). Each game can have its own profile with its own offsets.");
        var rows = new List<(Func<OffsetModel, float> get, Slider bar, NumericUpDown num)>();
        bool refreshing = false;
        int selHand = co.LinkHands ? 2 : 1, selKind = co.AimSame ? 2 : 0;
        OffsetModel Target()
        {
            var h = co.LinkHands || selHand == 1 ? co.Right : co.Left;
            return co.AimSame || selKind == 0 ? h.Grip : h.Aim;
        }
        void Refresh()
        {
            refreshing = true;
            var t = Target();
            foreach (var r in rows)
            {
                decimal v = Math.Clamp((decimal)r.get(t), r.num.Minimum, r.num.Maximum);
                r.num.Value = v;
                r.bar.Value = (double)v;
            }
            refreshing = false;
        }
        f.AddRow("Hand", Pick(new Choice[] { new("2", "Both hands, mirrored (the left copies the right)"), new("1", "Right hand on its own"), new("0", "Left hand on its own") },
            () => selHand.ToString(), v =>
            {
                int n = int.Parse(v);
                if (co.LinkHands && n != 2)
                { MirrorTo(co.Right.Grip, co.Left.Grip); MirrorTo(co.Right.Aim, co.Left.Aim); } // nothing jumps when unlinking
                co.LinkHands = n == 2;
                selHand = n;
                Refresh();
            }), "Mirrored: yaw, roll and left/right are flipped for the left hand, so both feel the same.");
        f.AddRow("Pose", Pick(new Choice[] { new("2", "Grip and aim together"), new("0", "Grip only (the hand, held guns and swords)"), new("1", "Aim only (the game's own laser / pointer ray)") },
            () => selKind.ToString(), v =>
            {
                int n = int.Parse(v);
                if (co.AimSame && n != 2)
                { co.Right.Aim = CloneOffset(co.Right.Grip); co.Left.Aim = CloneOffset(co.Left.Grip); }
                co.AimSame = n == 2;
                selKind = n;
                Refresh();
            }), "Most games put hands and weapons on the grip pose. Together is the safe choice: whatever the game uses gets the same fix.");

        void Row(string label, string unit, int min, int max, Func<OffsetModel, float> get, Action<OffsetModel, float> set, string hint)
        {
            var bar = new Slider { Minimum = min, Maximum = max, Width = 220, SmallChange = 0.5, LargeChange = 5, VerticalAlignment = VerticalAlignment.Center };
            var num = new NumericUpDown { Minimum = min, Maximum = max, Increment = 0.5m, FormatString = "0.0", Width = 130 };
            var minus = new Button { Content = "−" };
            var plus = new Button { Content = "+" };
            var zero = new Button { Content = "0" };
            ToolTip.SetTip(zero, "Set back to 0");
            void Apply(decimal v)
            {
                v = Math.Round(Math.Clamp(v, min, max), 1);
                if (refreshing || Math.Round((decimal)get(Target()), 1) == v)
                    return; // a refresh, or no real change
                set(Target(), (float)v);
                Refresh();
                Shell.Store.MarkDirty();
            }
            bar.ValueChanged += (s, e) => Apply(Math.Round((decimal)bar.Value * 2) / 2); // half steps, like the WinForms bar
            num.ValueChanged += (s, e) => { if (num.Value != null) Apply(num.Value.Value); };
            minus.Click += (s, e) => Apply((num.Value ?? 0) - 0.5m);
            plus.Click += (s, e) => Apply((num.Value ?? 0) + 0.5m);
            zero.Click += (s, e) => Apply(0);
            rows.Add((get, bar, num));
            f.AddRow(label, new StackPanel
            {
                Orientation = Orientation.Horizontal, Spacing = 6,
                Children = { minus, bar, plus, num, new TextBlock { Text = unit, Width = 34, VerticalAlignment = VerticalAlignment.Center }, zero },
            }, hint);
        }
        Row("Pitch (tip up +)", "deg", -60, 60, o => o.Pitch, (o, v) => o.Pitch = v,
            "If you have to point the controller down to aim down a gun's sights, raise this (Quest controllers often need +10 to +20).");
        Row("Yaw (turn left +)", "deg", -60, 60, o => o.Yaw, (o, v) => o.Yaw = v, null);
        Row("Roll (roll left +)", "deg", -90, 90, o => o.Roll, (o, v) => o.Roll = v, null);
        Row("Up / down", "cm", -20, 20, o => o.Y, (o, v) => o.Y = v, null);
        Row("Right / left", "cm", -20, 20, o => o.X, (o, v) => o.X = v, null);
        Row("Forward / back", "cm", -20, 20, o => -o.Z, (o, v) => o.Z = -v, "Positions move the hand in the controller's own directions.");

        var r1 = new Button { Content = "Reset this hand / pose" };
        var r2 = new Button { Content = "Reset everything" };
        r1.Click += (s, e) => { Target().Clear(); Refresh(); Shell.Store.MarkDirty(); };
        r2.Click += (s, e) => { foreach (var m in new[] { co.Left.Grip, co.Left.Aim, co.Right.Grip, co.Right.Aim }) m.Clear(); Refresh(); Shell.Store.MarkDirty(); };
        f.AddRow("Reset", new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Children = { r1, r2 } }, "Back to 0, the game's own controller angles.");
        f.AddRow("Steady the game's hands", Num(0, 1, 0.05m, 2, () => co.Smoothing, v => co.Smoothing = v),
            "For shaky hands: smooths the hands and guns in the game while you hold still; quick moves still follow straight away. 0 = off, 0.5 = a good start.");
        f.AddRow("In the headset", new TextBlock
        {
            TextWrapping = TextWrapping.Wrap, MaxWidth = 460, Foreground = Brush.Parse("#AAB2BF"),
            Text = "Add a button with the action \"Controller offsets\" to a wheel or wrist panel (the default left wrist panel has it under More). It opens a panel with + / - buttons (hold to repeat, or point at a row and push the stick), Grab to set (hold grip, turn the controller to where it feels right, let go) and resets. Changes save by themselves.",
        });
        Refresh();
        return f;
    }
}
