using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.TextFormatting;

namespace OverviewPrototype;

// One stick's options to scale (port of the WinForms StickZonesPreview): the circle is the full range, grey middle =
// inner deadzone (no push), orange edge band = outer deadzone (already full), blue wedges = where Up / Down / Left /
// Right count for bindings (past the press point, and within the direction zone's angle when it is set).
public class StickZonesView : Control
{
    float inner, outer, angle, stickOn = 0.7f;
    bool ignoreOutside;
    float ignoreCenter;

    public StickZonesView()
    {
        Width = 220;
        Height = 250;
    }

    public void ShowZones(float innerDeadzone, float outerDeadzone, float angleDeg, float directionAt, bool ignoreOutsideZones = false, float ignoreCenterShare = 0)
    {
        ignoreCenter = ignoreCenterShare;
        ignoreOutside = ignoreOutsideZones && angleDeg > 0;
        inner = innerDeadzone;
        outer = outerDeadzone;
        angle = angleDeg;
        stickOn = directionAt;
        InvalidateVisual();
    }

    static readonly Color Accent = Color.FromRgb(60, 140, 255), Full = Color.FromRgb(240, 140, 30);
    static readonly Color Back = Color.FromRgb(46, 50, 58), Grey = Color.FromRgb(135, 142, 156), TextC = Color.FromRgb(230, 233, 238), SubC = Color.FromRgb(160, 168, 182);

    public override void Render(DrawingContext g)
    {
        double w = Bounds.Width, size = Math.Min(w, Bounds.Height - 34), r = size / 2 - 4;
        var c = new Point(w / 2, r + 6);
        g.DrawEllipse(new SolidColorBrush(Back), null, c, r, r);
        // where each direction counts
        double half = angle > 0 ? angle : 45, from = r * stickOn;
        var fill = new SolidColorBrush(Color.FromArgb(70, Accent.R, Accent.G, Accent.B));
        var edge = new Pen(new SolidColorBrush(Color.FromArgb(150, Accent.R, Accent.G, Accent.B)), 1);
        if (ignoreOutside && half < 45) // between the zones: the game gets no push there
            foreach (double axis in new[] { -45.0, 45, 135, 225 })
                g.DrawGeometry(new SolidColorBrush(Color.FromRgb(92, 98, 110)), null, Wedge(c, r, 0.001, axis - (45 - half), 2 * (45 - half)));
        if (ignoreOutside && ignoreCenter > 0) // the centre too: small pushes give the game nothing
            g.DrawEllipse(new SolidColorBrush(Color.FromRgb(92, 98, 110)), null, c, r * ignoreCenter, r * ignoreCenter); // the same solid grey
        foreach (double axis in new[] { -90.0, 0, 90, 180 }) // up, right, down, left (screen angles)
            g.DrawGeometry(fill, edge, Wedge(c, r, from, axis - half, 2 * half));
        if (outer > 0) // the band that already counts as full
            g.DrawGeometry(new SolidColorBrush(Color.FromArgb(110, Full.R, Full.G, Full.B)), null,
                new GeometryGroup { FillRule = FillRule.EvenOdd, Children = { new EllipseGeometry(new Rect(c.X - r, c.Y - r, 2 * r, 2 * r)),
                    new EllipseGeometry(new Rect(c.X - r * (1 - outer), c.Y - r * (1 - outer), 2 * r * (1 - outer), 2 * r * (1 - outer))) } });
        if (inner > 0)
            g.DrawEllipse(new SolidColorBrush(Grey), new Pen(new SolidColorBrush(TextC), 1.2, DashStyle.Dash), c, r * inner, r * inner);
        g.DrawEllipse(null, new Pen(new SolidColorBrush(SubC), 1.2), c, r, r);
        g.DrawEllipse(new SolidColorBrush(TextC), null, c, 2.5, 2.5);
        string zones = angle > 0 ? $"blue: Up / Down / Left / Right (±{angle:0}°)" : "blue: where a direction counts";
        if (ignoreOutside) zones += "\ngrey: ignored (no push for the game)";
        string dead = (inner > 0 ? $"grey: no push ({inner * 100:0}%)" : "") + (inner > 0 && outer > 0 ? "   " : "") + (outer > 0 ? $"orange: full ({outer * 100:0}%)" : "");
        using var legend = new TextLayout(zones + (dead.Length > 0 ? "\n" + dead : ""), new Typeface("Segoe UI"), 10, new SolidColorBrush(SubC), TextAlignment.Center, maxWidth: w);
        legend.Draw(g, new Point(0, c.Y + r + 6));
    }

    // A ring slice between radius r (outside) and from (inside), from angle a0 sweeping sw degrees, clockwise.
    static Geometry Wedge(Point c, double r, double from, double a0, double sw)
    {
        Point At(double rr, double a) => new(c.X + rr * Math.Cos(a * Math.PI / 180), c.Y + rr * Math.Sin(a * Math.PI / 180));
        var geo = new StreamGeometry();
        using (var ctx = geo.Open())
        {
            bool large = sw > 180;
            ctx.BeginFigure(At(r, a0), true);
            ctx.ArcTo(At(r, a0 + sw), new Size(r, r), 0, large, SweepDirection.Clockwise);
            ctx.LineTo(At(from, a0 + sw));
            ctx.ArcTo(At(from, a0), new Size(from, from), 0, large, SweepDirection.CounterClockwise);
            ctx.EndFigure(true);
        }
        return geo;
    }
}
