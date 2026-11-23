namespace OpenXRCompanionEditor;

// The list on the left folds into a narrow bar of icons (« / »): one icon per section, a tooltip with its name, and
// the section you are in lit up. Folded or not is remembered between runs.
public partial class MainForm
{
    const int RailWidth = 64;
    FlowLayoutPanel rail;
    Panel listPanel;
    SplitContainer split2;
    int listWidth = 430;
    readonly List<(Button b, Func<object> tag)> railButtons = new();
    static string RailFile => Path.Combine(DataDir, "list_folded.txt");
    bool listFolded;

    // Called once the split and the list exist.
    void SetupRail(SplitContainer split, Panel left, Panel searchWrap)
    {
        split2 = split;
        listPanel = left;
        rail = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false, Visible = false, Padding = new Padding(4, 6, 0, 0), AutoScroll = true };
        split.Panel1.Controls.Add(rail);

        // « on the list's search row folds it
        var fold = new Button { Text = "«", Dock = DockStyle.Right, Width = 32, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, Font = new Font("Segoe UI", 12f, FontStyle.Bold) };
        fold.FlatAppearance.BorderSize = 0;
        FormPanel.Tips.SetToolTip(fold, "Fold the list into icons");
        fold.Click += (s, e) => SetFolded(true);
        searchWrap.Controls.Add(fold);

        RailButton("»", "Show the full list", () => SetFolded(false), null);
        RailButton("", "Overview", () => GoTo("overview"), () => "overview");
        RailButton("", "Settings", () => GoTo(Profile), () => Profile);
        RailButton("", "Controllers", () => GoTo(ControllersPage), () => ControllersPage);
        RailButton("", "Wheels", () => GoTo("wheels"), () => "wheels");
        RailButton("", "Left wrist panel", () => GoTo("wrist0"), () => "wrist0", "L");
        RailButton("", "Right wrist panel", () => GoTo("wrist1"), () => "wrist1", "R");
        RailButton("", "Search", () => { SetFolded(false); search.Focus(); }, null);

