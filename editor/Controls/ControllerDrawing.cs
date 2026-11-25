using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.TextFormatting;
using OpenXRCompanionEditor;
using RectF = System.Drawing.RectangleF;

namespace OverviewPrototype;

// One controller (a ControllerLayout, for one hand) from above and from the side, with a tag beside each part listing
// what it does, like SteamVR's binding screen. Click a part or its tag to pick it; the "+" on a tag adds to that part.
// Port of the WinForms ControllerPicture: same canvas, columns, tag growth (T) and traced outlines, drawn with Skia.
public class ControllerDrawing : Control
{
    // ---- canvas, in design units (scaled to fit the control)
    const float Gap = 10, CanvasMargin = 14;
    const int MaxLines = 8;
    float T = 1; // tag scale: tags grow into spare width when the height is what limits the picture
    const float MaxTagScale = 1.8f;
    float LabelWidth => 158 * T;
    float TagGap => 5 * T;
    float NameHeight => 19 * T;
    float LineHeight => 14.5f * T;
    float MinTagHeight => 38 * T;
    float TopX => LabelWidth;
    float MiddleX => TopX + ControllerLayouts.TopWidth + Gap;
    float SideX => MiddleX + LabelWidth + Gap;
    float CanvasWidth => SideX + ControllerLayouts.SideWidth + LabelWidth;
    const float ViewsHeight = ControllerLayouts.Height + 2 * CanvasMargin;

    ControllerLayout layout = ControllerLayouts.All[0];
    Silhouette silhouette;
    int hand = 1;
    string selected, hovered;
    bool addHovered;
    Func<ControllerPart, PartUse> useOf = _ => default;
    Func<ControllerPart, IReadOnlyList<TagLine>> linesOf = _ => Array.Empty<TagLine>();
    readonly List<(ControllerPart part, Geometry shape, RectF tag)> hits = new();

    public event Action<ControllerPart> PartClicked;
    public event Action<ControllerPart> AddClicked;
    public bool AddButtons { get; set; }
    public bool Stacked { get; set; } // the side view under the top view (tall pictures)

    float CanvasW => Stacked ? MiddleX + LabelWidth : CanvasWidth;
    float ViewsH => Stacked ? 2 * ViewsHeight : ViewsHeight;
    float ViewX(ControllerView view) => view == ControllerView.Top ? TopX
                                      : Stacked ? TopX + (ControllerLayouts.TopWidth - ControllerLayouts.SideWidth) / 2 : SideX;
    float ViewY(ControllerView view) => Stacked && view == ControllerView.Side ? ViewsHeight : 0;
    RectF AddBox(RectF tag) => new(tag.Right - 22 * T, tag.Y + 3 * T, 18 * T, 15 * T);

    public ControllerDrawing() => Cursor = new Cursor(StandardCursorType.Hand);

    public void ShowController(ControllerLayout newLayout, int newHand, string selectedComp, Func<ControllerPart, PartUse> use,
                               Func<ControllerPart, IReadOnlyList<TagLine>> lines = null)
    {
        layout = newLayout;
        hand = newHand;
        silhouette = ControllerSilhouettes.For(layout.Id, hand);
        selected = selectedComp;
        useOf = use;
        linesOf = lines ?? (_ => Array.Empty<TagLine>());
        InvalidateMeasure();
        InvalidateVisual();
    }

    public string Selected { get => selected; set { selected = value; InvalidateVisual(); } }

    // As tall as the picture needs at the width it gets (at the design tag size).
    protected override Size MeasureOverride(Size available)
    {
        double w = double.IsInfinity(available.Width) ? CanvasW : available.Width;
        float keep = T;
        T = 1;
        double h = w * CanvasHeight() / CanvasW;
        T = keep;
        return new Size(w, double.IsInfinity(available.Height) ? h : Math.Min(h, available.Height));
    }

