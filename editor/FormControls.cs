using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

// ------------------------------------------------------------ form helpers
public class FormPanel : TableLayoutPanel
{
    public FormPanel()
    {
        ColumnCount = 2; AutoSize = true; AutoSizeMode = AutoSizeMode.GrowAndShrink; Dock = DockStyle.Top;
        ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 170));
        ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        Padding = new Padding(0, 0, 0, 6);
    }
    // Every row, for search and highlighting: its label, its control and its help text.
    public readonly List<(Label label, Control control, string hint)> Rows = new();
    public static readonly ToolTip Tips = new() { AutoPopDelay = 30000, InitialDelay = 250, ReshowDelay = 100 };
    public static string Wrap(string s, int width = 70)
    {
        var sb = new System.Text.StringBuilder();
        foreach (var para in s.Split('\n'))
        {
            int col = 0;
            foreach (var w in para.Split(' '))
            {
                if (col > 0 && col + w.Length + 1 > width) { sb.Append('\n'); col = 0; }
                else if (col > 0) { sb.Append(' '); col++; }
                sb.Append(w); col += w.Length;
            }
            sb.Append('\n');
        }
        return sb.ToString().TrimEnd();
    }
    // A small (i) after a control: hover or click it to read the help.
    public static Label InfoIcon(string hint)
    {
        var i = new Label { Text = "\u24D8", AutoSize = true, Font = new Font("Segoe UI", 11f), ForeColor = Theme.Link, Cursor = Cursors.Hand, Margin = new Padding(6, 3, 0, 0), BackColor = Color.Transparent };
        string text = Wrap(hint);
        Tips.SetToolTip(i, text);
        i.Click += (s, e) => Tips.Show(text, i, 0, i.Height + 2, 20000);
        return i;
    }
    public Label AddRow(string label, Control c, string hint = null)
    {
        var l = new Label { Text = label, AutoSize = true, Anchor = AnchorStyles.Left | AnchorStyles.Top, Padding = new Padding(0, 9, 0, 0), BackColor = Color.Transparent };
        RowStyles.Add(new RowStyle(SizeType.AutoSize));
        int row = RowCount++;
        Controls.Add(l, 0, row);
        // Every row in the same holder, with or without help, so all rows get the same height and spacing.
        var f = new FlowLayoutPanel { FlowDirection = FlowDirection.LeftToRight, AutoSize = true, WrapContents = false, Margin = new Padding(0, 3, 0, 5), MinimumSize = new Size(0, 32), BackColor = Color.Transparent };
        f.Controls.Add(c);
        if (hint != null) f.Controls.Add(InfoIcon(hint));
        Controls.Add(f, 1, row);
        c.VisibleChanged += (s, e) => { f.Visible = c.Visible; };
        l.Tag = f;
        Rows.Add((l, c, hint));
        return l;
    }
    public static void Show(Label rowLabel, bool on) { rowLabel.Visible = on; if (rowLabel.Tag is Control c) c.Visible = on; }

    public static Label Header(string text, string help = null)
    {
        return new Label
        {
            Text = text + (help != null ? "\n" + help : ""), AutoSize = true, MaximumSize = new Size(640, 0),
            Font = new Font("Segoe UI", 9.5f), Padding = new Padding(0, 4, 0, 10), Dock = DockStyle.Top, ForeColor = Theme.SubText
        };
    }
}

