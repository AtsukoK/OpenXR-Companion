using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Shapes;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// A drawn Xbox 360-style pad: click a button, trigger, bumper, d-pad arrow or a stick direction to pick it.
// Our own simple drawing. Returns the pad button code ("a", "lt", "ls_up"...), or null when closed.
public static class GamepadPicker
{
    static readonly IBrush Face = Brush.Parse("#2A2F37"), Now = Brush.Parse("#4A3A6A");

    public static async Task<string> Show(Visual owner, string current)
    {
        string result = null;
        var win = new Window
        {
            Title = "Pick a gamepad button", Width = 820, Height = 600, WindowStartupLocation = WindowStartupLocation.CenterOwner,
            Background = Brush.Parse("#1B1E23"),
        };
        var c = new Canvas { Width = 760, Height = 430 };
        // the body
        c.Children.Add(Place(new Border { Width = 640, Height = 300, CornerRadius = new CornerRadius(140, 140, 170, 170),
                                          Background = Brush.Parse("#23272E"), BorderBrush = Brush.Parse("#4A5260"),
                                          BorderThickness = new Thickness(2) }, 60, 90));

        void Add(string code, string label, double x, double y, double w = 46, double h = 46, double round = 23, IBrush colour = null)
        {
            var b = new Button
            {
                Content = label, Width = w, Height = h, CornerRadius = new CornerRadius(round), Padding = new Thickness(0),
                HorizontalContentAlignment = HorizontalAlignment.Center, VerticalContentAlignment = VerticalAlignment.Center,
                Background = code == current ? Now : Face, FontWeight = FontWeight.SemiBold,
                Foreground = colour ?? Brushes.White, FontSize = label.Length > 2 ? 12 : 15,
            };
            ToolTip.SetTip(b, Choices.PadName(code));
            b.Click += (s, e) => { result = code; win.Close(); };
            c.Children.Add(Place(b, x - w / 2, y - h / 2));
        }

        // A stick: the click in the middle, a direction on each side.
        void Stick(string p, double x, double y)
        {
            c.Children.Add(Place(new Ellipse { Width = 130, Height = 130, Stroke = Brush.Parse("#4A5260"), StrokeThickness = 2 }, x - 65, y - 65));
            Add(p, p.ToUpperInvariant(), x, y, 50, 50, 25);
            Add(p + "_up", "▲", x, y - 45, 38, 30, 8);
            Add(p + "_down", "▼", x, y + 45, 38, 30, 8);
            Add(p + "_left", "◀", x - 45, y, 30, 38, 8);
            Add(p + "_right", "▶", x + 45, y, 30, 38, 8);
        }

        Add("lt", "LT", 170, 28, 110, 40, 10);
        Add("rt", "RT", 590, 28, 110, 40, 10);
        Add("lb", "LB", 170, 76, 150, 34, 12);
        Add("rb", "RB", 590, 76, 150, 34, 12);
        Stick("ls", 190, 190);
        Stick("rs", 480, 300);
        // d-pad
        Add("dpad_up", "▲", 280, 262, 40, 40, 6);
        Add("dpad_down", "▼", 280, 342, 40, 40, 6);
        Add("dpad_left", "◀", 240, 302, 40, 40, 6);
        Add("dpad_right", "▶", 320, 302, 40, 40, 6);
        // face buttons
        Add("y", "Y", 575, 145, 48, 48, 24, Brush.Parse("#F2C94C"));
        Add("a", "A", 575, 235, 48, 48, 24, Brush.Parse("#6FCF73"));
        Add("x", "X", 530, 190, 48, 48, 24, Brush.Parse("#5DA9F0"));
        Add("b", "B", 620, 190, 48, 48, 24, Brush.Parse("#EB6A6A"));
        // middle
        Add("back", "Back", 315, 190, 58, 30, 15);
        Add("guide", "Guide", 380, 145, 56, 56, 28);
        Add("start", "Start", 445, 190, 58, 30, 15);

        var cancel = new Button { Content = "Cancel" };
        cancel.Click += (s, e) => win.Close();
        var body = new StackPanel { Margin = new Thickness(20), Spacing = 8 };
        body.Children.Add(new TextBlock { Text = "Pick a gamepad button", FontSize = 22, FontWeight = FontWeight.SemiBold });
        body.Children.Add(new TextBlock { Text = "Click a button. The arrows around a stick push that stick all the way. Purple = the button it has now.",
                                          Classes = { "desc" }, TextWrapping = TextWrapping.Wrap });
        body.Children.Add(c);
        body.Children.Add(cancel);
        win.Content = new ScrollViewer { Content = body };
        if (TopLevel.GetTopLevel(owner) is Window parent)
            await win.ShowDialog(parent);
        return result;
    }

    static Control Place(Control ctl, double x, double y)
    {
        Canvas.SetLeft(ctl, x);
        Canvas.SetTop(ctl, y);
        return ctl;
    }
}