    // The largest tag size whose canvas still fits the control's shape.
    void FitTags()
    {
        T = 1;
        if (Bounds.Width <= 0 || Bounds.Height <= 0)
            return;
        double aspect = Bounds.Width / Bounds.Height;
        for (float t = 1.05f; t <= MaxTagScale + 0.001f; t += 0.05f)
        {
            T = t;
            if (CanvasW / CanvasHeight() > aspect)
            { T = t - 0.05f; break; }
        }
    }

    float CanvasHeight()
    {
        var tags = TagBoxes(ShownParts());
        return Math.Max(ViewsH, tags.Count == 0 ? 0 : tags.Values.Max(t => t.Bottom) + CanvasMargin);
    }

    Matrix CanvasToScreen()
    {
        double s = Math.Min(Bounds.Width / CanvasW, Bounds.Height / CanvasHeight());
        return Matrix.CreateScale(s, s) * Matrix.CreateTranslation((Bounds.Width - CanvasW * s) / 2, 0);
    }

    // ---- geometry (same as WinForms)
    RectF Place(ControllerView view, RectF box)
    {
        float viewX = ViewX(view);
        float viewWidth = view == ControllerView.Top ? ControllerLayouts.TopWidth : ControllerLayouts.SideWidth;
        float x = hand == 0 ? viewWidth - box.Right : box.X;
        return new RectF(viewX + x, ViewY(view) + CanvasMargin + box.Y, box.Width, box.Height);
    }

    float TurnFor(float turn) => hand == 0 ? -turn : turn;

    static Rect R(RectF r) => new(r.X, r.Y, r.Width, r.Height);

    static Geometry Turned(Geometry g, RectF box, float turn)
    {
        if (turn == 0)
            return g;
        var c = new Point(box.X + box.Width / 2, box.Y + box.Height / 2);
        g.Transform = new MatrixTransform(Matrix.CreateTranslation(-c.X, -c.Y) * Matrix.CreateRotation(turn * Math.PI / 180) * Matrix.CreateTranslation(c.X, c.Y));
        return g;
    }

    static Geometry RoundRect(RectF r, float radius)
    {
        double d = Math.Min(radius, Math.Min(r.Width, r.Height) / 2);
        return new RectangleGeometry(R(r), d, d);
    }

    List<ControllerPart> ShownParts()
        => layout.PartsFor(hand).Where(p => silhouette == null || ControllerSilhouettes.PartFor(silhouette, p) != null || !HasOutline(p.View)).ToList();

    bool HasOutline(ControllerView view) => silhouette != null && (view == ControllerView.Top ? silhouette.Top : silhouette.Side).Length > 0;

    Geometry LoopsPath(ControllerView view, float[][] loops)
    {
        float viewX = ViewX(view), viewY = ViewY(view);
        var geo = new StreamGeometry();
        using (var ctx = geo.Open())
        {
            ctx.SetFillRule(FillRule.EvenOdd);
            foreach (var loop in loops)
            {
                if (loop.Length < 6)
                    continue;
                ctx.BeginFigure(new Point(viewX + loop[0], viewY + CanvasMargin + loop[1]), true);
                for (int i = 1; i < loop.Length / 2; i++)
                    ctx.LineTo(new Point(viewX + loop[2 * i], viewY + CanvasMargin + loop[2 * i + 1]));
                ctx.EndFigure(true);
            }
        }
        return geo;
    }

    RectF PartBounds(ControllerPart part)
    {
        var b = PartPath(part).Bounds;
        return new RectF((float)b.X, (float)b.Y, (float)b.Width, (float)b.Height);
    }

    Geometry BodyPath(BodyPiece piece)
    {
        var box = Place(piece.View, piece.Box);
        Geometry g = piece.Shape switch
        {
            BodyShape.Ellipse => new EllipseGeometry(R(box)),
            BodyShape.RoundRect => RoundRect(box, Math.Min(box.Width, box.Height) / 2.2f),
            BodyShape.Ring => Ring(box, piece.Thickness),
            _ => new RectangleGeometry(R(box)),
        };
        return Turned(g, box, TurnFor(piece.Turn));
    }

