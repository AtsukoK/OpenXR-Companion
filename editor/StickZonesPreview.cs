using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

// A picture of one stick's options, to scale: the whole circle is the stick's full range. The grey middle is the
// inner deadzone (no push), the band at the edge the outer deadzone (already full), the wedges where Up / Down /
// Left / Right count for bindings (with no direction zone: everything past the press point counts).
public class StickZonesPreview : Control
{
    float inner, outer, angle;
    float stickOn = 0.7f; // where a direction counts as pushed (the profile's threshold)

    public StickZonesPreview()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw | ControlStyles.UserPaint, true);
        Size = new Size(220, 220);
    }

    // inner / outer: share of a full push (0 .. 0.9); angleDeg: direction zone, 0 = off; directionAt: the threshold.
    public void ShowZones(float innerDeadzone, float outerDeadzone, float angleDeg, float directionAt)
    {
        inner = innerDeadzone;
        outer = outerDeadzone;
        angle = angleDeg;
        stickOn = directionAt;
        Invalidate();
    }

    static Color Accent => Color.FromArgb(60, 140, 255);
    static Color Full => Color.FromArgb(240, 140, 30);

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAliasGridFit;
        g.Clear(BackColor);
        float size = Math.Min(Width, Height - 34), r = size / 2 - 4;
        var c = new PointF(Width / 2f, r + 6);
        RectangleF Circle(float radius) => new(c.X - radius, c.Y - radius, radius * 2, radius * 2);

        using (var b = new SolidBrush(Theme.IsDark ? Color.FromArgb(46, 50, 58) : Color.FromArgb(236, 239, 244)))
            g.FillEllipse(b, Circle(r));
        DrawDirectionWedges(g, c, r);
        if (outer > 0) // the band that already counts as full
            using (var band = new GraphicsPath { FillMode = FillMode.Alternate })
            {
                band.AddEllipse(Circle(r));
                band.AddEllipse(Circle(r * (1 - outer)));
                using var b = new SolidBrush(Color.FromArgb(110, Full));
                g.FillPath(b, band);
            }
        if (inner > 0)
        {
            using var b = new SolidBrush(Theme.IsDark ? Color.FromArgb(135, 142, 156) : Color.FromArgb(150, 156, 170));
            using var outline = new Pen(Theme.Text, 1.2f) { DashStyle = DashStyle.Dash };
            g.FillEllipse(b, Circle(r * inner));
            g.DrawEllipse(outline, Circle(r * inner));
        }
        using (var edge = new Pen(Theme.SubText, 1.2f))
            g.DrawEllipse(edge, Circle(r));
        using (var dot = new SolidBrush(Theme.Text))
            g.FillEllipse(dot, Circle(2.5f));
        DrawLegend(g, new RectangleF(0, c.Y + r + 6, Width, 30));
    }

    // Where each direction counts: past the press point, and (with a zone) within the angle of its axis.
    void DrawDirectionWedges(Graphics g, PointF c, float r)
    {
        float half = angle > 0 ? angle : 45;
        float from = r * stickOn;
        using var fill = new SolidBrush(Color.FromArgb(Theme.IsDark ? 70 : 55, Accent));
        using var edge = new Pen(Color.FromArgb(150, Accent), 1f);
        foreach (float axis in new[] { -90f, 0f, 90f, 180f }) // up, right, down, left (screen angles)
        {
            using var wedge = new GraphicsPath();
            wedge.AddArc(c.X - r, c.Y - r, 2 * r, 2 * r, axis - half, 2 * half);
            wedge.AddArc(c.X - from, c.Y - from, 2 * from, 2 * from, axis + half, -2 * half);
            wedge.CloseFigure();
            g.FillPath(fill, wedge);
            g.DrawPath(edge, wedge);
        }
    }

    void DrawLegend(Graphics g, RectangleF box)
    {
        using var f = new Font("Segoe UI", 7.5f);
        using var text = new SolidBrush(Theme.SubText);
        var center = new StringFormat { Alignment = StringAlignment.Center };
        string zones = angle > 0 ? $"blue: Up / Down / Left / Right (±{angle:0}°)" : "blue: where a direction counts";
        string dead = (inner > 0 ? $"grey: no push ({inner * 100:0}%)" : "") + (inner > 0 && outer > 0 ? "   " : "") +
                      (outer > 0 ? $"orange: full ({outer * 100:0}%)" : "");
        g.DrawString(zones + (dead.Length > 0 ? "\n" + dead : ""), f, text, box, center);
    }
}
