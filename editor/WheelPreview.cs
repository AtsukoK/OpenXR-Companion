using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

// ------------------------------------------------------------ wheel + item editors
public class WheelPreview : Control
{
    public WheelModel Wheel;
    public WheelItemModel Highlight;
    public event Action<WheelItemModel> ItemClicked;
    List<WheelItemModel> On => Wheel?.Items.Where(i => i.Enabled).ToList() ?? new();

    public WheelPreview() { DoubleBuffered = true; Size = new Size(320, 320); Cursor = Cursors.Hand; }

    // The wheel the wrist panel uses is previewed as its button grid (same layout maths as the layer's WristLayout).
    public bool IsWristPage;
    bool Grid => Wheel != null && IsWristPage;
    public int GridColumns = 3;
    int Columns => Math.Clamp(GridColumns, 1, 4);
    internal static List<RectangleF> GridLayout(int n, int cols, out RectangleF header)
    {
        const float pad = 0.03f, gap = 0.02f, headH = 0.14f;
        header = new RectangleF(pad, pad, 1 - 2 * pad, headH);
        var outp = new List<RectangleF>();
        if (n <= 0)
            return outp;
        cols = Math.Clamp(Math.Min(cols, n), 1, 4);
        int rows = (n + cols - 1) / cols;
        float top = pad + headH + gap, availW = 1 - 2 * pad, availH = 1 - top - pad;
        float cw = (availW - gap * (cols - 1)) / cols, ch = (availH - gap * (rows - 1)) / rows;
        float c = Math.Min(cw, ch * 1.25f);
        ch = Math.Min(ch, c);
        float left = 0.5f - (c * cols + gap * (cols - 1)) / 2;
        for (int i = 0; i < n; i++)
            outp.Add(new RectangleF(left + (i % cols) * (c + gap), top + (i / cols) * (ch + gap), c, ch));
        return outp;
    }

