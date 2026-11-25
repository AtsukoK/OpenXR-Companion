using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// A wheel drawn as it looks in the headset: slices around a centre with the title, or, for a wrist page, its button
// grid (same layout maths as the WinForms WheelPreview / the layer's WristLayout). Click an item to pick it; on a
// wrist page drag one onto another to swap them. Drawn with Skia on the GPU, crisp at any Windows scaling.
public class WheelView : Control
{
    public static readonly StyledProperty<WheelModel> WheelProperty = AvaloniaProperty.Register<WheelView, WheelModel>(nameof(Wheel));
    public WheelModel Wheel { get => GetValue(WheelProperty); set => SetValue(WheelProperty, value); }

    WheelItemModel highlight;
    public WheelItemModel Highlight { get => highlight; set { highlight = value; InvalidateVisual(); } }
    public bool IsWristPage { get; set; }
    public int GridColumns { get; set; } = 3;
    public bool Dimmed { get; set; } // nothing opens it: drawn paler

    public event Action<WheelItemModel> ItemClicked;
    public event Action<WheelItemModel, WheelItemModel> ItemDropped;

    static WheelView() => AffectsRender<WheelView>(WheelProperty);

    public WheelView()
    {
        Cursor = new Cursor(StandardCursorType.Hand);
        Width = 320;
        Height = 320;
    }

    List<WheelItemModel> On => Wheel?.Items.Where(i => i.Enabled).ToList() ?? new();
    bool Grid => Wheel != null && IsWristPage;
    int Columns => Math.Clamp(GridColumns, 1, 4);

    static readonly IBrush Back = new SolidColorBrush(Color.FromRgb(24, 28, 34));
    static readonly IBrush Slice = new SolidColorBrush(Color.FromRgb(44, 52, 70));
    static readonly IBrush Lit = new SolidColorBrush(Color.FromRgb(60, 140, 255));
    static readonly IBrush Centre = new SolidColorBrush(Color.FromRgb(16, 16, 20));
    static readonly IBrush Accent = new SolidColorBrush(Color.FromRgb(127, 196, 255));
    static readonly Typeface Bold = new("Segoe UI", FontStyle.Normal, FontWeight.Bold);

    // Same as the WinForms GridLayout: cells in 0..1 units of the picture's side.
    internal static List<Rect> GridLayout(int n, int cols, out Rect header)
    {
        const double pad = 0.03, gap = 0.02, headH = 0.14;
        header = new Rect(pad, pad, 1 - 2 * pad, headH);
        var outp = new List<Rect>();
        if (n <= 0)
            return outp;
        cols = Math.Clamp(Math.Min(cols, n), 1, 4);
        int rows = (n + cols - 1) / cols;
        double top = pad + headH + gap, availW = 1 - 2 * pad, availH = 1 - top - pad;
        double cw = (availW - gap * (cols - 1)) / cols, ch = (availH - gap * (rows - 1)) / rows;
        double c = Math.Min(cw, ch * 1.25);
        ch = Math.Min(ch, c);
        double left = 0.5 - (c * cols + gap * (cols - 1)) / 2;
        for (int i = 0; i < n; i++)
            outp.Add(new Rect(left + (i % cols) * (c + gap), top + (i / cols) * (ch + gap), c, ch));
        return outp;
    }

    int HitIndex(Point p)
    {
        int n = On.Count;
        if (n == 0)
            return -1;
        double S = Math.Min(Bounds.Width, Bounds.Height);
        if (Grid)
            return GridLayout(n, Columns, out _).FindIndex(r => r.Contains(new Point(p.X / S, p.Y / S)));
        double c = S / 2, dx = p.X - c, dy = p.Y - c, r = Math.Sqrt(dx * dx + dy * dy), R = S / 2 - 4;
        if (r < R * 0.34 || r > R)
            return -1;
        double ang = Math.Atan2(dx, -dy) * 180 / Math.PI, seg = 360.0 / n; // 0 = up, clockwise
        return (int)(((ang + seg / 2 + 720) % 360) / seg) % n;
    }

    // ---- mouse: click picks, drag swaps (wrist pages)
    int downAt = -1, dropAt = -1;
    Point downPos;
    bool dragging;

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        base.OnPointerPressed(e);
        if (!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        downPos = e.GetPosition(this);
        downAt = HitIndex(downPos);
        dragging = false;
    }

    protected override void OnPointerMoved(PointerEventArgs e)
    {
        base.OnPointerMoved(e);
        if (downAt < 0 || ItemDropped == null)
            return;
        var p = e.GetPosition(this);
        if (!dragging && Math.Abs(p.X - downPos.X) + Math.Abs(p.Y - downPos.Y) > 8)
        { dragging = true; Cursor = new Cursor(StandardCursorType.SizeAll); }
        if (dragging)
        {
            int over = HitIndex(p);
            if (over != dropAt) { dropAt = over; InvalidateVisual(); }
        }
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        base.OnPointerReleased(e);
        var items = On;
        int from = downAt, to = dropAt;
        bool wasDrag = dragging;
        downAt = dropAt = -1;
        dragging = false;
        Cursor = new Cursor(StandardCursorType.Hand);
        InvalidateVisual();
        if (from < 0 || from >= items.Count)
            return;
        if (!wasDrag)
            ItemClicked?.Invoke(items[from]);
        else if (to >= 0 && to != from && to < items.Count)
            ItemDropped?.Invoke(items[from], items[to]);
    }

