using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.TextFormatting;

namespace OverviewPrototype;

// The two cones that decide when a wrist panel shows, to scale (port of the WinForms WristCones): left, from your eye,
// the panel must be within "look within" degrees of the middle of your view; right, from the panel, its face must point
// at your eyes within "arm must face me" degrees.
public class ConesView : Control
{
    float gaze = 20, facing = 35;
    public float GazeDeg { get => gaze; set { gaze = value; InvalidateVisual(); } }
    public float FacingDeg { get => facing; set { facing = value; InvalidateVisual(); } }

    public ConesView()
    {
        Width = 560;
        Height = 220;
    }

    static readonly Color BackC = Color.FromRgb(32, 36, 44), ConeC = Color.FromArgb(90, 60, 140, 255), Edge = Color.FromRgb(60, 140, 255),
                          PanelC = Color.FromRgb(240, 150, 40), Ink = Color.FromRgb(220, 226, 236), Soft = Color.FromRgb(150, 158, 172), ArmC = Color.FromRgb(90, 98, 112);
    static readonly Typeface Ui = new("Segoe UI"), UiBold = new("Segoe UI", FontStyle.Normal, FontWeight.Bold);
    static IBrush B(Color c) => new SolidColorBrush(c);

    public override void Render(DrawingContext g)
    {
        g.DrawRectangle(B(BackC), null, new Rect(Bounds.Size), 6, 6);
        double half = Bounds.Width / 2;
        DrawGaze(g, new Rect(0, 0, half - 6, Bounds.Height));
        DrawFacing(g, new Rect(half + 6, 0, half - 6, Bounds.Height));
    }

    static Geometry Triangle(Point a, Point b, Point c)
    {
        var geo = new StreamGeometry();
        using (var ctx = geo.Open())
        {
            ctx.BeginFigure(a, true);
            ctx.LineTo(b);
            ctx.LineTo(c);
            ctx.EndFigure(true);
        }
        return geo;
    }

    void DrawGaze(DrawingContext g, Rect r)
    {
        double eyeX = r.X + 34, eyeY = r.Y + 78, a = Math.Min(gaze, 80) * Math.PI / 180;
        double len = Math.Min(r.Width - 70, 70 / Math.Tan(a)), spread = Math.Tan(a) * len; // a wide cone is drawn shorter
        g.DrawGeometry(B(ConeC), null, Triangle(new(eyeX, eyeY), new(eyeX + len, eyeY - spread), new(eyeX + len, eyeY + spread)));
        var edge = new Pen(B(Edge), 1.5);
        g.DrawLine(edge, new(eyeX, eyeY), new(eyeX + len, eyeY - spread));
        g.DrawLine(edge, new(eyeX, eyeY), new(eyeX + len, eyeY + spread));
        g.DrawLine(new Pen(B(Soft), 1, DashStyle.Dash), new(eyeX, eyeY), new(eyeX + len + 10, eyeY));
        g.DrawEllipse(B(Ink), null, new Point(eyeX - 2, eyeY), 11, 8); // the eye
        g.DrawEllipse(B(BackC), null, new Point(eyeX, eyeY), 4, 4);
        double px = eyeX + len * 0.75, inY = eyeY - spread * 0.75 * 0.45, outY = eyeY + spread * 0.75 + 22;
        DrawPanel(g, px, inY, true);
        if (outY + 16 < r.Bottom - 52)
            DrawPanel(g, px, outY, false);
        Caption(g, r, $"Look within {gaze:0}°", "How close to the middle of your view the panel must be. Smaller = look right at it.");
    }

    void DrawFacing(DrawingContext g, Rect r)
    {
        double cx = r.X + r.Width - 60, cy = r.Y + 78, a = Math.Min(facing, 80) * Math.PI / 180;
        double len = Math.Min(r.Width - 110, 70 / Math.Tan(a)), spread = Math.Tan(a) * len;
        g.DrawGeometry(B(ConeC), null, Triangle(new(cx, cy), new(cx - len, cy - spread), new(cx - len, cy + spread)));
        var edge = new Pen(B(Edge), 1.5);
        g.DrawLine(edge, new(cx, cy), new(cx - len, cy - spread));
        g.DrawLine(edge, new(cx, cy), new(cx - len, cy + spread));
        g.DrawRectangle(B(ArmC), null, new Rect(cx + 4, cy - 46, 22, 92)); // the forearm, the panel on it facing the eye
        g.DrawRectangle(B(PanelC), null, new Rect(cx - 3, cy - 22, 7, 44));
        var arrow = new Pen(B(Ink), 2);
        g.DrawLine(arrow, new(cx - 4, cy), new(cx - 40, cy));
        g.DrawLine(arrow, new(cx - 40, cy), new(cx - 33, cy - 5));
        g.DrawLine(arrow, new(cx - 40, cy), new(cx - 33, cy + 5));
        double ex = cx - len * 0.85, ey = cy - Math.Min(spread * 0.4, 40);
        g.DrawEllipse(B(Ink), null, new Point(ex - 1, ey), 10, 7); // the eye, inside the cone
        Caption(g, r, $"Arm faces you within {facing:0}°", "How far the panel's face may point away from your eyes. Smaller = turn the arm more.");
    }

    static void DrawPanel(DrawingContext g, double x, double y, bool lit)
    {
        g.DrawRectangle(B(lit ? PanelC : Color.FromArgb(90, PanelC.R, PanelC.G, PanelC.B)), null, new Rect(x - 4, y - 16, 8, 32));
        using var t = new TextLayout(lit ? "shows" : "hidden", UiBold, 10.5, B(lit ? Ink : Soft));
        t.Draw(g, new Point(x + 8, y - 8));
    }

    static void Caption(DrawingContext g, Rect r, string title, string text)
    {
        using var t = new TextLayout(title, UiBold, 13, B(Ink), maxWidth: r.Width - 16);
        t.Draw(g, new Point(r.X + 8, r.Bottom - 50));
        using var s = new TextLayout(text, Ui, 11, B(Soft), textWrapping: TextWrapping.Wrap, maxWidth: r.Width - 16);
        s.Draw(g, new Point(r.X + 8, r.Bottom - 31));
    }
}