    int HitIndex(Point p)
    {
        var items = On;
        int n = items.Count;
        if (n == 0)
            return -1;
        if (Grid)
        {
            float S0 = Math.Min(Width, Height);
            float u = p.X / S0, v = p.Y / S0;
            var cells = GridLayout(n, Columns, out _);
            return cells.FindIndex(r => r.Contains(u, v));
        }
        float cx = Width / 2f, cy = Height / 2f, dx = p.X - cx, dy = p.Y - cy;
        float r = MathF.Sqrt(dx * dx + dy * dy), R = Math.Min(Width, Height) / 2f - 4;
        if (r < R * 0.34f || r > R)
            return -1;
        float ang = MathF.Atan2(dx, -dy) * 180f / MathF.PI; // 0 = up, clockwise
        float seg = 360f / n;
        return (int)(((ang + seg / 2 + 720) % 360) / seg) % n;
    }
    // Drag a button onto another to swap them (wrist pages); a click without dragging picks it.
    public event Action<WheelItemModel, WheelItemModel> ItemDropped;
    int downAt = -1, dropAt = -1;
    Point downPos;
    bool dragging;
    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (e.Button != MouseButtons.Left) return;
        downAt = HitIndex(e.Location);
        downPos = e.Location;
        dragging = false;
    }
    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        if (downAt < 0 || e.Button != MouseButtons.Left || ItemDropped == null) return;
        if (!dragging && Math.Abs(e.X - downPos.X) + Math.Abs(e.Y - downPos.Y) > 8)
        { dragging = true; Cursor = Cursors.SizeAll; }
        if (dragging)
        {
            int over = HitIndex(e.Location);
            if (over != dropAt) { dropAt = over; Invalidate(); }
        }
    }
    protected override void OnMouseUp(MouseEventArgs e)
    {
        base.OnMouseUp(e);
        var items = On;
        int from = downAt, to = dropAt;
        bool wasDrag = dragging;
        downAt = dropAt = -1; dragging = false; Cursor = Cursors.Hand; Invalidate();
        if (from < 0 || from >= items.Count) return;
        if (!wasDrag)
            ItemClicked?.Invoke(items[from]);
        else if (to >= 0 && to != from && to < items.Count)
            ItemDropped?.Invoke(items[from], items[to]);
    }
    internal int DropTarget => dragging ? dropAt : -1;

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
        g.Clear(BackColor);
        if (Grid)
        { PaintGrid(g); return; }
        float S = Math.Min(Width, Height), c = S / 2f, R = S / 2f - 4, r = R * 0.34f;
        using var bg = new SolidBrush(Color.FromArgb(255, 24, 28, 34));
        g.FillEllipse(bg, c - R, c - R, 2 * R, 2 * R);
        var items = On;
        int n = items.Count;
        float seg = n > 0 ? 360f / n : 360f;
        using var segB = new SolidBrush(Color.FromArgb(255, 44, 52, 70));
        using var hovB = new SolidBrush(Color.FromArgb(255, 60, 140, 255));
        using var font = new Font("Segoe UI", 8.5f, FontStyle.Bold);
        var fmt = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center };
        for (int i = 0; i < n; i++)
        {
            var it = items[i];
            float mid = -90 + i * seg, a0 = mid - seg / 2 + 1, sw = seg - 2;
            using var path = new GraphicsPath();
            path.AddArc(c - R + 4, c - R + 4, 2 * (R - 4), 2 * (R - 4), a0, sw);
            path.AddArc(c - r - 3, c - r - 3, 2 * (r + 3), 2 * (r + 3), a0 + sw, -sw);
            path.CloseFigure();
            Brush br = ReferenceEquals(it, Highlight) ? hovB : segB;
            SolidBrush custom = null;
            if (!ReferenceEquals(it, Highlight) && TryColor(it.Color, out var col))
                br = custom = new SolidBrush(col);
            g.FillPath(br, path);
            custom?.Dispose();
            float rad = mid * MathF.PI / 180, tr = (R + r) / 2;
            float boxW = Math.Min(R - r, 2 * tr * MathF.Sin(Math.Min(seg, 179) * MathF.PI / 360) * 0.9f);
            g.DrawString(it.Label, font, Brushes.White, new RectangleF(c + MathF.Cos(rad) * tr - boxW / 2, c + MathF.Sin(rad) * tr - 20, boxW, 40), fmt);
        }
        using var cb = new SolidBrush(Color.FromArgb(255, 16, 16, 20));
        g.FillEllipse(cb, c - r, c - r, 2 * r, 2 * r);
        using var tf = new Font("Segoe UI", 9.5f, FontStyle.Bold);
        using var accent = new SolidBrush(Color.FromArgb(255, 127, 196, 255));
        g.DrawString(n == 0 ? "(empty)" : Wheel.Title, tf, accent, new RectangleF(c - r, c - r, 2 * r, 2 * r), fmt);
    }

    void PaintGrid(Graphics g)
    {
        float S = Math.Min(Width, Height);
        var items = On;
        int n = items.Count;
        var cells = GridLayout(n, Columns, out var hd);
        float bottom = hd.Bottom;
        foreach (var r in cells)
            bottom = Math.Max(bottom, r.Bottom);
        RectangleF Px(RectangleF r) => new(r.X * S, r.Y * S, r.Width * S, r.Height * S);
        static GraphicsPath Round(RectangleF r, float rad)
        {
            var gp = new GraphicsPath();
            gp.AddArc(r.X, r.Y, rad, rad, 180, 90);
            gp.AddArc(r.Right - rad, r.Y, rad, rad, 270, 90);
            gp.AddArc(r.Right - rad, r.Bottom - rad, rad, rad, 0, 90);
            gp.AddArc(r.X, r.Bottom - rad, rad, rad, 90, 90);
            gp.CloseFigure();
            return gp;
        }
        using (var bgp = Round(new RectangleF(1, 1, S - 2, (bottom + 0.03f) * S - 2), S * 0.06f))
        using (var bg = new SolidBrush(Color.FromArgb(255, 24, 28, 34)))
            g.FillPath(bg, bgp);
        var fmt = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center };
        using var tf = new Font("Segoe UI", 9.5f, FontStyle.Bold);
        using var accent = new SolidBrush(Color.FromArgb(255, 127, 196, 255));
        g.DrawString(n == 0 ? "(empty)" : Wheel.Title, tf, accent, Px(hd), fmt);
        using var font = new Font("Segoe UI", 8.5f, FontStyle.Bold);
        using var segB = new SolidBrush(Color.FromArgb(255, 44, 52, 70));
        using var hovB = new SolidBrush(Color.FromArgb(255, 60, 140, 255));
        for (int i = 0; i < n; i++)
        {
            var it = items[i];
            var r = Px(cells[i]);
            using var gp = Round(r, r.Height * 0.22f);
            if (i == DropTarget) // where a dragged button will land
            { using var dp = new Pen(Color.FromArgb(255, 255, 170, 60), 3) { DashStyle = DashStyle.Dash }; g.DrawPath(dp, gp); }
            Brush br = ReferenceEquals(it, Highlight) ? hovB : segB;
            SolidBrush custom = null;
            if (!ReferenceEquals(it, Highlight) && TryColor(it.Color, out var col))
                br = custom = new SolidBrush(col);
            g.FillPath(br, gp);
            custom?.Dispose();
            g.DrawString(it.Label, font, Brushes.White, r, fmt);
        }
    }

    public static bool TryColor(string s, out Color c)
    {
        c = Color.Empty;
        if (string.IsNullOrWhiteSpace(s))
            return false;
        s = s.TrimStart('#');
        if (s.Length != 6 && s.Length != 8)
            return false;
        try
        {
            uint v = Convert.ToUInt32(s, 16);
            c = s.Length == 6 ? Color.FromArgb(255, (int)(v >> 16) & 255, (int)(v >> 8) & 255, (int)v & 255)
                              : Color.FromArgb((int)v & 255, (int)(v >> 24) & 255, (int)(v >> 16) & 255, (int)(v >> 8) & 255);
            return true;
        }
        catch { return false; }
    }
}