    static Geometry Ring(RectF box, float thickness)
    {
        var inner = box;
        inner.Inflate(-thickness, -thickness);
        return new GeometryGroup { FillRule = FillRule.EvenOdd, Children = { new EllipseGeometry(R(box)), new EllipseGeometry(R(inner)) } };
    }

    Geometry PartPath(ControllerPart part)
    {
        if (silhouette != null && ControllerSilhouettes.PartFor(silhouette, part) is SilhouettePart traced)
            return LoopsPath(part.View, traced.Loops);
        var box = Place(part.View, part.Box);
        Geometry g = part.Shape switch
        {
            PartShape.Button or PartShape.Stick => new EllipseGeometry(R(box)),
            PartShape.Pad or PartShape.Pill => RoundRect(box, Math.Min(box.Width, box.Height) / 2),
            PartShape.Trigger => TriggerPath(box),
            PartShape.Grip => RoundRect(box, box.Width / 2.5f),
            _ => new RectangleGeometry(R(box)),
        };
        return Turned(g, box, TurnFor(part.Turn));
    }

    // A curved finger trigger hanging under the front of the head.
    Geometry TriggerPath(RectF b)
    {
        bool front = hand == 1;
        float x0 = front ? b.Left : b.Right, x1 = front ? b.Right : b.Left;
        var geo = new StreamGeometry();
        using (var ctx = geo.Open())
        {
            ctx.BeginFigure(new Point(x1, b.Top), true);
            ctx.CubicBezierTo(new Point(x0 - (x1 - x0) * 0.1f, b.Top + b.Height * 0.15f), new Point(x0, b.Bottom - b.Height * 0.2f), new Point(x0 + (x1 - x0) * 0.45f, b.Bottom));
            ctx.CubicBezierTo(new Point(x0 + (x1 - x0) * 0.75f, b.Bottom - b.Height * 0.3f), new Point(x1, b.Top + b.Height * 0.5f), new Point(x1, b.Top + b.Height * 0.2f));
            ctx.EndFigure(true);
        }
        return geo;
    }

    enum TagColumn { Left, Middle, Right }

    TagColumn ColumnOf(ControllerPart part)
    {
        if (part.View == ControllerView.Side)
            return Stacked ? TagColumn.Middle : TagColumn.Right;
        var box = PartBounds(part);
        return box.X + box.Width / 2 < TopX + ControllerLayouts.TopWidth / 2 ? TagColumn.Left : TagColumn.Middle;
    }

    float ColumnX(TagColumn column) => column switch
    {
        TagColumn.Left => 0,
        TagColumn.Middle => MiddleX,
        _ => CanvasW - LabelWidth + 6 * T,
    };

    int ShownLines(ControllerPart part)
    {
        int n = part.IsUsable ? linesOf(part).Count : 0;
        return n <= MaxLines ? n : MaxLines;
    }

    float TagHeightOf(ControllerPart part) => Math.Max(MinTagHeight, NameHeight + 4 + Math.Max(1, ShownLines(part)) * LineHeight);

    Dictionary<ControllerPart, RectF> TagBoxes(IReadOnlyList<ControllerPart> parts)
    {
        var tags = new Dictionary<ControllerPart, RectF>();
        foreach (var col in Enum.GetValues<TagColumn>())
        {
            float x = ColumnX(col);
            var column = parts.Where(p => ColumnOf(p) == col)
                              .Select(p => (part: p, h: TagHeightOf(p), y: PartBounds(p).Y + PartBounds(p).Height / 2 - TagHeightOf(p) / 2))
                              .OrderBy(t => t.y).ToList();
            float next = CanvasMargin;
            foreach (var (part, h, y) in column)
            {
                float top = Math.Max(y, next);
                tags[part] = new RectF(x, top, LabelWidth - 6 * T, h);
                next = top + h + TagGap;
            }
        }
        return tags;
    }