    // ---- drawing
    public override void Render(DrawingContext g)
    {
        if (Grid)
        { RenderGrid(g); return; }
        double S = Math.Min(Bounds.Width, Bounds.Height), c = S / 2, R = S / 2 - 4, r = R * 0.34;
        using var dim = g.PushOpacity(Dimmed ? 0.55 : 1);
        g.DrawEllipse(Back, null, new Point(c, c), R, R);
        var items = On;
        int n = items.Count;
        double seg = n > 0 ? 360.0 / n : 360;
        for (int i = 0; i < n; i++)
        {
            var it = items[i];
            double mid = -90 + i * seg, a0 = mid - seg / 2 + 1, sw = seg - 2;
            g.DrawGeometry(Fill(it), null, Ring(c, R - 4, r + 3, a0, sw));
            double rad = mid * Math.PI / 180, tr = (R + r) / 2;
            double boxW = Math.Min(R - r, 2 * tr * Math.Sin(Math.Min(seg, 179) * Math.PI / 360) * 0.9);
            DrawText(g, it.Label, S * 0.036, Brushes.White, new Rect(c + Math.Cos(rad) * tr - boxW / 2, c + Math.Sin(rad) * tr - 20, boxW, 40));
        }
        g.DrawEllipse(Centre, null, new Point(c, c), r, r);
        DrawText(g, n == 0 ? "(empty)" : Wheel?.Title, S * 0.04, Accent, new Rect(c - r, c - r, 2 * r, 2 * r));
    }

    void RenderGrid(DrawingContext g)
    {
        double S = Math.Min(Bounds.Width, Bounds.Height);
        using var dim = g.PushOpacity(Dimmed ? 0.55 : 1);
        var items = On;
        int n = items.Count;
        var cells = GridLayout(n, Columns, out var hd);
        double bottom = cells.Count == 0 ? hd.Bottom : Math.Max(hd.Bottom, cells.Max(x => x.Bottom));
        Rect Px(Rect x) => new(x.X * S, x.Y * S, x.Width * S, x.Height * S);
        g.DrawRectangle(Back, null, new Rect(1, 1, S - 2, (bottom + 0.03) * S - 2), S * 0.03, S * 0.03);
        DrawText(g, n == 0 ? "(empty)" : Wheel?.Title, S * 0.04, Accent, Px(hd));
        for (int i = 0; i < n; i++)
        {
            var r = Px(cells[i]);
            double rad = r.Height * 0.11;
            g.DrawRectangle(Fill(items[i]), null, r, rad, rad);
            if (i == (dragging ? dropAt : -1)) // where a dragged button will land
                g.DrawRectangle(null, new Pen(new SolidColorBrush(Color.FromRgb(255, 170, 60)), 3, DashStyle.Dash), r, rad, rad);
            DrawText(g, items[i].Label, S * 0.036, Brushes.White, r);
        }
    }

    IBrush Fill(WheelItemModel it) =>
        ReferenceEquals(it, Highlight) ? Lit : TryColor(it.Color, out var col) ? new SolidColorBrush(col) : Slice;

    // A ring slice from angle a0, sweep sw (degrees, clockwise from 3 o'clock like GDI+), between radii outer and inner.
    static Geometry Ring(double c, double outer, double inner, double a0, double sw)
    {
        Point At(double rr, double a) => new(c + rr * Math.Cos(a * Math.PI / 180), c + rr * Math.Sin(a * Math.PI / 180));
        var geo = new StreamGeometry();
        using (var ctx = geo.Open())
        {
            bool large = sw > 180;
            ctx.BeginFigure(At(outer, a0), true);
            ctx.ArcTo(At(outer, a0 + sw), new Size(outer, outer), 0, large, SweepDirection.Clockwise);
            ctx.LineTo(At(inner, a0 + sw));
            ctx.ArcTo(At(inner, a0), new Size(inner, inner), 0, large, SweepDirection.CounterClockwise);
            ctx.EndFigure(true);
        }
        return geo;
    }

    // Centred, wrapped text in a box (cut with … when it doesn't fit).
    static void DrawText(DrawingContext g, string text, double size, IBrush brush, Rect box)
    {
        if (string.IsNullOrEmpty(text) || box.Width <= 2)
            return;
        using var layout = new Avalonia.Media.TextFormatting.TextLayout(text, Bold, Math.Max(7, size), brush, TextAlignment.Center,
            TextWrapping.Wrap, TextTrimming.CharacterEllipsis, maxWidth: box.Width, maxHeight: box.Height);
        layout.Draw(g, new Point(box.X, box.Y + (box.Height - layout.Height) / 2));
    }

    public static bool TryColor(string s, out Color c)
    {
        c = default;
        if (string.IsNullOrWhiteSpace(s))
            return false;
        s = s.TrimStart('#');
        if (s.Length != 6 && s.Length != 8)
            return false;
        try
        {
            uint v = Convert.ToUInt32(s, 16);
            c = s.Length == 6 ? Color.FromRgb((byte)(v >> 16), (byte)(v >> 8), (byte)v)
                              : Color.FromArgb((byte)v, (byte)(v >> 24), (byte)(v >> 16), (byte)(v >> 8));
            return true;
        }
        catch (FormatException) { return false; }
        catch (OverflowException) { return false; }
    }
}