// A white rounded box around a group of settings, with a heading, an icon and a small tag.
public class Card : Panel
{
    readonly TableLayoutPanel inner;
    public Card(string title, string glyph, string tag, Color tagColor, string tagHelp, params Control[] body)
    {
        DoubleBuffered = true; AutoSize = true; AutoSizeMode = AutoSizeMode.GrowAndShrink; Dock = DockStyle.Top;
        Padding = new Padding(18, 12, 18, 12); BackColor = Theme.Page;
        inner = new TableLayoutPanel { ColumnCount = 1, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, BackColor = Color.Transparent, Location = new Point(18, 12), Margin = Padding.Empty };
        var head = new FlowLayoutPanel { AutoSize = true, WrapContents = false, BackColor = Color.Transparent, Margin = new Padding(0, 0, 0, 6) };
        if (glyph != null) head.Controls.Add(new Label { Text = glyph, Font = new Font("Segoe MDL2 Assets", 13f), ForeColor = Theme.Heading, AutoSize = true, Margin = new Padding(0, 5, 8, 0), BackColor = Color.Transparent });
        head.Controls.Add(new Label { Text = title, Font = new Font("Segoe UI", 12f, FontStyle.Bold), ForeColor = Theme.Heading, AutoSize = true, Margin = new Padding(0, 2, 10, 0), BackColor = Color.Transparent });
        if (tag != null)
        {
            var pill = new Label { Text = tag, Font = new Font("Segoe UI", 8.5f, FontStyle.Bold), ForeColor = Color.White, BackColor = tagColor, AutoSize = true, Padding = new Padding(6, 2, 6, 2), Margin = new Padding(0, 6, 0, 0) };
            if (tagHelp != null) FormPanel.Tips.SetToolTip(pill, tagHelp);
            head.Controls.Add(pill);
        }
        inner.Controls.Add(head);
        foreach (var b in body) { b.Dock = DockStyle.None; b.BackColor = Color.Transparent; b.Margin = Padding.Empty; inner.Controls.Add(b); }
        Controls.Add(inner);
        void Solid(Control c) { foreach (Control k in c.Controls) { if (k is TrackBar) k.BackColor = Theme.Card; Solid(k); } }
        Solid(inner); // sliders can't be see-through: give them the card's colour
    }
    protected override void OnPaintBackground(PaintEventArgs e)
    {
        e.Graphics.Clear(Theme.Page);
        e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        var r = new Rectangle(0, 0, Width - 1, Height - 1);
        using var path = new System.Drawing.Drawing2D.GraphicsPath();
        int d = 16;
        path.AddArc(r.X, r.Y, d, d, 180, 90); path.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        path.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90); path.AddArc(r.X, r.Bottom - d, d, d, 90, 90); path.CloseFigure();
        using (var b = new SolidBrush(Theme.Card)) e.Graphics.FillPath(b, path);
        using (var p = new Pen(Theme.CardBorder)) e.Graphics.DrawPath(p, path);
    }
    protected override void OnResize(EventArgs e) { base.OnResize(e); Invalidate(); }
}

// One page of Settings in the list on the left.
public sealed class SettingsPage
{
    public string Key, Title, Sub, Icon;
    public bool Live; // applies straight away (not saved with the profile)
    public static readonly SettingsPage[] All =
    {
        new() { Key = "general", Title = "General", Sub = "On/off, vibration, messages, updates", Icon = "\uE713" },
        new() { Key = "wheel", Title = "Wheel look and picking", Sub = "Where it appears, size, colours, how items are picked", Icon = "\uE790" },
        new() { Key = "pointing", Title = "Pointing", Sub = "Laser direction, fingertip, steadying", Icon = "\uE7C9" },
        new() { Key = "keyboard", Title = "VR keyboard", Sub = "Size and position", Icon = "\uE765" },
        new() { Key = "screen", Title = "Screen mirror", Sub = "A monitor or window in VR", Icon = "\uE7F4" },
        new() { Key = "offsets", Title = "Controller offsets", Sub = "How the game's hands and guns point", Icon = "\uE7FC" },
        new() { Key = "mirror", Title = "Headset mirror", Sub = "What the headset shows, on the desktop", Icon = "\uE890", Live = true },
        new() { Key = "experimental", Title = "Experimental", Sub = "Head smoothing (can cause motion sickness)", Icon = "\uE7BA", Live = true },
        new() { Key = "advanced", Title = "Advanced", Sub = "Thresholds, wheel details, focus", Icon = "\uE90F" },
    };
    public static SettingsPage ByKey(string k) => All.FirstOrDefault(p => p.Key == k) ?? All[0];
}
