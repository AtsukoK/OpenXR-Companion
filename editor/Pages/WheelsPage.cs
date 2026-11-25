using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Every wheel as a card: its picture, name, items and what opens it. Port of the WinForms WheelsHub.
public class WheelsPage : UserControl
{
    static readonly IBrush Green = Brush.Parse("#3CBE6E"), Orange = Brush.Parse("#F0A040");

    // Who opens a wheel: a controller binding, or an item on another wheel (then it is a sub-wheel).
    public static string OpenerOf(ProfileModel p, string name, out bool isSub)
    {
        isSub = false;
        var b = p.Bindings.FirstOrDefault(x => x.Enabled && x.Action?.Type == ActionType.Wheel && x.Action.Wheel == name)
             ?? p.Bindings.FirstOrDefault(x => x.Action?.Type == ActionType.Wheel && x.Action.Wheel == name);
        if (b != null)
            return (b.Enabled ? "Opened by " : "Switched off: ") + InputText.Friendly(b.Input) + b.Gesture switch
            {
                GestureKind.Hold => " (hold)", GestureKind.Tap => " (tap)", GestureKind.DoubleTap => " (double-tap)",
                GestureKind.DoubleTapHold => " (double-tap and hold)", GestureKind.WhileHeld => " (while held)", _ => "",
            };
        foreach (var w in p.Wheels.Values)
            foreach (var it in w.Items)
                if (it.Enabled && it.Action?.Type == ActionType.Wheel && it.Action.Wheel == name)
                {
                    isSub = true;
                    return $"Sub-wheel of '{w.Title}' ({it.Label})";
                }
        return null;
    }

    public WheelsPage()
    {
        var p = Shell.Store.Profile;
        var body = new StackPanel { Margin = new Thickness(16, 12), Spacing = 6 };
        body.Children.Add(new TextBlock { Text = "Radial wheels", FontSize = 24, FontWeight = FontWeight.SemiBold });
        body.Children.Add(new TextBlock { Text = "Every wheel in this profile and what opens it. Click one to edit its items.", Classes = { "desc" } });
        body.Children.Add(new HowYouPress("wheel"));

        var list = p.Wheels.Values.Select(w => (w, opener: OpenerOf(p, w.Name, out bool sub), sub)).ToList();
        if (list.Count == 0)
            body.Children.Add(Heading("No wheels yet"));
        bool newShown = false;
        foreach (var (sub, heading) in new[] { (false, "Main wheels"), (true, "Sub-wheels") })
        {
            var these = list.Where(x => x.sub == sub).ToList();
            if (these.Count == 0)
                continue;
            body.Children.Add(Heading(heading));
            var row = new WrapPanel();
            foreach (var (w, opener, _) in these)
                row.Children.Add(sub ? Tile(w, opener) : MainTile(p, w, opener));
            if (!sub)
            { var nt = NewTile(); nt.VerticalAlignment = VerticalAlignment.Bottom; row.Children.Add(nt); newShown = true; } // level with the cards, under their switch row
            body.Children.Add(row);
        }
        if (!newShown)
            body.Children.Add(new WrapPanel { Children = { NewTile() } });
        Content = new ScrollViewer { Content = body };
    }

    static TextBlock Heading(string text) => new()
    {
        Text = text.ToUpperInvariant(), FontWeight = FontWeight.Bold, FontSize = 12, Foreground = Brush.Parse("#7FA8F0"),
        Margin = new Thickness(0, 12, 0, 4),
    };

    static List<BindingModel> Openers(ProfileModel p, string name) =>
        p.Bindings.Where(x => x.Action?.Type == ActionType.Wheel && x.Action.Wheel == name).ToList();