    // ---- drawing
    static readonly Color BodyFill = Color.FromRgb(62, 68, 80), BodyEdge = Color.FromRgb(98, 106, 120), PartFill = Color.FromRgb(34, 37, 44);
    static readonly Color Accent = Color.FromRgb(60, 140, 255), SelectedEdge = Color.FromRgb(240, 140, 30);
    static readonly Color Surface = Color.FromRgb(35, 39, 46), SelectedTag = Color.FromRgb(40, 64, 110), TagBorder = Color.FromRgb(58, 66, 80);
    static readonly Color TextC = Color.FromRgb(230, 233, 238), SubC = Color.FromRgb(160, 168, 182), OffC = Color.FromRgb(110, 118, 130), DoesC = Color.FromRgb(140, 190, 255);
    static Color A(byte a, Color c) => Color.FromArgb(a, c.R, c.G, c.B);
    static readonly Typeface Ui = new("Segoe UI"), UiBold = new("Segoe UI", FontStyle.Normal, FontWeight.Bold);

    public override void Render(DrawingContext g)
    {
        FitTags();
        hits.Clear();
        using var push = g.PushTransform(CanvasToScreen());
        DrawViewCaptions(g);
        DrawBody(g);
        var parts = ShownParts();
        var tags = TagBoxes(parts);
        foreach (var part in parts)
        {
            var shape = PartPath(part);
            DrawLeader(g, part, shape, tags[part]);
            DrawPart(g, part, shape);
            DrawTag(g, part, tags[part]);
            if (AddButtons && part.IsUsable)
                DrawAdd(g, AddBox(tags[part]), part.Comp == hovered && addHovered);
            hits.Add((part, shape, tags[part]));
        }
    }

    static void Text(DrawingContext g, string text, Typeface face, double size, Color color, Rect box, TextAlignment align = TextAlignment.Left, bool centreV = false)
    {
        if (string.IsNullOrEmpty(text) || box.Width <= 1)
            return;
        using var t = new TextLayout(text, face, size, new SolidColorBrush(color), align, TextWrapping.NoWrap, TextTrimming.CharacterEllipsis, maxWidth: box.Width);
        t.Draw(g, new Point(box.X, centreV ? box.Y + (box.Height - t.Height) / 2 : box.Y));
    }

    void DrawViewCaptions(DrawingContext g)
    {
        Text(g, "Buttons (from above)", Ui, 10, SubC, new Rect(TopX, ViewsHeight - CanvasMargin + 1, ControllerLayouts.TopWidth, 14), TextAlignment.Center);
        float sx = ViewX(ControllerView.Side), sy = ViewY(ControllerView.Side);
        Text(g, "Trigger and grip (side)", Ui, 10, SubC, new Rect(sx - 10, sy + ViewsHeight - CanvasMargin + 1, ControllerLayouts.SideWidth + 20, 14), TextAlignment.Center);
    }

    void DrawBody(DrawingContext g)
    {
        var fill = new SolidColorBrush(BodyFill);
        var edge = new Pen(new SolidColorBrush(BodyEdge), 1.4);
        var strap = new Pen(new SolidColorBrush(BodyEdge), 1.4, DashStyle.Dash);
        if (silhouette != null)
            foreach (var (view, loops) in new[] { (ControllerView.Top, silhouette.Top), (ControllerView.Side, silhouette.Side) })
                g.DrawGeometry(fill, edge, LoopsPath(view, loops));
        foreach (var piece in layout.Body.Where(b => !HasOutline(b.View)))
        {
            var p = BodyPath(piece);
            if (piece.IsStrap)
                g.DrawGeometry(null, strap, p);
            else
                g.DrawGeometry(fill, edge, p);
        }
    }

