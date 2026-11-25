using Avalonia.Controls;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Settings > Headset mirror (WinForms SettingsEditor.BuildHeadsetMirror). App-wide, in headset_mirror.json shared with
// the layer: every change is written straight away and never marks the profile unsaved. Like the other fields, a value
// is written only when the user changes it (Avalonia fires change events on load too).
public partial class SettingsPageView
{
    static CheckBox MCheck(string text, Func<bool> get, Action<bool> set)
    {
        var c = new CheckBox { Content = new TextBlock { Text = text, TextWrapping = TextWrapping.Wrap, MaxWidth = 520 }, IsChecked = get() };
        c.IsCheckedChanged += (s, e) => { bool v = c.IsChecked == true; if (v != get()) set(v); };
        return c;
    }
    static NumericUpDown MNum(decimal min, decimal max, decimal step, int dec, Func<double> get, Action<double> set)
    {
        decimal Shown() => Math.Round(Math.Clamp((decimal)get(), min, max), dec);
        var n = new NumericUpDown { Minimum = min, Maximum = max, Increment = step, Width = 140, FormatString = dec == 0 ? "0" : "0." + new string('0', dec), Value = Shown() };
        n.ValueChanged += (s, e) => { if (n.Value != null && n.Value != Shown()) set((double)n.Value.Value); };
        return n;
    }

