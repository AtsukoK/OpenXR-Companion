using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

// The settings as a grid of big tiles (from the dist3 design).
// One big clickable tile: an icon or picture, a title, what it is for, and a status line.
public sealed class HubTile : Control
{
    public string Glyph, Title, Sub, Status;
    public Image Picture;
    public Color StatusColor = Color.Empty;
    public Color AccentColor = Color.FromArgb(40, 140, 255);
    bool hover;

    public void PerformClick() => OnClick(EventArgs.Empty);

    public HubTile()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw | ControlStyles.UserPaint, true);
        Cursor = Cursors.Hand;
        Margin = new Padding(0, 0, 16, 16);
    }

    protected override void OnMouseEnter(EventArgs e) { base.OnMouseEnter(e); hover = true; Invalidate(); }
    protected override void OnMouseLeave(EventArgs e) { base.OnMouseLeave(e); hover = false; Invalidate(); }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
        g.Clear(Parent?.BackColor ?? Theme.Page);
        var r = new RectangleF(1, 1, Width - 3, Height - 3);
        using (var path = Round(r, 12))
        {
            using var fill = new SolidBrush(hover ? Theme.Selected : Theme.Card);
            using var edge = new Pen(hover ? AccentColor : Theme.CardBorder, hover ? 2f : 1f);
            g.FillPath(fill, path);
            g.DrawPath(edge, path);
        }
        float x = 22, y = 18;
        if (Picture != null)
        {
            float ph = Math.Min(Picture.Height, Height * 0.48f), pw = Picture.Width * ph / Picture.Height;
            g.DrawImage(Picture, x, y, pw, ph);
            y += ph + 10;
        }
        else if (!string.IsNullOrEmpty(Glyph))
        {
            using var iconFont = new Font("Segoe MDL2 Assets", 30f);
            using var iconBrush = new SolidBrush(AccentColor);
            g.DrawString(Glyph, iconFont, iconBrush, x - 4, y);
            y += 58;
        }
        float w = Width - 2 * x;
        using var titleFont = new Font("Segoe UI", 15f, FontStyle.Bold);
        using var subFont = new Font("Segoe UI", 10f);
        using var statusFont = new Font("Segoe UI", 10f, FontStyle.Bold);
        using var text = new SolidBrush(Theme.Text);
        using var sub = new SolidBrush(Theme.SubText);
        var fmt = new StringFormat { Trimming = StringTrimming.EllipsisWord };
        var titleSize = g.MeasureString(Title ?? "", titleFont, (int)w);
        g.DrawString(Title ?? "", titleFont, text, new RectangleF(x, y, w, titleSize.Height + 2), fmt);
        y += titleSize.Height + 4;
        float statusH = string.IsNullOrEmpty(Status) ? 0 : 26;
        if (!string.IsNullOrEmpty(Sub))
            g.DrawString(Sub, subFont, sub, new RectangleF(x, y, w, Height - y - statusH - 14), fmt);
        if (statusH > 0)
        {
            using var sb = new SolidBrush(StatusColor.IsEmpty ? AccentColor : StatusColor);
            g.DrawString(Status, statusFont, sb, new RectangleF(x, Height - 16 - 20, w, 22), new StringFormat { Trimming = StringTrimming.EllipsisCharacter, FormatFlags = StringFormatFlags.NoWrap });
        }
    }

    static GraphicsPath Round(RectangleF r, float rad)
    {
        var p = new GraphicsPath();
        float d = rad * 2;
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
            Picture?.Dispose();
        base.Dispose(disposing);
    }
}

// A screen of tiles under a big title. Tiles wrap to the window's width.
public class HubPage : Panel
{
    // tiles and headings, placed by hand: tiles wrap to the width, a heading starts a new row
    protected readonly Panel Tiles = new() { Dock = DockStyle.Top, Height = 10 };
    readonly List<(Control c, bool heading, bool breakAfter)> items = new();
    readonly FlowLayoutPanel head = new() { Dock = DockStyle.Top, AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false };
    public static readonly Color Green = Color.FromArgb(60, 180, 110), Orange = Color.FromArgb(235, 150, 50), Grey = Color.FromArgb(140, 145, 155);