    void DrawPart(DrawingContext g, ControllerPart part, Geometry shape)
    {
        int count = part.IsUsable ? useOf(part).On : 0;
        bool isSelected = part.Comp == selected, isHovered = part.Comp == hovered;
        var fill = !part.IsUsable ? BodyFill : count > 0 ? A(150, Accent) : PartFill;
        g.DrawGeometry(new SolidColorBrush(fill), null, shape);
        var b = shape.Bounds;
        if (part.Shape == PartShape.Stick) // a cap ring, so it reads as a stick
            g.DrawEllipse(null, new Pen(new SolidColorBrush(A(120, BodyEdge)), 1.2), b.Center, b.Width * 0.28, b.Height * 0.28);
        var edgeColor = isSelected ? SelectedEdge : isHovered ? Accent : BodyEdge;
        g.DrawGeometry(null, new Pen(new SolidColorBrush(edgeColor), isSelected ? 3 : isHovered ? 2.2 : 1.4, part.IsUsable ? null : DashStyle.Dot), shape);
        DrawButtonLetter(g, part, b);
        if (count > 0)
            DrawBadge(g, b, count);
    }

    void DrawButtonLetter(DrawingContext g, ControllerPart part, Rect b)
    {
        if (part.Shape != PartShape.Button || part.View != ControllerView.Top) return;
        string label = part.LabelFor(hand);
        string letter = label.Length <= 2 ? label : part.Comp switch
        {
            "menu" => "≡", "dpad_up" => "▴", "dpad_down" => "▾", "dpad_left" => "◂", "dpad_right" => "▸", _ => null,
        };
        if (letter == null) return;
        double size = Math.Max(6, Math.Min(b.Width, b.Height) * 0.55);
        Text(g, letter, UiBold, size, A(230, Colors.White), b, TextAlignment.Center, centreV: true);
    }

    static void DrawBadge(DrawingContext g, Rect part, int count)
    {
        var r = new Rect(part.Right - 7, part.Top - 7, 14, 14);
        g.DrawEllipse(new SolidColorBrush(Accent), null, r.Center, 7, 7);
        Text(g, count > 9 ? "9+" : count.ToString(), UiBold, 9, Colors.White, r, TextAlignment.Center, centreV: true);
    }

    void DrawLeader(DrawingContext g, ControllerPart part, Geometry shape, RectF tag)
    {
        var b = shape.Bounds;
        var to = new Point(ColumnOf(part) == TagColumn.Left ? tag.Right : tag.Left, tag.Y + Math.Min(tag.Height / 2, NameHeight));
        bool sel = part.Comp == selected;
        g.DrawLine(new Pen(new SolidColorBrush(A(sel ? (byte)220 : (byte)110, sel ? SelectedEdge : BodyEdge)), sel ? 1.6 : 1), b.Center, to);
    }