    // A main wheel: the card (still a button that opens the wheel) with an on/off switch and a
    // "Set the button" shortcut above it, like the wrist panels.
    static Control MainTile(ProfileModel p, WheelModel w, string opener)
    {
        var bs = Openers(p, w.Name);
        var sw = new ToggleSwitch { IsChecked = bs.Any(b => b.Enabled), IsEnabled = bs.Count > 0, OnContent = "Wheel on", OffContent = "Wheel off",
                                    FontSize = 15, FontWeight = FontWeight.SemiBold, VerticalAlignment = VerticalAlignment.Center };
        ToolTip.SetTip(sw, bs.Count > 0 ? "Switches the controller button(s) that open this wheel on or off" : "Nothing opens this wheel yet: set a button first");
        sw.IsCheckedChanged += (o, e) =>
        {
            bool on = sw.IsChecked == true;
            if (bs.Any(b => b.Enabled) == on) return; // only a real change
            foreach (var b in bs) b.Enabled = on;
            Shell.Store.MarkDirty();
            Shell.Refresh();
        };
        var big = new LayoutTransformControl { LayoutTransform = new ScaleTransform(1.5, 1.5), Child = sw, VerticalAlignment = VerticalAlignment.Center };
        var bind = new Button { Content = bs.Count > 0 ? "Button that opens it…" : "Set a button to open it…", VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(12, 0, 0, 0) };
        bind.Click += (o, e) =>
        {
            var b = bs.FirstOrDefault(x => x.Enabled) ?? bs.FirstOrDefault();
            if (b == null)
            {
                b = new BindingModel { Name = "Open " + (string.IsNullOrWhiteSpace(w.Title) ? w.Name : w.Title), Input = "right.thumbrest", Gesture = GestureKind.DoubleTap,
                                       Action = new ActionModel { Type = ActionType.Wheel, Wheel = w.Name } };
                p.Bindings.Add(b);
                Shell.Store.MarkDirty();
            }
            Shell.Go("binding:" + p.Bindings.IndexOf(b));
        };
        return new StackPanel { Margin = new Thickness(0, 0, 12, 12), Children =
        {
            new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(4, 0, 0, 4), Children = { big, bind } },
            Tile(w, opener, noMargin: true),
        } };
    }

    static Button Tile(WheelModel w, string opener, bool noMargin = false)
    {
        int on = w.Items.Count(i => i.Enabled);
        var pic = new WheelView { Wheel = w, Width = 170, Height = 170, Dimmed = opener == null, IsHitTestVisible = false };
        var tile = new Button
        {
            Classes = { "option" }, Width = 380, Margin = noMargin ? new Thickness(0) : new Thickness(0, 0, 12, 12),
            HorizontalContentAlignment = HorizontalAlignment.Left,
            Content = new StackPanel
            {
                Orientation = Orientation.Horizontal, Spacing = 14,
                Children =
                {
                    pic,
                    new StackPanel
                    {
                        Width = 170, VerticalAlignment = VerticalAlignment.Center, Spacing = 4,
                        Children =
                        {
                            new TextBlock { Text = string.IsNullOrWhiteSpace(w.Title) ? w.Name : w.Title, FontSize = 16, FontWeight = FontWeight.SemiBold, TextWrapping = TextWrapping.Wrap },
                            new TextBlock { Text = $"{on} of {w.Items.Count} items on · name '{w.Name}'", Foreground = Brush.Parse("#AAB2BF"), TextWrapping = TextWrapping.Wrap },
                            new TextBlock { Text = opener ?? "Not assigned: nothing opens it", Foreground = opener != null && !opener.StartsWith("Switched off") ? Green : Orange, TextWrapping = TextWrapping.Wrap },
                        },
                    },
                },
            },
        };
        tile.Click += (s, e) => Shell.Go("wheel:" + w.Name);
        return tile;
    }

    static Button NewTile()
    {
        var tile = new Button
        {
            Classes = { "option" }, Width = 300, MinHeight = 194, Margin = new Thickness(0, 0, 12, 12),
            Content = new StackPanel
            {
                VerticalAlignment = VerticalAlignment.Center, Spacing = 6,
                Children =
                {
                    new TextBlock { Text = "+", FontSize = 40, HorizontalAlignment = HorizontalAlignment.Center, Foreground = Brush.Parse("#7FA8F0") },
                    new TextBlock { Text = "New wheel", FontSize = 16, FontWeight = FontWeight.SemiBold, HorizontalAlignment = HorizontalAlignment.Center },
                    new TextBlock { Text = "A new main wheel. Then give it items and pick the button that opens it.", TextWrapping = TextWrapping.Wrap,
                                    TextAlignment = TextAlignment.Center, Foreground = Brush.Parse("#AAB2BF") },
                },
            },
        };
        tile.Click += (s, e) =>
        {
            var p = Shell.Store.Profile;
            string name = "wheel" + (p.Wheels.Count + 1);
            for (int i = p.Wheels.Count + 2; p.Wheels.ContainsKey(name); i++)
                name = "wheel" + i;
            p.Wheels[name] = new WheelModel { Name = name, Title = name };
            Shell.Store.MarkDirty();
            Shell.Go("wheel:" + name);
        };
        return tile;
    }
}
