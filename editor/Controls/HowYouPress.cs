using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;
using static OverviewPrototype.Fields;

namespace OverviewPrototype;

// "How you press": one strip near the top of the Radial wheels, Wrist panels, wheel and wrist panel pages, so nobody has
// to hunt for it in the settings. An "Everything" choice sets the wheels and both wrist panels at once; the three own
// choices below it can still differ (it then says "Mixed").
//   wheels:       profile selection.method  thumbstick | point | both
//   wrist panels: wrist_left/right.press     poke | point | both
// The wheels have no touching: their no-laser way is the thumbstick, so "Touch" = poke on panels + thumbstick on wheels.
public class HowYouPress : Border
{
    static readonly Choice[] Master =
    {
        new("point", "Point + trigger (laser)"),
        new("touch", "No laser: touch the panels, thumbstick on wheels"),
        new("both", "Either way (point or touch / thumbstick)"),
    };
    static readonly Choice[] Wheel = { new("point", "Point + trigger"), new("thumbstick", "Thumbstick toward the item"), new("both", "Either") };
    static readonly Choice[] Wrist = { new("point", "Point + trigger"), new("poke", "Touch with your finger"), new("both", "Either") };

    // which row to show first and lit up: "wheel", "wrist0", "wrist1" or null (all equal)
    readonly string focus;
    static bool expanded = Environment.GetEnvironmentVariable("VRO_SHOT_PRESS_OPEN") == "1"; // "Separate settings" open (kept while the editor runs; env var for screenshot checks)
    static event Action AnyChanged; // keeps every strip on screen in step

    public HowYouPress(string focus = null)
    {
        this.focus = focus;
        Classes.Add("card");
        Padding = new Thickness(14, 8);
        Margin = new Thickness(0, 4, 0, 8);
        BorderBrush = Brush.Parse("#4A90F0");
        BorderThickness = new Thickness(3, 0, 0, 0);
        Build();
        Action again = () => Avalonia.Threading.Dispatcher.UIThread.Post(Build);
        AttachedToVisualTree += (s, e) => AnyChanged += again;
        DetachedFromVisualTree += (s, e) => AnyChanged -= again;
    }

    static ProfileModel P => Shell.Store.Profile;

    static string MasterNow()
    {
        string w = P.Selection.Method ?? "both", l = P.WristLeft.Press ?? "both", r = P.WristRight.Press ?? "both";
        if (w == "point" && l == "point" && r == "point") return "point";
        if (w == "thumbstick" && l == "poke" && r == "poke") return "touch";
        if (w == "both" && l == "both" && r == "both") return "both";
        return "mixed";
    }

    void Build()
    {
        var p = P;
        var master = MasterNow();
        var masterChoices = master == "mixed" ? Master.Append(new Choice("mixed", "Mixed (set one by one below)")).ToArray() : Master;
        var all = Pick(masterChoices, () => MasterNow(), v =>
        {
            if (v == "mixed") return;
            p.Selection.Method = v == "touch" ? "thumbstick" : v;
            p.WristLeft.Press = p.WristRight.Press = v == "touch" ? "poke" : v;
            AnyChanged?.Invoke();
        }, 300);

        Control Row(string key, string label, Choice[] items, Func<string> get, Action<string> set)
        {
            var name = new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center, FontWeight = key == focus ? FontWeight.Bold : FontWeight.Normal };
            var pick = Pick(items, () => get() ?? "both", v => { set(v); AnyChanged?.Invoke(); }, 220);
            return new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Children = { name, pick } };
        }
        var rows = new List<(string key, Control c)>
        {
            ("wheel", Row("wheel", "Radial wheels", Wheel, () => p.Selection.Method, v => p.Selection.Method = v)),
            ("wrist0", Row("wrist0", "Left wrist panel", Wrist, () => p.WristLeft.Press, v => p.WristLeft.Press = v)),
            ("wrist1", Row("wrist1", "Right wrist panel", Wrist, () => p.WristRight.Press, v => p.WristRight.Press = v)),
        };
        if (focus != null) // this page's own row first
            rows = rows.OrderBy(r => r.key == focus ? 0 : focus == "wrist" && r.key.StartsWith("wrist") ? 0 : 1).ToList();

        const string help = "Point + trigger: aim the laser at an item and pull the trigger. Touch: push your other hand's fingertip into a wrist panel button. " +
                            "Wheels have no touching: without the laser you push the thumbstick toward an item. Saved with the profile (Ctrl+S).";
        bool active = master != "mixed";
        var title = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6, VerticalAlignment = VerticalAlignment.Center, Children =
        {
            new TextBlock { Text = "How you press", FontWeight = FontWeight.Bold, VerticalAlignment = VerticalAlignment.Center },
            new TextBlock { Text = "ⓘ", Foreground = Brush.Parse("#4A90F0"), VerticalAlignment = VerticalAlignment.Center },
        } };
        ToolTip.SetTip(title, help);
        // is "Everything" in effect? green = one way for all three; orange = they differ (set separately)
        var badge = new Border
        {
            CornerRadius = new CornerRadius(4), Padding = new Thickness(8, 3), VerticalAlignment = VerticalAlignment.Center,
            Background = Brush.Parse(active ? "#2E9E5B" : "#B8741A"),
            Child = new TextBlock { Text = active ? "✓ Active for all" : "Not active: set separately", Foreground = Brushes.White, FontWeight = FontWeight.SemiBold },
        };
        ToolTip.SetTip(badge, active ? "Wheels and both wrist panels all use this." : "The wheels and wrist panels use different ways. Pick one above to make them all the same, or see them with 'Separate settings'.");
        var more = new Button { Classes = { "tool" }, Content = (expanded ? "▾" : "▸") + " Separate settings", VerticalAlignment = VerticalAlignment.Center };
        more.Click += (s, e) => { expanded = !expanded; Build(); };
        var line = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 12, Children = { title, all, badge, more } };
        var separate = new WrapPanel { IsVisible = expanded, Margin = new Thickness(0, 8, 0, 0) };
        foreach (var r in rows) { r.c.Margin = new Thickness(0, 0, 28, 4); separate.Children.Add(r.c); }
        HorizontalAlignment = HorizontalAlignment.Left;
        Child = new StackPanel { Children = { line, separate } };
    }
}