    void DrawTag(DrawingContext g, ControllerPart part, RectF tagF)
    {
        var tag = R(tagF);
        var use = part.IsUsable ? useOf(part) : default;
        int count = use.On;
        bool isSelected = part.Comp == selected;
        g.DrawRectangle(new SolidColorBrush(isSelected ? SelectedTag : Surface),
                        new Pen(new SolidColorBrush(isSelected ? SelectedEdge : part.Comp == hovered ? Accent : TagBorder), isSelected ? 2 : 1), tag, 6, 6);
        double nameSize = 12.5 * T, subSize = 11.3 * T;
        Text(g, part.LabelFor(hand), UiBold, nameSize, part.IsUsable ? TextC : OffC,
             new Rect(tag.X + 6 * T, tag.Y + 2 * T, tag.Width - (AddButtons && part.IsUsable ? 30 : 10) * T, 16 * T));
        var lines = part.IsUsable ? linesOf(part) : Array.Empty<TagLine>();
        if (lines.Count == 0)
        {
            string status = !part.IsUsable ? "not available"
                            : count > 0 ? (count == 1 ? "1 binding" : count + " bindings") + (use.Off > 0 ? $", {use.Off} off" : "")
                            : use.Off > 0 ? $"{use.Off} switched off" : "nothing set";
            Text(g, status, Ui, subSize, count > 0 ? Accent : SubC, new Rect(tag.X + 6 * T, tag.Y + NameHeight, tag.Width - 10 * T, 14 * T));
            return;
        }
        int shown = ShownLines(part);
        for (int i = 0; i < shown; i++)
        {
            double y = tag.Y + NameHeight + i * LineHeight;
            if (i == MaxLines - 1 && lines.Count > MaxLines)
            {
                Text(g, $"+ {lines.Count - MaxLines + 1} more", Ui, subSize, SubC, new Rect(tag.X + 6 * T, y, tag.Width - 10 * T, 14 * T));
                break;
            }
            var line = lines[i];
            string when = (line.Layer != null ? line.Layer + " · " : "") + line.When + ": ";
            using var w = new TextLayout(when, Ui, subSize, new SolidColorBrush(SubC));
            w.Draw(g, new Point(tag.X + 6 * T, y));
            double ww = w.WidthIncludingTrailingWhitespace;
            Text(g, line.Does, line.IsUnused ? Ui : UiBold, subSize, line.IsUnused ? OffC : DoesC,
                 new Rect(tag.X + 6 * T + ww, y, Math.Max(1, tag.Width - 12 * T - ww), 14 * T));
        }
    }

    static void DrawAdd(DrawingContext g, RectF r, bool hot)
    {
        double d = r.Height, cx = r.X + r.Width / 2, cy = r.Y + r.Height / 2, a = d * 0.28;
        g.DrawEllipse(new SolidColorBrush(hot ? Accent : A(70, Accent)), null, new Point(cx, cy), d / 2, d / 2);
        var pen = new Pen(new SolidColorBrush(hot ? Colors.White : Accent), 1.6);
        g.DrawLine(pen, new Point(cx - a, cy), new Point(cx + a, cy));
        g.DrawLine(pen, new Point(cx, cy - a), new Point(cx, cy + a));
    }

    // ---- mouse
    Point ToCanvas(Point screen) => CanvasToScreen().Invert().Transform(screen);

    bool OnAdd(Point screen, out ControllerPart part)
    {
        part = null;
        if (!AddButtons)
            return false;
        var at = ToCanvas(screen);
        foreach (var h in hits)
        {
            var box = AddBox(h.tag);
            box.Inflate(3, 3);
            if (h.part.IsUsable && box.Contains((float)at.X, (float)at.Y))
            { part = h.part; return true; }
        }
        return false;
    }

    ControllerPart PartAt(Point screen)
    {
        var at = ToCanvas(screen);
        foreach (var h in hits)
            if (h.tag.Contains((float)at.X, (float)at.Y) || h.shape.FillContains(at))
                return h.part;
        return null;
    }

    protected override void OnPointerMoved(PointerEventArgs e)
    {
        base.OnPointerMoved(e);
        var p = e.GetPosition(this);
        string now = PartAt(p)?.Comp;
        bool onAdd = OnAdd(p, out _);
        if (now == hovered && onAdd == addHovered)
            return;
        hovered = now;
        addHovered = onAdd;
        ToolTip.SetTip(this, AddButtons && onAdd ? "Add something to this button" : null);
        Cursor = new Cursor(now == null ? StandardCursorType.Arrow : StandardCursorType.Hand);
        InvalidateVisual();
    }

    protected override void OnPointerExited(PointerEventArgs e)
    {
        base.OnPointerExited(e);
        hovered = null;
        InvalidateVisual();
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        base.OnPointerReleased(e);
        var p = e.GetPosition(this);
        if (OnAdd(p, out var addTo))
        { AddClicked?.Invoke(addTo); return; }
        var part = PartAt(p);
        if (part == null)
            return;
        Selected = part.Comp;
        PartClicked?.Invoke(part);
    }
}
