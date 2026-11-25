using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;

namespace OverviewPrototype;

// A colour swatch you can click: opens Avalonia's colour picker (spectrum, sliders, hex, transparency) beside it.
// Every change in the picker goes through picked() as "#RRGGBBAA", so the owner's own checks and saving apply.
public static class ColourButton
{
    public static Button Wrap(Border swatch, Func<string> current, Action<string> picked)
    {
        var view = new ColorView
        {
            IsAlphaEnabled = true, IsAlphaVisible = true, IsColorPaletteVisible = true,
            Width = 360, Height = 420,
        };
        bool filling = false;
        view.ColorChanged += (s, e) =>
        {
            if (filling) return;
            var c = e.NewColor;
            picked($"#{c.R:X2}{c.G:X2}{c.B:X2}{c.A:X2}");
        };
        var fly = new Flyout { Content = view, Placement = PlacementMode.RightEdgeAlignedTop };
        fly.Opening += (s, e) =>
        {
            filling = true; // show the current colour without writing it back
            view.Color = WheelView.TryColor(current(), out var c) ? c : Color.Parse("#2E9E5BE0");
            filling = false;
        };
        var b = new Button { Content = swatch, Padding = new Thickness(0), Background = Brushes.Transparent, BorderThickness = new Thickness(0), Flyout = fly };
        ToolTip.SetTip(b, "Click to pick a colour");
        return b;
    }
}
