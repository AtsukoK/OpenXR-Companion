using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Microsoft.Win32;

namespace OpenXRCompanionEditor;

// Light / dark look for the whole editor.
// Theme holds the colours the hand-drawn parts use; ThemeWalker recolours ordinary controls by
// mapping light backgrounds to dark ones and dark text to light text (hues kept, so coloured strips
// and buttons stay recognisable). The original colours are remembered, so switching back works.
public static class ThemeMode
{
    static string FlagPath => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "OpenXRCompanion", "editor_theme.txt");
    // dark (the default) | light | auto (follow Windows)
    public static string Choice
    {
        get { try { var s = File.ReadAllText(FlagPath).Trim(); return s is "light" or "dark" or "auto" ? s : "dark"; } catch { return "dark"; } } // dark unless chosen otherwise
        set { try { Directory.CreateDirectory(Path.GetDirectoryName(FlagPath)!); File.WriteAllText(FlagPath, value); } catch { } }
    }
    public static bool WindowsDark()
    {
        try { return Registry.GetValue(@"HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize", "AppsUseLightTheme", 1) is int v && v == 0; }
        catch { return false; }
    }
    public static bool IsDark => Choice == "dark" || (Choice == "auto" && WindowsDark());
}

public static class ThemeWalker
{
    sealed class Orig { public Color Back, Fore; public bool OwnBack, OwnFore; public FlatStyle? Flat; public Color? Border; }
    static readonly ConditionalWeakTable<Control, Orig> origs = new();

