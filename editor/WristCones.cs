using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

// The two cones that decide when a wrist panel shows, drawn to scale:
// left, from your eye: the panel must be within "look within" degrees of the middle of your view;
// right, from the panel: the panel's face must point at your eyes within "arm must face me" degrees.
public sealed class WristCones : Control
{
    public float GazeDeg = 20, FacingDeg = 35;

    public WristCones()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw | ControlStyles.UserPaint, true);
        Size = new Size(560, 220);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
        g.Clear(Theme.IsDark ? Color.FromArgb(32, 36, 44) : Color.FromArgb(240, 243, 248));
        float half = Width / 2f;
        DrawGaze(g, new RectangleF(0, 0, half - 6, Height));
        DrawFacing(g, new RectangleF(half + 6, 0, half - 6, Height));
    }

    static readonly Color Cone = Color.FromArgb(90, 60, 140, 255), ConeEdge = Color.FromArgb(60, 140, 255), PanelCol = Color.FromArgb(240, 150, 40);
    Color Ink => Theme.IsDark ? Color.FromArgb(220, 226, 236) : Color.FromArgb(40, 50, 70);
    Color Soft => Theme.IsDark ? Color.FromArgb(150, 158, 172) : Color.FromArgb(110, 118, 132);

    // From the eye: a cone around the line of sight. Inside it: the panel counts as looked at.
    void DrawGaze(Graphics g, RectangleF r)
    {
        float eyeX = r.X + 34, eyeY = r.Y + 78;
        float a = (float)(Math.Min(GazeDeg, 80) * Math.PI / 180);
        float len = Math.Min(r.Width - 70, 70f / (float)Math.Tan(a)); // a wide cone is drawn shorter, so it fits
        float spread = (float)Math.Tan(a) * len;
        using (var p = new GraphicsPath())
        {
            p.AddPolygon(new[] { new PointF(eyeX, eyeY), new PointF(eyeX + len, eyeY - spread), new PointF(eyeX + len, eyeY + spread) });
            using var fill = new SolidBrush(Cone);
            g.FillPath(fill, p);
            using var edge = new Pen(ConeEdge, 1.5f);
            g.DrawLine(edge, eyeX, eyeY, eyeX + len, eyeY - spread);
            g.DrawLine(edge, eyeX, eyeY, eyeX + len, eyeY + spread);
        }
        using (var mid = new Pen(Soft, 1f) { DashStyle = DashStyle.Dash })
            g.DrawLine(mid, eyeX, eyeY, eyeX + len + 10, eyeY);
        // the eye
        using (var eye = new SolidBrush(Ink))
        {
            g.FillEllipse(eye, eyeX - 13, eyeY - 8, 22, 16);
            using var pupil = new SolidBrush(Theme.IsDark ? Color.FromArgb(32, 36, 44) : Color.White);
            g.FillEllipse(pupil, eyeX - 4, eyeY - 4, 8, 8);
        }
        // a panel inside the cone (lit) and one outside it (dim)
        float px = eyeX + len * 0.75f;
        float inY = eyeY - spread * 0.75f * 0.45f, outY = eyeY + spread * 0.75f + 22;
        DrawPanel(g, px, inY, true);
        if (outY + 16 < r.Bottom - 52)
            DrawPanel(g, px, outY, false);
        Caption(g, r, $"Look within {GazeDeg:0}°", "How close to the middle of your view the panel must be. Smaller = look right at it.");
    }

    // From the panel: a cone around the way its face points. Your eyes inside it: the arm counts as turned to you.
    void DrawFacing(Graphics g, RectangleF r)
    {
        float cx = r.X + r.Width - 60, cy = r.Y + 78;
        float a = (float)(Math.Min(FacingDeg, 80) * Math.PI / 180);
        float len = Math.Min(r.Width - 110, 70f / (float)Math.Tan(a));
        float spread = (float)Math.Tan(a) * len;
        using (var p = new GraphicsPath())
        {
            p.AddPolygon(new[] { new PointF(cx, cy), new PointF(cx - len, cy - spread), new PointF(cx - len, cy + spread) });
            using var fill = new SolidBrush(Cone);
            g.FillPath(fill, p);
            using var edge = new Pen(ConeEdge, 1.5f);
            g.DrawLine(edge, cx, cy, cx - len, cy - spread);
            g.DrawLine(edge, cx, cy, cx - len, cy + spread);
        }
        // the forearm with the panel on it, facing left (toward the eye)
        using (var arm = new SolidBrush(Theme.IsDark ? Color.FromArgb(90, 98, 112) : Color.FromArgb(170, 178, 192)))
            g.FillRectangle(arm, cx + 4, cy - 46, 22, 92);
        using (var pan = new SolidBrush(PanelCol))
            g.FillRectangle(pan, cx - 3, cy - 22, 7, 44);
        using (var face = new Pen(Ink, 2f) { EndCap = LineCap.ArrowAnchor })
            g.DrawLine(face, cx - 4, cy, cx - 40, cy);
        // the eye, inside the cone
        float ex = cx - len * 0.85f, ey = cy - Math.Min(spread * 0.4f, 40);
        using (var eye = new SolidBrush(Ink))
            g.FillEllipse(eye, ex - 11, ey - 7, 20, 14);
        Caption(g, r, $"Arm faces you within {FacingDeg:0}°", "How far the panel's face may point away from your eyes. Smaller = turn the arm more.");
    }

    void DrawPanel(Graphics g, float x, float y, bool lit)
    {
        using var b = new SolidBrush(lit ? PanelCol : Color.FromArgb(90, PanelCol));
        g.FillRectangle(b, x - 4, y - 16, 8, 32);
        using var f = new Font("Segoe UI", 8f, FontStyle.Bold);
        using var t = new SolidBrush(lit ? Ink : Soft);
        g.DrawString(lit ? "shows" : "hidden", f, t, x + 8, y - 7);
    }

    void Caption(Graphics g, RectangleF r, string title, string text)
    {
        using var tf = new Font("Segoe UI", 10f, FontStyle.Bold);
        using var sf = new Font("Segoe UI", 8.5f);
        using var tb = new SolidBrush(Ink);
        using var sb = new SolidBrush(Soft);
        g.DrawString(title, tf, tb, new RectangleF(r.X + 8, r.Bottom - 50, r.Width - 16, 20));
        g.DrawString(text, sf, sb, new RectangleF(r.X + 8, r.Bottom - 31, r.Width - 16, 30));
    }
}