        try { listFolded = File.Exists(RailFile) && File.ReadAllText(RailFile).Trim() == "1"; } catch (IOException) { }
        if (listFolded)
            Shown += (s, e) => SetFolded(true);
    }

    void RailButton(string glyph, string name, Action click, Func<object> tag, string badge = null)
    {
        var b = new Button
        {
            Size = new Size(RailWidth - 10, 52), FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, Margin = new Padding(0, 0, 0, 4),
            Font = new Font("Segoe MDL2 Assets", 16f), Text = glyph, TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Theme.IsDark ? Theme.Card : Color.FromArgb(240, 243, 248), ForeColor = Theme.IsDark ? Theme.Text : Color.FromArgb(40, 60, 100),
        };
        b.FlatAppearance.BorderSize = 0;
        if (glyph == "»")
            b.Font = new Font("Segoe UI", 14f, FontStyle.Bold);
        if (badge != null) // L / R in the corner: which hand
            b.Paint += (s, e) =>
            {
                using var f = new Font("Segoe UI", 8f, FontStyle.Bold);
                using var br = new SolidBrush(b.ForeColor);
                e.Graphics.DrawString(badge, f, br, b.Width - 14, b.Height - 17);
            };
        FormPanel.Tips.SetToolTip(b, name);
        b.Click += (s, e) => click();
        rail.Controls.Add(b);
        railButtons.Add((b, tag));
    }

    void SetFolded(bool folded)
    {
        if (split2 == null)
            return;
        if (folded && !listFolded)
            listWidth = Math.Max(240, split2.SplitterDistance);
        listFolded = folded;
        split2.SuspendLayout();
        listPanel.Visible = !folded;
        rail.Visible = folded;
        split2.IsSplitterFixed = folded;
        split2.FixedPanel = folded ? FixedPanel.Panel1 : FixedPanel.None;
        split2.Panel1MinSize = folded ? RailWidth : 25;
        split2.SplitterDistance = folded ? RailWidth : listWidth;
        split2.ResumeLayout();
        UpdateRail();
        try { File.WriteAllText(RailFile, folded ? "1" : "0"); } catch (IOException) { }
    }

    // The icon of the section you are in is lit.
    void UpdateRail()
    {
        if (rail == null || !rail.Visible)
            return;
        var n = tree.SelectedNode;
        while (n?.Parent != null && n.Parent.Tag is not ProfileModel)
            n = n.Parent;
        object section = lastTag as string == ControllersPage || n?.Tag as string is "bindings_left" or "bindings_right" ? ControllersPage
                       : n?.Tag is SettingsPage ? Profile : n?.Tag;
        foreach (var (b, tag) in railButtons)
        {
            bool on = tag != null && Equals(tag(), section);
            b.BackColor = on ? Color.FromArgb(40, 110, 200) : Theme.IsDark ? Theme.Card : Color.FromArgb(240, 243, 248);
            b.ForeColor = on ? Color.White : Theme.IsDark ? Theme.Text : Color.FromArgb(40, 60, 100);
        }
    }

    // Pictures of the list folded and open (built-in default profile, nothing saved; the folded state is put back).
    public static void RailShot(string dir)
    {
        Directory.CreateDirectory(dir);
        string before = File.Exists(RailFile) ? File.ReadAllText(RailFile) : null;
        var f = new MainForm { StartPosition = FormStartPosition.Manual, Location = new Point(0, 0), Size = new Size(1920, 1040) };
        f.Show();
        var builtin = Path.Combine(Path.GetTempPath(), "vro_builtin_default.json");
        File.WriteAllText(builtin, Native.DefaultProfile() ?? "{}");
        f.LoadFile(Environment.GetEnvironmentVariable("VRO_SHOTS_PROFILE") is string shown && File.Exists(shown) ? shown : builtin);
        void Snap(string name)
        {
            for (int i = 0; i < 10; i++)
                Application.DoEvents();
            using var bmp = new Bitmap(f.Width, f.Height);
            f.DrawToBitmap(bmp, new Rectangle(0, 0, f.Width, f.Height));
            bmp.Save(Path.Combine(dir, name + ".png"));
        }
        f.GoTo("wheels");
        f.SetFolded(true);
        Snap("wheels_folded");
        f.SetFolded(false);
        Snap("list_open");
        f.SetFolded(true);
        Snap("list_folded");
        f.GoTo(f.Profile);
        Snap("settings_tiles");
        f.GoTo("overview");
        Snap("overview");
        if (f.Profile.Wheels.Values.FirstOrDefault() is WheelModel firstWheel)
        {
            f.GoTo(firstWheel);
            if (f.currentEditor is WheelPageEditor wpe) { wpe.EditFirstItem(); Snap("wheel_page"); wpe.ShowOpeners(); Snap("wheel_openers"); }
        }
        f.GoTo("wrist0");
        Snap("wrist_left");
        if (f.currentEditor is SettingsEditor low)
        { low.AutoScrollPosition = new Point(0, 99999); Snap("wrist_left_lower"); low.AutoScrollPosition = new Point(0, 0); }
        if (f.currentEditor is SettingsEditor se && se.EditFirstButton != null)
        { se.EditFirstButton(); Snap("wrist_left_editing"); se.ShowAdvanced?.Invoke(); Snap("wrist_left_advanced"); }
        using (var cones = new WristCones { GazeDeg = 20, FacingDeg = 35 })
        using (var bmp = new Bitmap(cones.Width, cones.Height))
        { cones.DrawToBitmap(bmp, new Rectangle(0, 0, cones.Width, cones.Height)); bmp.Save(Path.Combine(dir, "cones.png")); }
        f.GoTo(ControllersPage);
        Snap("controllers_folded");
        if (f.currentEditor is ControllersHub hub)
        { hub.OpenPart(1, "trigger"); Snap("controllers_drawer"); }
        f.SetFolded(false);
        Snap("controllers_list");
        f.Close();
        if (before == null) File.Delete(RailFile); else File.WriteAllText(RailFile, before);
    }
}