    [DllImport("dwmapi.dll")] static extern int DwmSetWindowAttribute(IntPtr h, int attr, ref int val, int size);
    [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)] static extern int SetWindowTheme(IntPtr h, string app, string id);

    public static void TitleBar(Form f, bool dark)
    {
        int v = dark ? 1 : 0;
        if (f.IsHandleCreated) { DwmSetWindowAttribute(f.Handle, 20, ref v, 4); DwmSetWindowAttribute(f.Handle, 19, ref v, 4); }
    }

    // Recolour c and everything inside it for the current theme.
    public static void Apply(Control c)
    {
        bool dark = Theme.IsDark;
        if (c is Form) c.SuspendLayout();
        Walk(c, dark);
        if (c is Form) c.ResumeLayout();
    }

    static void Walk(Control c, bool dark)
    {
        if (c is not (WheelPreview or ArmPosePreview) && c.Tag as string != "swatch") // pictures and colour swatches keep their own colours
        {
            if (!origs.TryGetValue(c, out var o))
            {
                var props = System.ComponentModel.TypeDescriptor.GetProperties(c);
                o = new Orig { Back = c.BackColor, Fore = c.ForeColor,
                               OwnBack = props["BackColor"]?.ShouldSerializeValue(c) ?? true, OwnFore = props["ForeColor"]?.ShouldSerializeValue(c) ?? true };
                if (c is ButtonBase bb) { o.Flat = bb.FlatStyle; o.Border = bb.FlatAppearance.BorderColor; }
                origs.Add(c, o);
            }
            if (!dark)
            {
                if (o.OwnBack || c is Form) c.BackColor = o.Back; else c.ResetBackColor();
                if (o.OwnFore || c is Form) c.ForeColor = o.Fore; else c.ResetForeColor();
                if (c is ButtonBase b0 && o.Flat.HasValue) { b0.FlatStyle = o.Flat.Value; if (o.Border.HasValue) b0.FlatAppearance.BorderColor = o.Border.Value; }
            }
            else
            {
                Color back = o.Back;
                if (c is TextBoxBase or ComboBox or NumericUpDown or ListBox || (o.OwnBack && back == SystemColors.Window)) c.BackColor = Theme.Input;
                else if (c is Form) c.BackColor = Theme.Bar;
                else if (o.OwnBack && back.A > 0) c.BackColor = MapBack(back);   // inherited colours follow their parent by themselves
                if (c is Form) c.ForeColor = Theme.Text;
                else if (o.OwnFore || c is TextBoxBase or ComboBox or NumericUpDown or ListBox) c.ForeColor = c is TextBoxBase or ComboBox or NumericUpDown or ListBox ? Theme.Text : MapFore(o.Fore);
                if (c is Button b && o.Flat == FlatStyle.Standard)
                {
                    b.FlatStyle = FlatStyle.Flat; b.FlatAppearance.BorderColor = Theme.CardBorder; b.BackColor = Theme.Button;
                }
                else if (c is ButtonBase b2 && o.Flat == FlatStyle.Flat && o.Border.HasValue && o.Border.Value.GetBrightness() > 0.6f)
                    b2.FlatAppearance.BorderColor = Theme.CardBorder;
                if (c is ComboBox cb) DarkCombo(cb);
            }
            if (c is TreeView || (c is ScrollableControl sc && sc.AutoScroll))
            {
                void Scroll() => SetWindowTheme(c.Handle, dark ? "DarkMode_Explorer" : "Explorer", null);
                if (c.IsHandleCreated) Scroll(); else c.HandleCreated += (s, e) => Scroll();
            }
        }
        if (c is ToolStrip ts) Strip(ts, dark);
        foreach (Control k in c.Controls) Walk(k, dark);
        if (c is Form f) TitleBar(f, dark);
    }

    static void Strip(ToolStrip ts, bool dark)
    {
        ts.Renderer = dark ? new ToolStripProfessionalRenderer(new DarkTable()) { RoundedEdges = false } : new ToolStripProfessionalRenderer();
        ts.BackColor = dark ? Theme.Bar : SystemColors.Control;
        foreach (ToolStripItem it in ts.Items)
        {
            if (it.Tag is not Color orig) { orig = it.ForeColor; it.Tag = orig; }
            it.ForeColor = dark ? MapFore(orig) : orig;
            if (it is ToolStripStatusLabel sl && sl.IsLink) { sl.LinkColor = sl.ActiveLinkColor = dark ? Theme.Link : Color.FromArgb(0, 0, 255); }
            if (it is ToolStripComboBox tcd && dark) DarkCombo(tcd.ComboBox);
            if (it is ToolStripComboBox tc) { tc.BackColor = dark ? Theme.Input : SystemColors.Window; tc.ForeColor = dark ? Theme.Text : SystemColors.WindowText; tc.FlatStyle = dark ? FlatStyle.Flat : FlatStyle.Standard; }
        }
    }

    // A drop-down list ignores its colours unless it draws its own items.
    static readonly ConditionalWeakTable<ComboBox, object> drawn = new();
    static void DarkCombo(ComboBox cb)
    {
        cb.FlatStyle = FlatStyle.Flat;
        if (drawn.TryGetValue(cb, out _)) return;
        drawn.Add(cb, null);
        cb.DrawMode = DrawMode.OwnerDrawFixed;
        cb.DrawItem += (s, e) =>
        {
            bool sel = (e.State & DrawItemState.Selected) != 0 && (e.State & DrawItemState.ComboBoxEdit) == 0;
            Color back = Theme.IsDark ? (sel ? Theme.Selected : Theme.Input) : (sel ? SystemColors.Highlight : SystemColors.Window);
            Color fore = Theme.IsDark ? Theme.Text : (sel ? SystemColors.HighlightText : SystemColors.WindowText);
            using (var b = new SolidBrush(back)) e.Graphics.FillRectangle(b, e.Bounds);
            if (e.Index >= 0)
                TextRenderer.DrawText(e.Graphics, cb.GetItemText(cb.Items[e.Index]), cb.Font, new Rectangle(e.Bounds.X + 2, e.Bounds.Y, e.Bounds.Width - 2, e.Bounds.Height), fore,
                    TextFormatFlags.VerticalCenter | TextFormatFlags.Left | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
        };
    }

    sealed class DarkTable : ProfessionalColorTable
    {
        public override Color ToolStripGradientBegin => Theme.Bar;
        public override Color ToolStripGradientMiddle => Theme.Bar;
        public override Color ToolStripGradientEnd => Theme.Bar;
        public override Color ToolStripBorder => Theme.Bar;
        public override Color StatusStripGradientBegin => Theme.Bar;
        public override Color StatusStripGradientEnd => Theme.Bar;
        public override Color SeparatorDark => Color.FromArgb(70, 74, 82);
        public override Color SeparatorLight => Theme.Bar;
        public override Color ButtonSelectedHighlight => Color.FromArgb(60, 66, 78);
        public override Color ButtonSelectedGradientBegin => Color.FromArgb(60, 66, 78);
        public override Color ButtonSelectedGradientMiddle => Color.FromArgb(60, 66, 78);
        public override Color ButtonSelectedGradientEnd => Color.FromArgb(60, 66, 78);
        public override Color ButtonSelectedBorder => Color.FromArgb(90, 100, 120);
        public override Color ButtonPressedGradientBegin => Color.FromArgb(75, 84, 100);
        public override Color ButtonPressedGradientMiddle => Color.FromArgb(75, 84, 100);
        public override Color ButtonPressedGradientEnd => Color.FromArgb(75, 84, 100);
        public override Color ToolStripDropDownBackground => Theme.Card;
        public override Color ImageMarginGradientBegin => Theme.Card;
        public override Color ImageMarginGradientMiddle => Theme.Card;
        public override Color ImageMarginGradientEnd => Theme.Card;
        public override Color MenuItemSelected => Color.FromArgb(60, 66, 78);
        public override Color MenuItemBorder => Color.FromArgb(90, 100, 120);
        public override Color OverflowButtonGradientBegin => Theme.Bar;
        public override Color OverflowButtonGradientMiddle => Theme.Bar;
        public override Color OverflowButtonGradientEnd => Theme.Bar;
    }

    // ---------------- colour mapping (dark only)
    public static Color MapBack(Color c)
    {
        if (c.A == 0) return c;
        if (c == SystemColors.Control || c == SystemColors.ButtonFace) return Theme.Bar;
        RgbToHsl(c, out float h, out float s, out float l);
        if (l < 0.75f) return c;                          // already dark or a strong colour (buttons, pills)
        if (s < 0.2f || l > 0.97f) return l > 0.97f ? Theme.Card : Theme.Page;
        return Hsl(h, Math.Min(s, 0.35f), 0.19f);         // light tinted strip -> dark tint of the same hue
    }
    public static Color MapFore(Color c)
    {
        if (c.A == 0) return c;
        RgbToHsl(c, out float h, out float s, out float l);
        if (l >= 0.55f) return c;                         // already light (white text on buttons)
        if (s < 0.15f) { float g = Math.Clamp(1f - l * 0.75f, 0.62f, 0.9f); int v = (int)(g * 255); return Color.FromArgb(v, v, v + Math.Min(6, 255 - v)); }
        return Hsl(h, Math.Min(s, 0.75f), Math.Max(0.66f, 1f - l));
    }
    public static Color Lighter(Color c) { RgbToHsl(c, out float h, out float s, out float l); return Hsl(h, s, Math.Max(0.68f, l)); }

    static void RgbToHsl(Color c, out float h, out float s, out float l)
    {
        float r = c.R / 255f, g = c.G / 255f, b = c.B / 255f;
        float max = Math.Max(r, Math.Max(g, b)), min = Math.Min(r, Math.Min(g, b));
        l = (max + min) / 2; h = 0; s = 0;
        if (max == min) return;
        float d = max - min;
        s = l > 0.5f ? d / (2 - max - min) : d / (max + min);
        if (max == r) h = (g - b) / d + (g < b ? 6 : 0); else if (max == g) h = (b - r) / d + 2; else h = (r - g) / d + 4;
        h /= 6;
    }
    static Color Hsl(float h, float s, float l)
    {
        float q = l < 0.5f ? l * (1 + s) : l + s - l * s, p = 2 * l - q;
        float f(float t) { if (t < 0) t += 1; if (t > 1) t -= 1; return t < 1f / 6 ? p + (q - p) * 6 * t : t < 0.5f ? q : t < 2f / 3 ? p + (q - p) * (2f / 3 - t) * 6 : p; }
        return Color.FromArgb((int)(f(h + 1f / 3) * 255), (int)(f(h) * 255), (int)(f(h - 1f / 3) * 255));
    }
}