    public HubPage(string title, string intro)
    {
        Dock = DockStyle.Fill;
        AutoScroll = true;
        BackColor = Theme.Page;
        Padding = new Padding(36, 26, 20, 20);
        head.Controls.Add(new Label { Text = title, AutoSize = true, Font = new Font("Segoe UI", 22f, FontStyle.Bold), ForeColor = Theme.Text, Margin = new Padding(0, 0, 0, 4) });
        if (!string.IsNullOrEmpty(intro))
            head.Controls.Add(new Label { Text = intro, AutoSize = true, MaximumSize = new Size(900, 0), Font = new Font("Segoe UI", 10.5f), ForeColor = Theme.SubText, Margin = new Padding(0, 0, 0, 18) });
        Controls.Add(Tiles);
        Controls.Add(head);
        Tiles.Resize += (s, e) => PlaceTiles();
    }

    void PlaceTiles()
    {
        const int gap = 16;
        int x = 0, y = 0, rowH = 0, width = Math.Max(Tiles.ClientSize.Width, 300);
        foreach (var (c, heading, breakAfter) in items)
        {
            if (heading)
            {
                if (x > 0 || rowH > 0) { y += rowH + gap; x = 0; rowH = 0; }
                c.Location = new Point(0, y + (y > 0 ? 10 : 0));
                y = c.Bottom + 8;
                continue;
            }
            if (x > 0 && x + c.Width > width)
            { x = 0; y += rowH + gap; rowH = 0; }
            c.Location = new Point(x, y);
            x += c.Width + gap;
            rowH = Math.Max(rowH, c.Height);
            if (breakAfter)
            { x = 0; y += rowH + gap; rowH = 0; }
        }
        int h = y + rowH + 4;
        if (Tiles.Height != h)
            Tiles.Height = h;
    }

    protected void RowBreak()
    {
        if (items.Count > 0)
            items[^1] = (items[^1].c, items[^1].heading, true);
    }

    protected HubTile Tile(string glyph, string title, string sub, string status, Color statusColor, Action click, int w = 330, int h = 200, Image picture = null)
    {
        var t = new HubTile { Glyph = glyph, Title = title, Sub = sub, Status = status, StatusColor = statusColor, Size = new Size(w, h), Picture = picture };
        t.Click += (s, e) => click();
        Tiles.Controls.Add(t);
        items.Add((t, false, false));
        PlaceTiles();
        return t;
    }

    protected void Break(string heading)
    {
        var l = new Label { Text = heading.ToUpperInvariant(), AutoSize = true, Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), ForeColor = Theme.Heading };
        Tiles.Controls.Add(l);
        items.Add((l, true, false));
        PlaceTiles();
    }
}

public sealed class FeaturesHub : HubPage
{
    public FeaturesHub(ProfileModel p, Action<object> go) : base("Configure features", "Pick a feature: only its settings open, using the whole window. Back returns here.")
    {
        Break("Headset features");
        foreach (var key in new[] { "screen", "offsets", "mirror", "pointing", "keyboard" })
            Add(key);
        Break("Look and behaviour");
        foreach (var key in new[] { "general", "wheel", "advanced", "experimental" })
            if (key != "experimental" || HeadSmoothingFile.ShowPage || HeadSmoothingFile.On)
                Add(key);
        Tile("", "What's on now", "Every feature with an on / off switch, and how you open it in the headset.", null, Color.Empty, () => go("overview"), 300, 170);

        void Add(string key)
        {
            var sp = SettingsPage.ByKey(key);
            Tile(sp.Icon, sp.Title, sp.Sub, sp.Live ? "Applies straight away" : null, Grey, () => go(sp), 300, 170);
        }
    }
}

