using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

// Draws one controller (a ControllerLayout, for one hand) with a name tag for each part, and says which part
// was clicked. Parts that already do something get a count badge; the selected part is outlined.
public class ControllerPicture : Control
{
    // Design canvas: [labels][top view][labels][side view][labels], see ControllerLayouts for the view sizes.
    // Top-view parts are named on whichever side of the top view they sit, so leader lines don't cross the body.
    const float LabelWidth = 128, Gap = 10, CanvasMargin = 14, TagHeight = 34, TagGap = 5;
    const float TopX = LabelWidth, MiddleX = TopX + ControllerLayouts.TopWidth + Gap;
    const float SideX = MiddleX + LabelWidth + Gap;
    const float CanvasWidth = SideX + ControllerLayouts.SideWidth + LabelWidth;
    const float CanvasHeight = ControllerLayouts.Height + 2 * CanvasMargin;

    ControllerLayout layout = ControllerLayouts.All[0];
    int hand = 1;
    string selected, hovered;
    Func<ControllerPart, int> bindingCount = _ => 0;
    readonly List<(ControllerPart part, GraphicsPath shape, RectangleF tag)> hits = new();

    public event Action<ControllerPart> PartClicked;

    public ControllerPicture()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw | ControlStyles.UserPaint, true);
        Cursor = Cursors.Hand;
    }

    public void ShowController(ControllerLayout newLayout, int newHand, string selectedComp, Func<ControllerPart, int> countOf)
    {
        layout = newLayout;
        hand = newHand;
        selected = selectedComp;
        bindingCount = countOf;
        Invalidate();
    }

    public string Selected
    {
        get => selected;
        set { selected = value; Invalidate(); }
    }

    // A picture of one controller as a bitmap (previews and screenshots).
    public static Bitmap Render(ControllerLayout layout, int hand, string selectedComp, Func<ControllerPart, int> countOf, int width)
    {
        using var pic = new ControllerPicture { Size = new Size(width, (int)(width * CanvasHeight / CanvasWidth)), BackColor = Theme.Card };
        pic.ShowController(layout, hand, selectedComp, countOf);
        var bmp = new Bitmap(pic.Width, pic.Height);
        pic.DrawToBitmap(bmp, new Rectangle(Point.Empty, pic.Size));
        return bmp;
    }

    // ------------------------------------------------------------ geometry
    Matrix CanvasToScreen()
    {
        float s = Math.Min(Width / CanvasWidth, Height / CanvasHeight);
        var m = new Matrix();
        m.Translate((Width - CanvasWidth * s) / 2, (Height - CanvasHeight * s) / 2);
        m.Scale(s, s);
        return m;
    }

    // A box given for the right hand, placed on the canvas (mirrored inside its view for the left hand).
    RectangleF Place(ControllerView view, RectangleF box)
    {
        float viewX = view == ControllerView.Top ? TopX : SideX;
        float viewWidth = view == ControllerView.Top ? ControllerLayouts.TopWidth : ControllerLayouts.SideWidth;
        float x = hand == 0 ? viewWidth - box.Right : box.X;
        return new RectangleF(viewX + x, CanvasMargin + box.Y, box.Width, box.Height);
    }

    float TurnFor(float turn) => hand == 0 ? -turn : turn;

    static GraphicsPath Turned(GraphicsPath p, RectangleF box, float turn)
    {
        if (turn == 0)
            return p;
        using var m = new Matrix();
        m.RotateAt(turn, new PointF(box.X + box.Width / 2, box.Y + box.Height / 2));
        p.Transform(m);
        return p;
    }

    static GraphicsPath RoundRect(RectangleF r, float radius)
    {
        float d = Math.Min(radius * 2, Math.Min(r.Width, r.Height));
        var p = new GraphicsPath();
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }

    GraphicsPath BodyPath(BodyPiece piece)
    {
        var box = Place(piece.View, piece.Box);
        var p = new GraphicsPath();
        switch (piece.Shape)
        {
            case BodyShape.Ellipse:
                p.AddEllipse(box);
                break;
            case BodyShape.RoundRect:
                p.AddPath(RoundRect(box, Math.Min(box.Width, box.Height) / 2.2f), false);
                break;
            case BodyShape.Ring:
                p.AddEllipse(box);
                var inner = box;
                inner.Inflate(-piece.Thickness, -piece.Thickness);
                p.AddEllipse(inner); // alternate fill: a hole in the middle
                break;
        }
        return Turned(p, box, TurnFor(piece.Turn));
    }

    GraphicsPath PartPath(ControllerPart part)
    {
        var box = Place(part.View, part.Box);
        var p = new GraphicsPath();
        switch (part.Shape)
        {
            case PartShape.Button:
            case PartShape.Stick:
                p.AddEllipse(box);
                break;
            case PartShape.Pad:
                p.AddPath(RoundRect(box, Math.Min(box.Width, box.Height) / 2), false);
                break;
            case PartShape.Pill:
                p.AddPath(RoundRect(box, Math.Min(box.Width, box.Height) / 2), false);
                break;
            case PartShape.Trigger:
                p.AddPath(TriggerPath(box), false);
                break;
            case PartShape.Grip:
                p.AddPath(RoundRect(box, box.Width / 2.5f), false);
                break;
        }
        return Turned(p, box, TurnFor(part.Turn));
    }

    // A curved finger trigger hanging under the front of the head.
    GraphicsPath TriggerPath(RectangleF b)
    {
        bool front = hand == 1; // front is on the left for the right hand, on the right when mirrored
        float x0 = front ? b.Left : b.Right, x1 = front ? b.Right : b.Left;
        var p = new GraphicsPath();
        p.AddBezier(x1, b.Top, x0 - (x1 - x0) * 0.1f, b.Top + b.Height * 0.15f, x0, b.Bottom - b.Height * 0.2f, x0 + (x1 - x0) * 0.45f, b.Bottom);
        p.AddBezier(x0 + (x1 - x0) * 0.45f, b.Bottom, x0 + (x1 - x0) * 0.75f, b.Bottom - b.Height * 0.3f, x1, b.Top + b.Height * 0.5f, x1, b.Top + b.Height * 0.2f);
        p.CloseFigure();
        return p;
    }

    enum TagColumn { Left, Middle, Right }

    // Top-view parts left of the view's middle are named in the left column, the others in the middle one;
    // side-view parts in the right column.
    TagColumn ColumnOf(ControllerPart part)
    {
        if (part.View == ControllerView.Side)
            return TagColumn.Right;
        var box = Place(part.View, part.Box);
        return box.X + box.Width / 2 < TopX + ControllerLayouts.TopWidth / 2 ? TagColumn.Left : TagColumn.Middle;
    }

    static float ColumnX(TagColumn column) => column switch
    {
        TagColumn.Left => 0,
        TagColumn.Middle => MiddleX,
        _ => CanvasWidth - LabelWidth + 6,
    };

    // Name tags at the height of their part, pushed apart so they never overlap.
    Dictionary<ControllerPart, RectangleF> TagBoxes(IReadOnlyList<ControllerPart> parts)
    {
        var tags = new Dictionary<ControllerPart, RectangleF>();
        foreach (var col in Enum.GetValues<TagColumn>())
        {
            float x = ColumnX(col);
            var column = parts.Where(p => ColumnOf(p) == col)
                              .Select(p => (part: p, y: Place(p.View, p.Box).Y + p.Box.Height / 2 - TagHeight / 2))
                              .OrderBy(t => t.y).ToList();
            var ys = column.Select(t => t.y).ToArray();
            for (int i = 0; i < ys.Length; i++) // down: no overlap with the tag above
                ys[i] = Math.Max(ys[i], i == 0 ? CanvasMargin : ys[i - 1] + TagHeight + TagGap);
            for (int i = ys.Length - 1; i >= 0; i--) // up: stay on the canvas
                ys[i] = Math.Min(ys[i], i == ys.Length - 1 ? CanvasHeight - CanvasMargin - TagHeight : ys[i + 1] - TagHeight - TagGap);
            for (int i = 0; i < column.Count; i++)
                tags[column[i].part] = new RectangleF(x, ys[i], LabelWidth - 6, TagHeight);
        }
        return tags;
    }

    // ------------------------------------------------------------ drawing
    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAliasGridFit;
        g.Clear(BackColor);
        foreach (var h in hits)
            h.shape.Dispose();
        hits.Clear();
        using var m = CanvasToScreen();
        g.Transform = m;

        DrawViewCaptions(g);
        DrawBody(g);
        var parts = layout.PartsFor(hand).ToList();
        var tags = TagBoxes(parts);
        foreach (var part in parts)
        {
            var shape = PartPath(part);
            DrawLeader(g, part, shape, tags[part]);
            DrawPart(g, part, shape);
            DrawTag(g, part, tags[part]);
            hits.Add((part, shape, tags[part]));
        }
    }

    Color BodyFill => Theme.IsDark ? Color.FromArgb(62, 68, 80) : Color.FromArgb(206, 212, 222);
    Color BodyEdge => Theme.IsDark ? Color.FromArgb(98, 106, 120) : Color.FromArgb(150, 158, 172);
    Color PartFill => Theme.IsDark ? Color.FromArgb(34, 37, 44) : Color.FromArgb(248, 249, 251);
    static Color Accent => Color.FromArgb(60, 140, 255);
    static Color SelectedEdge => Color.FromArgb(240, 140, 30);

    void DrawViewCaptions(Graphics g)
    {
        using var f = new Font("Segoe UI", 7.5f);
        using var b = new SolidBrush(Theme.SubText);
        var center = new StringFormat { Alignment = StringAlignment.Center };
        g.DrawString("Buttons (from above)", f, b, new RectangleF(TopX, CanvasHeight - CanvasMargin + 1, ControllerLayouts.TopWidth, 14), center);
        g.DrawString("Trigger and grip (side)", f, b, new RectangleF(SideX - 10, CanvasHeight - CanvasMargin + 1, ControllerLayouts.SideWidth + 20, 14), center);
    }

    void DrawBody(Graphics g)
    {
        using var fill = new SolidBrush(BodyFill);
        using var edge = new Pen(BodyEdge, 1.4f);
        using var strap = new Pen(BodyEdge, 1.4f) { DashStyle = DashStyle.Dash };
        foreach (var piece in layout.Body)
        {
            using var p = BodyPath(piece);
            if (piece.IsStrap)
            {
                g.DrawPath(strap, p);
                continue;
            }
            g.FillPath(fill, p);
            g.DrawPath(edge, p);
        }
    }

    void DrawPart(Graphics g, ControllerPart part, GraphicsPath shape)
    {
        int count = part.IsUsable ? bindingCount(part) : 0;
        bool isSelected = part.Comp == selected;
        bool isHovered = part.Comp == hovered;
        Color fill = !part.IsUsable ? BodyFill : count > 0 ? Color.FromArgb(Theme.IsDark ? 150 : 110, Accent) : PartFill;
        using (var b = new SolidBrush(fill))
            g.FillPath(b, shape);
        if (!part.IsUsable)
            using (var hatch = new HatchBrush(HatchStyle.WideUpwardDiagonal, Color.FromArgb(90, BodyEdge), Color.Transparent))
                g.FillPath(hatch, shape);
        if (part.Shape == PartShape.Stick) // a cap ring, so it reads as a stick
        {
            var box = Place(part.View, part.Box);
            box.Inflate(-box.Width * 0.22f, -box.Height * 0.22f);
            using var cap = new Pen(Color.FromArgb(120, BodyEdge), 1.2f);
            g.DrawEllipse(cap, box);
        }
        using (var edge = new Pen(isSelected ? SelectedEdge : isHovered ? Accent : BodyEdge, isSelected ? 3f : isHovered ? 2.2f : 1.4f) { DashStyle = part.IsUsable ? DashStyle.Solid : DashStyle.Dot })
            g.DrawPath(edge, shape);
        if (count > 0)
            DrawBadge(g, shape.GetBounds(), count);
    }

    static void DrawBadge(Graphics g, RectangleF partBounds, int count)
    {
        var r = new RectangleF(partBounds.Right - 7, partBounds.Top - 7, 14, 14);
        using (var b = new SolidBrush(Accent))
            g.FillEllipse(b, r);
        using var f = new Font("Segoe UI", 7f, FontStyle.Bold);
        g.DrawString(count > 9 ? "9+" : count.ToString(), f, Brushes.White, r, new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center });
    }

    void DrawLeader(Graphics g, ControllerPart part, GraphicsPath shape, RectangleF tag)
    {
        var b = shape.GetBounds();
        var from = new PointF(b.X + b.Width / 2, b.Y + b.Height / 2);
        var to = new PointF(ColumnOf(part) == TagColumn.Left ? tag.Right : tag.Left, tag.Y + tag.Height / 2);
        using var pen = new Pen(Color.FromArgb(part.Comp == selected ? 220 : 110, part.Comp == selected ? SelectedEdge : BodyEdge), part.Comp == selected ? 1.6f : 1f);
        g.DrawLine(pen, from, to);
    }

    void DrawTag(Graphics g, ControllerPart part, RectangleF tag)
    {
        int count = part.IsUsable ? bindingCount(part) : 0;
        bool isSelected = part.Comp == selected;
        using (var p = RoundRect(tag, 6))
        {
            using var fill = new SolidBrush(isSelected ? Theme.Selected : Theme.Surface);
            using var edge = new Pen(isSelected ? SelectedEdge : part.Comp == hovered ? Accent : Theme.CardBorder, isSelected ? 2f : 1f);
            g.FillPath(fill, p);
            g.DrawPath(edge, p);
        }
        using var nameFont = new Font("Segoe UI", 8.5f, FontStyle.Bold);
        using var subFont = new Font("Segoe UI", 7.5f);
        using var text = new SolidBrush(part.IsUsable ? Theme.Text : Theme.OffText);
        using var sub = new SolidBrush(count > 0 ? Accent : Theme.SubText);
        var fmt = new StringFormat { Trimming = StringTrimming.EllipsisCharacter, FormatFlags = StringFormatFlags.NoWrap };
        g.DrawString(part.LabelFor(hand), nameFont, text, new RectangleF(tag.X + 6, tag.Y + 2, tag.Width - 10, 16), fmt);
        string status = !part.IsUsable ? "not available" : count == 0 ? "nothing set" : count == 1 ? "1 binding" : count + " bindings";
        g.DrawString(status, subFont, sub, new RectangleF(tag.X + 6, tag.Y + 17, tag.Width - 10, 14), fmt);
    }

    // ------------------------------------------------------------ mouse
    ControllerPart PartAt(Point screen)
    {
        using var m = CanvasToScreen();
        m.Invert();
        var pts = new[] { new PointF(screen.X, screen.Y) };
        m.TransformPoints(pts);
        var at = pts[0];
        foreach (var h in hits)
            if (h.tag.Contains(at) || h.shape.IsVisible(at))
                return h.part;
        return null;
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        string now = PartAt(e.Location)?.Comp;
        if (now == hovered)
            return;
        hovered = now;
        Cursor = now == null ? Cursors.Default : Cursors.Hand;
        Invalidate();
    }

    protected override void OnMouseLeave(EventArgs e)
    {
        base.OnMouseLeave(e);
        hovered = null;
        Invalidate();
    }

    protected override void OnMouseClick(MouseEventArgs e)
    {
        base.OnMouseClick(e);
        var part = PartAt(e.Location);
        if (part == null)
            return;
        Selected = part.Comp;
        PartClicked?.Invoke(part);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
            foreach (var h in hits)
                h.shape.Dispose();
        base.Dispose(disposing);
    }
}