    Form HeadsetMirror()
    {
        var f = new Form { LabelWidth = 220 };
        var on = MCheck("Show the headset mirror window", () => HeadsetMirrorFile.On, v => HeadsetMirrorFile.On = v);
        f.AddRow("", on, "Same as the button on the toolbar. A desktop window with what the headset shows, including the wheel, wrist panels, screen and lasers. For OBS, capture that window. Closing it turns it off. D3D11 games (Bannerlord VR, Fallout VR mods...).");
        // the layer turns it off when its window is closed: follow that while the page is open
        var t = new Avalonia.Threading.DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        t.Tick += (s, e) => { if ((on.IsChecked == true) != HeadsetMirrorFile.On) on.IsChecked = HeadsetMirrorFile.On; };
        on.AttachedToVisualTree += (s, e) => t.Start();
        on.DetachedFromVisualTree += (s, e) => t.Stop();
        f.AddRow("", MCheck("Always on top of other windows", () => HeadsetMirrorFile.OnTop, v => HeadsetMirrorFile.OnTop = v),
            "Keeps the mirror above the game on your monitor, e.g. to record or show it. The mirror never takes focus from the game, even when you click or move it, so the game keeps getting your input.");

        // Saved as two values: "eye" (left / right) and "both_eyes" (the other eye fills the nose side).
        string EyeCode() => (HeadsetMirrorFile.BothEyes ? "both_" : "") + HeadsetMirrorFile.Eye;
        var eye = new ChoiceCombo(new Choice[] {
            new("left", "Left eye"), new("right", "Right eye"),
            new("both_right", "Both eyes (right eye, left fills in)"), new("both_left", "Both eyes (left eye, right fills in)") }, 320) { Code = EyeCode() };
        eye.SelectionChanged += (s, e) =>
        {
            string code = eye.Code ?? "right";
            if (code == EyeCode()) return;
            HeadsetMirrorFile.BothEyes = code.StartsWith("both_");
            HeadsetMirrorFile.Eye = code.EndsWith("left") ? "left" : "right";
        };
        f.AddRow("Eye", eye, "One eye: exactly that eye's picture. Both eyes: centred on where that eye looks, and the other eye fills in the nose side, where one eye's picture ends in a black strip, so even zoom 1 stays centred. Far things line up; close things (your hands, the gun) can look a little doubled on that side.");

        var preset = new ChoiceCombo(new Choice[] {
            new("custom", "Custom"), new("gaze", "Centred where you look (default)"), new("gaze_room", "Centred, smoothing makes its own room"),
            new("both", "Wide: both eyes, centred where you look"), new("original", "Original: middle of the picture"),
            new("exact", "Exact headset view (no zoom, no smoothing)") }, 320);
        f.AddRow("Preset", preset, "Sets zoom, head camera smoothing, centre, edge margin, smoothing's own room and one / both eyes together (the side you picked stays). Change any of them afterwards and it shows Custom.");

        var zoom = MNum(1, 3, 0.05m, 2, () => HeadsetMirrorFile.Zoom, v => HeadsetMirrorFile.Zoom = v);
        f.AddRow("Zoom", zoom, "1 = the whole eye picture (very wide, blurry edges). 1.3 looks like a normal camera. The same with smoothing on or off. It always fills the window; resize the window for a different shape.");

        // Saved as one value, "smoothing" (0 = off); the strength is kept while it is off.
        var smoothOn = new CheckBox { Content = "Head camera smoothing", IsChecked = HeadsetMirrorFile.Smoothing > 0.001 };
        var st = MNum(0.05m, 1, 0.05m, 2,
            () => HeadsetMirrorFile.Smoothing > 0.001 ? HeadsetMirrorFile.Smoothing : HeadsetMirrorFile.SmoothingStrength,
            v => { HeadsetMirrorFile.SmoothingStrength = v; if (smoothOn.IsChecked == true) HeadsetMirrorFile.Smoothing = v; });
        st.IsEnabled = smoothOn.IsChecked == true;
        smoothOn.IsCheckedChanged += (s, e) =>
        {
            bool v = smoothOn.IsChecked == true;
            st.IsEnabled = v;
            if (v != HeadsetMirrorFile.Smoothing > 0.001)
                HeadsetMirrorFile.Smoothing = v ? (double)(st.Value ?? 0.5m) : 0;
        };
        decimal SmoothingShown() => smoothOn.IsChecked == true ? st.Value ?? 0 : 0;
        f.AddRow("", smoothOn, "Smooths small head shakes out of the window, like a camera stabiliser; real head turns still follow. Off = the exact headset view. It can only drift into the picture the zoom leaves out: at zoom 1 it barely works, from about 1.3 it works fully (or turn on \"Smoothing makes its own room\").");
        f.AddRow("Smoothing strength", st, "How much head shake it takes out: 0.05 = a little, 0.5 = the default, 1 = the most (real turns follow later).");

        var centre = new ChoiceCombo(new Choice[] { new("gaze", "Where the eye looks"), new("middle", "Middle of the picture") }, 240) { Code = HeadsetMirrorFile.Center };
        centre.SelectionChanged += (s, e) => { if (centre.Code != null && centre.Code != HeadsetMirrorFile.Center) HeadsetMirrorFile.Center = centre.Code; };
        f.AddRow("Centre the view on", centre, "Where the eye looks: what you aim at sits in the middle of the window. Middle of the picture: the older way, a little off to one side on Quest, but further from the black strip on the nose side.");
        var margin = MNum(0, 0.3m, 0.01m, 2, () => HeadsetMirrorFile.SideMargin, v => HeadsetMirrorFile.SideMargin = v);
        f.AddRow("Edge margin", margin, "How far the view stays from the eye picture's sides (0.12 = 12% of its width), to keep the black strip on the nose side out. Lower it if you see no strip, raise it if you do.");
        var room = MCheck("Smoothing makes its own room", () => HeadsetMirrorFile.SteadyRoom, v => HeadsetMirrorFile.SteadyRoom = v);
        f.AddRow("", room, "Off: zoom means the same with smoothing on or off, and smoothing only uses the room the zoom leaves. On: smoothing crops in around the centre by itself so it always works fully, so low zoom looks closer.");
        var fps = new NumericUpDown { Minimum = 0, Maximum = 240, Increment = 5, Width = 140, FormatString = "0", Value = Math.Clamp(HeadsetMirrorFile.Fps, 0, 240) };
        fps.ValueChanged += (s, e) => { if (fps.Value != null && (int)fps.Value.Value != HeadsetMirrorFile.Fps) HeadsetMirrorFile.Fps = (int)fps.Value.Value; };
        f.AddRow("Frame rate cap (fps)", fps, "0 = every headset frame. 60 is smooth for recording and costs little. 30 halves the cost.");
        var quality = new ChoiceCombo(new Choice[] { new("light", "Light (cheaper, default)"), new("full", "Full (copy with mipmaps)") }, 240) { Code = HeadsetMirrorFile.Quality };
        quality.SelectionChanged += (s, e) => { if (quality.Code != null && quality.Code != HeadsetMirrorFile.Quality) HeadsetMirrorFile.Quality = quality.Code; };
        f.AddRow("Picture", quality, "Light draws straight from the game's picture: much cheaper, looks the same at normal window sizes. Full copies the picture and makes mipmaps first: smoother only when the window is much smaller than the headset picture (light switches to it by itself then).");

        // zoom, smoothing (0 = off), centre, edge margin, own room, both eyes
        var presets = new Dictionary<string, (decimal zoom, decimal steady, string centre, decimal margin, bool room, bool both)>
        {
            ["gaze"] = (1.3m, 0m, "gaze", 0.12m, false, false), // = the app's defaults
            ["gaze_room"] = (1.3m, 0.5m, "gaze", 0.12m, true, false),
            ["both"] = (1.0m, 0.5m, "gaze", 0.12m, false, true),
            ["original"] = (1.3m, 0.5m, "middle", 0.04m, true, false),
            ["exact"] = (1.0m, 0m, "middle", 0m, false, false),
        };
        bool applying = false;
        string Matching()
        {
            foreach (var (k, v) in presets)
                if (zoom.Value == v.zoom && SmoothingShown() == v.steady && centre.Code == v.centre && margin.Value == v.margin &&
                    (room.IsChecked == true) == v.room && (eye.Code ?? "").StartsWith("both_") == v.both)
                    return k;
            return "custom";
        }
        void ShowMatching() { applying = true; preset.Code = Matching(); applying = false; }
        ShowMatching();
        preset.SelectionChanged += (s, e) =>
        {
            if (applying || !presets.TryGetValue(preset.Code ?? "", out var v) || preset.Code == Matching()) return;
            applying = true;
            zoom.Value = v.zoom;
            if (v.steady > 0) st.Value = v.steady;
            smoothOn.IsChecked = v.steady > 0;
            centre.Code = v.centre;
            margin.Value = v.margin;
            room.IsChecked = v.room;
            string side = (eye.Code ?? "right").EndsWith("left") ? "left" : "right";
            eye.Code = (v.both ? "both_" : "") + side;
            applying = false;
        };
        void Changed() { if (!applying) ShowMatching(); }
        zoom.ValueChanged += (s, e) => Changed();
        st.ValueChanged += (s, e) => Changed();
        smoothOn.IsCheckedChanged += (s, e) => Changed();
        centre.SelectionChanged += (s, e) => Changed();
        margin.ValueChanged += (s, e) => Changed();
        room.IsCheckedChanged += (s, e) => Changed();
        eye.SelectionChanged += (s, e) => Changed();
        f.AddRow("", new TextBlock { Classes = { "desc" }, Text = "These apply straight away, in every game and profile (no need to save)." });
        return f;
    }
}
