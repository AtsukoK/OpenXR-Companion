using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Win32;

namespace OpenXRCompanionEditor;

static class Native
{
    [DllImport("OpenXRCompanion.dll", CallingConvention = CallingConvention.Cdecl)]
    static extern int vro_validate([MarshalAs(UnmanagedType.LPUTF8Str)] string text, byte[] err, int errLen);
    [DllImport("OpenXRCompanion.dll", CallingConvention = CallingConvention.Cdecl)]
    static extern IntPtr vro_default_profile();

    public static string Validate(string json)
    {
        try
        {
            var buf = new byte[1024];
            if (vro_validate(json, buf, buf.Length) != 0)
                return null;
            return Encoding.UTF8.GetString(buf, 0, Array.IndexOf(buf, (byte)0) is int n && n >= 0 ? n : buf.Length);
        }
        catch (Exception e) { return "(could not load OpenXRCompanion.dll to check: " + e.Message + ")"; }
    }
    [DllImport("OpenXRCompanion.dll", CallingConvention = CallingConvention.Cdecl)]
    static extern int vro_check_keys([MarshalAs(UnmanagedType.LPUTF8Str)] string text, int seq, byte[] err, int errLen);

    /// <summary>null = fine (or cannot check), otherwise the layer's reason.</summary>
    public static string CheckKeys(string text, bool sequence)
    {
        if (string.IsNullOrWhiteSpace(text))
            return null;
        try
        {
            var buf = new byte[512];
            if (vro_check_keys(text, sequence ? 1 : 0, buf, buf.Length) != 0)
                return null;
            int n = Array.IndexOf(buf, (byte)0);
            return Encoding.UTF8.GetString(buf, 0, n >= 0 ? n : buf.Length);
        }
        catch { return null; }
    }
    [DllImport("OpenXRCompanion.dll", CallingConvention = CallingConvention.Cdecl)]
    static extern void vro_prepare_defaults();
    // default.json = the built-in standard (refreshed); an older changed one becomes user_defaults.json (once).
    public static void PrepareDefaults() { try { vro_prepare_defaults(); } catch { } }
    public static string DefaultProfile()
    {
        try
        { return Marshal.PtrToStringUTF8(vro_default_profile()); }
        catch { return null; }
    }
}

public partial class MainForm : Form
{
    public static MainForm Current;
    public ProfileModel Profile;

    const string LayerJson = "XR_APILAYER_NOVENDOR_openxr_companion.json";
    const string ImplicitKey = @"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit";
    static readonly string DataDir = MigrateDataDir();
    // First run after the rename from VR Radial Overlay: copy the old settings folder across.
    static string MigrateDataDir()
    {
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var dir = Path.Combine(local, "OpenXRCompanion");
        // OpenXR Hand Menus first, before that VR Radial Overlay
        var old = Path.Combine(local, "OpenXRHandMenus");
        if (!Directory.Exists(old))
            old = Path.Combine(local, "VRRadialOverlay");
        try
        {
            if (!Directory.Exists(dir) && Directory.Exists(old))
                foreach (var f in Directory.GetFiles(old, "*", SearchOption.AllDirectories))
                {
                    var t = Path.Combine(dir, Path.GetRelativePath(old, f));
                    Directory.CreateDirectory(Path.GetDirectoryName(t));
                    File.Copy(f, t, false);
                }
        }
        catch { }
        return dir;
    }
    static readonly string ProfilesDir = Path.Combine(DataDir, "profiles");
    internal static string ProfilesPath => ProfilesDir;

    readonly ToolStripComboBox profileBox = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 220 };
    readonly ToolStripLabel layerStatus = new();
    readonly TreeView tree = new() { Dock = DockStyle.Fill, HideSelection = false, Font = new Font("Segoe UI", 10f) };
    readonly PropertyGrid grid = new() { Dock = DockStyle.Fill, PropertySort = PropertySort.Categorized, ToolbarVisible = false };
    readonly StatusStrip status = new();
    readonly ToolStripStatusLabel statusText = new() { Spring = true, TextAlign = ContentAlignment.MiddleLeft };
    string currentFile;
    bool dirty, loading;

    readonly Panel editorHost = new BufferedHost { Dock = DockStyle.Fill };
    // The page area: Windows double-buffers it as a whole (WS_EX_COMPOSITED), so a page appears in one go.
    sealed class BufferedHost : Panel
    {
        protected override CreateParams CreateParams { get { var cp = base.CreateParams; cp.ExStyle |= 0x02000000; return cp; } }
    }
    [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr h, int msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] static extern bool RedrawWindow(IntPtr h, IntPtr rc, IntPtr rgn, uint flags);
    // Stop a control (and everything in it) from painting, then paint it once when done.
    static void Redraw(Control c, bool on, bool erase = true)
    {
        if (!c.IsHandleCreated)
            return;
        SendMessage(c.Handle, 0x000B /*WM_SETREDRAW*/, on ? (IntPtr)1 : IntPtr.Zero, IntPtr.Zero);
        if (on)
            RedrawWindow(c.Handle, IntPtr.Zero, IntPtr.Zero, 0x0001 | (erase ? 0x0004u : 0) | 0x0080 | 0x0100 | 0x0400 /*INVALIDATE|ERASE|ALLCHILDREN|UPDATENOW|FRAME*/);
    }
    Stack<object> history = new();
    object lastTag;
    bool navigatingBack;
    Button backBtn, homeBtn, dupBtn, delBtn, upBtn, downBtn;

    // Big, easy to spot navigation above every page.
    Control BuildNavBar()
    {
        var bar = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 46, Padding = new Padding(8, 6, 8, 4), BackColor = Color.FromArgb(232, 238, 248), WrapContents = false };
        Button Nav(string text, Color back)
        {
            var b = new Button { Text = text, AutoSize = true, Height = 32, FlatStyle = FlatStyle.Flat, BackColor = back, ForeColor = Color.White, Font = new Font("Segoe UI", 10f, FontStyle.Bold), Margin = new Padding(0, 0, 8, 0), Padding = new Padding(8, 0, 8, 0), Cursor = Cursors.Hand };
            b.FlatAppearance.BorderSize = 0;
            return b;
        }
        backBtn = Nav("\u25C0  Back", Color.FromArgb(70, 90, 120));
        homeBtn = Nav("\u2302  Overview", Color.FromArgb(40, 90, 170));
        backBtn.Click += (s, e) => GoBack();
        homeBtn.Click += (s, e) => GoTo("overview");
        bar.Controls.Add(backBtn);
        bar.Controls.Add(homeBtn);
        Button Act(string text, Action a)
        {
            var b = new Button { Text = text, AutoSize = true, Height = 32, FlatStyle = FlatStyle.Flat, BackColor = Color.White, ForeColor = Color.FromArgb(40, 50, 70), Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), Margin = new Padding(0, 0, 6, 0), Cursor = Cursors.Hand };
            b.FlatAppearance.BorderColor = Color.FromArgb(190, 200, 215);
            b.Click += (s, e) => a();
            return b;
        }
        bar.Controls.Add(new Label { Width = 14 });
        dupBtn = Act("Duplicate", Duplicate);
        delBtn = Act("Delete", Remove);
        upBtn = Act("\u25B2 Up", () => MoveItem(-1));
        downBtn = Act("\u25BC Down", () => MoveItem(1));
        delBtn.ForeColor = Color.FromArgb(170, 40, 40);
        bar.Controls.AddRange(new Control[] { dupBtn, delBtn, upBtn, downBtn });
        UpdateNav();
        return bar;
    }
    void UpdateNav()
    {
        if (dupBtn != null)
        {
            var tag = tree.SelectedNode?.Tag;
            string what = tag switch
            {
                BindingModel => "binding",
                WheelItemModel => InWrist(tree.SelectedNode) ? "button" : "item",
                WheelModel => InWrist(tree.SelectedNode) ? "page" : "wheel",
                _ => null,
            };
            bool movable = tag is BindingModel or WheelItemModel;
            dupBtn.Enabled = delBtn.Enabled = what != null;
            upBtn.Enabled = downBtn.Enabled = movable;
            dupBtn.Text = what != null ? "Duplicate " + what : "Duplicate";
            delBtn.Text = what != null ? "Delete " + what : "Delete";
        }
        if (backBtn != null)
            backBtn.Enabled = history.Count > 0;
        if (homeBtn != null)
            homeBtn.Enabled = lastTag as string != "overview";
    }
    public void GoTo(object tag)
    {
        var n = Find(tree.Nodes, tag);
        if (n != null)
        { tree.SelectedNode = n; n.EnsureVisible(); }
    }
    void GoBack()
    {
        while (history.Count > 0)
        {
            var tag = history.Pop();
            var n = Find(tree.Nodes, tag);
            if (n == null)
                continue; // deleted since
            navigatingBack = true;
            try
            { tree.SelectedNode = n; n.EnsureVisible(); }
            finally { navigatingBack = false; }
            break;
        }
        UpdateNav();
    }
    readonly Button addBtn = new() { Text = "Add", AutoSize = true };
    Control currentEditor;

    static readonly JsonSerializerOptions JsonOpts = new() { Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping, WriteIndented = true, PropertyNameCaseInsensitive = true, ReadCommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true };

    public MainForm()
    {
        Current = this;
        Text = "OpenXR Companion - Profile Editor";
        Size = new Size(1150, 760);
        Font = new Font("Segoe UI", 10.5f);
        Directory.CreateDirectory(ProfilesDir);

        var ts = new ToolStrip { GripStyle = ToolStripGripStyle.Hidden, Padding = new Padding(4) };
        ts.Items.Add(new ToolStripLabel("Profile:"));
        ts.Items.Add(profileBox);
        ts.Items.Add(Btn("New game profile...", NewProfile));
        ts.Items.Add(Btn("Delete profile", DeleteProfile));
        ts.Items.Add(new ToolStripSeparator());
        ts.Items.Add(Btn("Controllers", () => ChooseControllers(false)));
        ts.Items.Add(Btn("Tutorial", StartTutorial));
        ts.Items.Add(new ToolStripSeparator());
        ts.Items.Add(Btn("Save (Ctrl+S)", () => Save()));
        ts.Items.Add(Btn("Revert", () => LoadFile(currentFile)));
        ts.Items.Add(new ToolStripSeparator());
        ts.Items.Add(Btn("Open folder", () => Process.Start("explorer.exe", ProfilesDir)));
        ts.Items.Add(Btn("Logs", () => { Directory.CreateDirectory(Path.Combine(DataDir, "logs")); Process.Start("explorer.exe", Path.Combine(DataDir, "logs")); }));
        ts.Items.Add(new ToolStripSeparator());
        ts.Items.Add(Btn("Install layer", () => RunScript("install.ps1")));
        ts.Items.Add(Btn("Uninstall layer", () => RunScript("uninstall.ps1")));
        ts.Items.Add(layerStatus);
        {   // Headset mirror: always one click away, whatever page is open (first on the toolbar so it never overflows)
            var mb = new ToolStripButton { CheckOnClick = false, DisplayStyle = ToolStripItemDisplayStyle.Text, Font = new Font(Font, FontStyle.Bold) };
            void ShowMirror()
            {
                bool on = HeadsetMirrorFile.On;
                mb.Text = on ? "● Headset mirror: ON" : "○ Headset mirror: off";
                mb.ForeColor = on ? Color.ForestGreen : Color.DimGray;
                mb.ToolTipText = "A window on your desktop showing what the headset shows, with the wheel, wrist panels, screen and lasers in it " +
                                 "(the game's own mirror can't show those). Turns on in a running game within a second; closing the window turns it off. " +
                                 "Eye, zoom and frame rate: Settings > Headset mirror. D3D11 games.";
            }
            mb.Click += (s, e) => { HeadsetMirrorFile.On = !HeadsetMirrorFile.On; ShowMirror(); };
            var mt = new System.Windows.Forms.Timer { Interval = 1000 }; // the layer turns it off when its window is closed
            mt.Tick += (s, e) => ShowMirror();
            mt.Start();
            ShowMirror();
            ts.Items.Insert(0, mb);
            ts.Items.Insert(1, new ToolStripSeparator());
        }
        {   // light / dark: Auto follows Windows
            var tb = new ToolStripButton { Alignment = ToolStripItemAlignment.Right, DisplayStyle = ToolStripItemDisplayStyle.Text };
            void ShowT()
            { tb.Text = ThemeMode.Choice switch { "dark" => "☾ Dark", "light" => "☀ Light", _ => "◐ Auto" }; tb.ToolTipText = "Light or dark look. Auto follows your Windows setting. Click to switch."; }
            tb.Click += (s, e) =>
            {
                ThemeMode.Choice = ThemeMode.Choice switch { "dark" => "light", "light" => "auto", _ => "dark" };
                ShowT();
                Theme.Set(ThemeMode.IsDark);
                ThemeWalker.Apply(this);
                tree.BackColor = Theme.Surface;
                tree.Invalidate();
                ShowEditor(tree.SelectedNode);
            };
            ShowT();
            ts.Items.Add(tb);
        }
        AddUpdateButton(ts);
        profileBox.SelectedIndexChanged += (s, e) => { if (!loading && profileBox.SelectedItem is string f) { if (ConfirmDiscard()) { LoadFile(Path.Combine(ProfilesDir, f)); ActiveProfile = f; SetStatus("Games now use " + f + " (a running game switches within a second).", false); } } };

        var left = new Panel { Dock = DockStyle.Fill };
        addBtn.Click += (s, e) => Add();
        left.Controls.Add(tree);
        var searchWrap = new Panel { Dock = DockStyle.Top, Height = 40, Padding = new Padding(6, 6, 6, 4), BackColor = Color.FromArgb(248, 249, 251) };
        searchWrap.Controls.Add(search);
        left.Controls.Add(searchWrap);


        var split = new SplitContainer { Dock = DockStyle.Fill, SplitterDistance = 430 };
        split.Panel1.Controls.Add(left);
        split.Panel2.Controls.Add(editorHost);
        split.Panel2.Controls.Add(BuildNavBar());

        status.Items.Add(statusText);
        {   // credit, bottom right on every page
            var credit = new ToolStripStatusLabel("Created by Atsuko")
            {
                IsLink = true,
                LinkBehavior = LinkBehavior.HoverUnderline,
                Font = new Font(Font, FontStyle.Bold),
                ToolTipText = "OpenXR Companion v" + AppVersion.ToString(3) + " - github.com/AtsukoK/OpenXR-Companion\nSupport: patreon.com/cw/AtsukoK and ko-fi.com/atsukok",
            };
            credit.Click += (s, e) => Process.Start(new ProcessStartInfo("https://www.patreon.com/cw/AtsukoK") { UseShellExecute = true });
            status.Items.Add(credit);
        }
        Controls.Add(split);
        Controls.Add(ts);
        Controls.Add(status);
        split.SplitterDistance = 430;

        tree.AfterSelect += (s, e) =>
        {
            // Remember where we came from so Back can return there.
            if (!navigatingBack && lastTag != null && !ReferenceEquals(lastTag, e.Node?.Tag))
            { history.Push(lastTag); if (history.Count > 50) history = new Stack<object>(history.Take(50).Reverse()); }
            lastTag = e.Node?.Tag;
            ShowEditor(e.Node);
            tree.Invalidate();
            UpdateNav();
        };
        SetupTreeLook();
        tree.KeyDown += (s, e) =>
        {
            if (e.KeyCode == Keys.Delete)
            { Remove(); e.Handled = true; }
            else if (e.Control && e.KeyCode == Keys.D)
            { Duplicate(); e.Handled = true; }
            else if (e.Alt && e.KeyCode == Keys.Up)
            { MoveItem(-1); e.Handled = true; }
            else if (e.Alt && e.KeyCode == Keys.Down)
            { MoveItem(1); e.Handled = true; }
        };
        var menu = new ContextMenuStrip { Font = new Font("Segoe UI", 10f) };
        tree.NodeMouseClick += (s, e) =>
        {
            if (e.Button != MouseButtons.Right || e.Node.Tag is AddSlot)
                return;
            tree.SelectedNode = e.Node;
            menu.Items.Clear();
            string add = e.Node.Tag switch
            {
                "bindings_right" or "bindings_left" or BindingModel => "Add binding",
                "wheels" => "New wheel",
                "wrist0" or "wrist1" => "New page",
                WheelModel or WheelItemModel => InWrist(e.Node) ? "Add button" : "Add item",
                _ => null,
            };
            if (add != null)
                menu.Items.Add(add, null, (o, a) => Add());
            if (dupBtn.Enabled)
            { menu.Items.Add(dupBtn.Text + "   (Ctrl+D)", null, (o, a) => Duplicate()); menu.Items.Add(delBtn.Text + "   (Del)", null, (o, a) => Remove()); }
            if (upBtn.Enabled)
            { menu.Items.Add("Move up   (Alt+\u2191)", null, (o, a) => MoveItem(-1)); menu.Items.Add("Move down   (Alt+\u2193)", null, (o, a) => MoveItem(1)); }
            if (menu.Items.Count > 0)
                menu.Show(tree, e.Location);
        };
        SetupDragDrop();
        grid.PropertyValueChanged += (s, e) => { MarkDirty(); RefreshNodeTexts(); };
        KeyPreview = true;
        KeyDown += (s, e) => { if (e.Control && e.KeyCode == Keys.S && !KeyBox.Recording) { Save(); e.SuppressKeyPress = true; } };
        FormClosing += (s, e) => { if (!ConfirmDiscard()) e.Cancel = true; };

        UpdateLayerStatus();
        Native.PrepareDefaults(); // before choosing: this may move an edited default.json to user_defaults.json (and select it)
        RefreshProfileList(ActiveProfile);
        if (Theme.IsDark)
            ThemeWalker.Apply(this);
        HandleCreated += (s, e) => ThemeWalker.TitleBar(this, Theme.IsDark);
        Shown += (s, e) => ShowDefaultsMovedNotice();
    }

    // Once, after the update that split default.json: tell the user their own bindings moved to user_defaults.json.
    // The layer (or this editor) leaves the notice file when it moves an edited default.json.
    void ShowDefaultsMovedNotice()
    {
        string notice = Path.Combine(ProfilesDir, ".defaults_moved_notice");
        if (Environment.GetCommandLineArgs().Length > 1 && Environment.GetCommandLineArgs()[1] != "--updated")
            return; // tests and screenshots
        if (!File.Exists(notice))
            return;
        bool selected = string.Equals(Path.GetFileName(currentFile), UserDefaultsFile, StringComparison.OrdinalIgnoreCase);
        MessageBox.Show(this,
            "Your own settings have moved to a new profile: user_defaults.json\n\n" +
            "Your default.json had your own bindings and changes in it. To keep them safe from future updates, they are now in user_defaults.json, exactly as they were. Nothing was lost.\n\n" +
            (selected ? "user_defaults.json is now your selected profile, and your games use it" : "Select user_defaults.json in the profile list and your games use it") +
            ", so everything works as before.\n\n" +
            "default.json is now the standard profile. It updates with each new version, so you can always look at or go back to the standard setup. " +
            "If you save changes while default.json is open, they go to a profile of your own; default.json itself is never changed.\n\n" +
            "The profile you select in the editor is always the one your games use. To use the standard, select default.json.",
            "Your settings moved to user_defaults.json", MessageBoxButtons.OK, MessageBoxIcon.Information);
        try
        { File.Delete(notice); }
        catch { }
    }

    // Timing of the slow parts (opening each page, rebuilding the list, moving an item), on a copy of a profile.
    public static void Perf(string profile, string outFile)
    {
        var sb = new StringBuilder();
        var f = new MainForm { StartPosition = FormStartPosition.Manual, Location = new Point(0, 0), Size = new Size(1500, 1000) };
        f.Show();
        for (int i = 0; i < 10; i++)
            Application.DoEvents();
        var copy = Path.Combine(Path.GetTempPath(), "vro_perf_profile.json");
        File.Copy(profile, copy, true);
        var sw = System.Diagnostics.Stopwatch.StartNew();
        f.LoadFile(copy);
        for (int i = 0; i < 5; i++)
            Application.DoEvents();
        sb.AppendLine($"load file: {sw.ElapsedMilliseconds} ms");
        double Time(Action a)
        { var t = System.Diagnostics.Stopwatch.StartNew(); a(); for (int i = 0; i < 3; i++) Application.DoEvents(); f.Update(); return t.Elapsed.TotalMilliseconds; }
        var nodes = new List<TreeNode>();
        void All(TreeNodeCollection c)
        { foreach (TreeNode n in c) { if (n.Tag is not AddSlot) nodes.Add(n); All(n.Nodes); } }
        All(f.tree.Nodes);
        foreach (var round in new[] { "first", "again" })
            foreach (var n in nodes)
            {
                PerfLog = new StringBuilder();
                double ms = Time(() => f.tree.SelectedNode = n);
                sb.AppendLine($"{round,-6} select {(n.Tag?.GetType().Name ?? "-"),-16} {n.Text,-34} {ms,7:F0} ms  |{PerfLog}");
                PerfLog = null;
            }
        {   // building blocks, 5 times each
            double T(Action a)
            { var t = System.Diagnostics.Stopwatch.StartNew(); for (int i = 0; i < 5; i++) a(); return t.Elapsed.TotalMilliseconds / 5; }
            var w0 = f.Profile.Wheels.Values.First();
            var it0 = w0.Items.First();
            sb.AppendLine($"  new ActionEditor: {T(() => new ActionEditor(true).Dispose()):F0} ms");
            sb.AppendLine($"  new ItemEditor: {T(() => new ItemEditor(w0, it0).Dispose()):F0} ms");
            sb.AppendLine($"  new BindingEditor: {T(() => new BindingEditor(f.Profile.Bindings[0]).Dispose()):F0} ms");
            sb.AppendLine($"  new SettingsEditor general: {T(() => new SettingsEditor(f.Profile).Dispose()):F0} ms");
            sb.AppendLine($"  Monitors.Choices: {T(() => Monitors.Choices(true)):F1} ms");
            sb.AppendLine($"  ChoiceBox(40 items): {T(() => new ChoiceBox(Enumerable.Range(0, 40).Select(i => new Choice(i.ToString(), "x" + i))).Dispose()):F1} ms");
            sb.AppendLine($"  new KeyBox: {T(() => new KeyBox().Dispose()):F1} ms");
            sb.AppendLine($"  new ChordEditor: {T(() => new ChordEditor().Dispose()):F1} ms");
            var ie = new ItemEditor(w0, it0);
            f.editorHost.Controls.Add(ie);
            sb.AppendLine($"  theme walk item page: {T(() => ThemeWalker.Apply(ie)):F1} ms");
            f.editorHost.Controls.Remove(ie);
            ie.Dispose();
        }
        sb.AppendLine($"rebuild list: {Time(() => f.BuildTree()):F0} ms");
        var item = nodes.FirstOrDefault(n => n.Tag is WheelItemModel);
        if (item != null)
        { f.tree.SelectedNode = item; sb.AppendLine($"move item down: {Time(() => f.MoveItem(1)):F0} ms"); sb.AppendLine($"move item up: {Time(() => f.MoveItem(-1)):F0} ms"); }
        {   // the list must stay where it was scrolled when an item moves
            var items = f.VisibleRows().Where(n => n.Tag is WheelItemModel).ToList();
            if (items.Count > 3)
            {
                var it = items[items.Count / 2];
                f.tree.SelectedNode = it;
                var rows = f.VisibleRows();
                f.tree.TopNode = rows[Math.Max(0, rows.IndexOf(it) - 4)];
                for (int i = 0; i < 3; i++)
                    Application.DoEvents();
                int before = f.VisibleRows().IndexOf(f.tree.TopNode);
                f.MoveItem(-1);
                for (int i = 0; i < 3; i++)
                    Application.DoEvents();
                int after = f.VisibleRows().IndexOf(f.tree.TopNode);
                sb.AppendLine($"list scroll kept on move: top row {before} -> {after} {(before == after ? "OK" : "MOVED")}");
                f.MoveItem(1);
            }
        }
        var bind = nodes.FirstOrDefault(n => n.Tag is BindingModel);
        if (bind != null)
        { f.tree.SelectedNode = bind; sb.AppendLine($"move binding: {Time(() => f.MoveItem(1)):F0} ms"); }
        sb.AppendLine($"search 'a': {Time(() => { f.search.Text = "a"; f.BuildTree(); }):F0} ms");
        sb.AppendLine($"search clear: {Time(() => { f.search.Text = ""; f.BuildTree(); }):F0} ms");
        File.WriteAllText(outFile, sb.ToString());
        f.dirty = false;
        f.Close();
    }

    // Headless screenshots of each editor page (for checking layout).
    public static void Shots(string dir)
    {
        Directory.CreateDirectory(dir);
        var f = new MainForm { StartPosition = FormStartPosition.Manual, Location = new Point(0, 0), Size = new Size(1500, 1000) };
        f.Show();
        for (int i = 0; i < 10; i++)
            Application.DoEvents();
        // Screenshots show the app's built-in default profile, never the user's own (nothing is saved).
        var builtin = Path.Combine(Path.GetTempPath(), "vro_builtin_default.json");
        File.WriteAllText(builtin, Native.DefaultProfile() ?? "{}");
        f.LoadFile(builtin);
        void Snap(string name, TreeNode n)
        {
            if (n == null)
                return;
            f.tree.SelectedNode = n;
            for (int i = 0; i < 5; i++)
                Application.DoEvents();
            using var bmp = new Bitmap(f.Width, f.Height);
            f.DrawToBitmap(bmp, new Rectangle(0, 0, f.Width, f.Height));
            bmp.Save(Path.Combine(dir, name + ".png"));
        }
        {   // the whole Settings page at once (it scrolls in the app)
            var se = new SettingsEditor(f.Profile) { Size = new Size(1000, 5200), AutoScroll = false };
            se.CreateControl();
            se.PerformLayout();
            for (int i = 0; i < 10; i++)
                Application.DoEvents();
            int hgt = 0;
            foreach (Control c in se.Controls)
                hgt = Math.Max(hgt, c.Bottom);
            using var bmp = new Bitmap(se.Width, Math.Max(200, Math.Min(hgt + 10, 5200)));
            se.DrawToBitmap(bmp, new Rectangle(0, 0, bmp.Width, bmp.Height));
            bmp.Save(Path.Combine(dir, "settings_full.png"));
        }
        {   // every arm pose picture, both arms, side by side
            string[] modes = { "fingers", "between", "along_arm" };
            using var sheet = new Bitmap(380 * 3, 300 * 2);
            for (int arm = 0; arm < 2; arm++)
                for (int m = 0; m < 3; m++)
                {
                    using var pv = new ArmPosePreview { Mode = modes[m], RightArm = arm == 1 };
                    using var b = new Bitmap(pv.Width, pv.Height);
                    pv.DrawToBitmap(b, new Rectangle(0, 0, pv.Width, pv.Height));
                    using var gg = Graphics.FromImage(sheet);
                    gg.DrawImage(b, m * 380, arm * 300);
                }
            sheet.Save(Path.Combine(dir, "0_arm_poses.png"));
        }
        {   // every small picture used on the Overview, on and off
            var pics = new List<Bitmap>
            {
                MiniPics.Headset(96, 72, true), MiniPics.Wheel(96, 72, 1, null, true), MiniPics.Wheel(96, 72, 0, null, false),
                MiniPics.Wrist(96, 72, 0, null, "ON"), MiniPics.Wrist(96, 72, 1, null, "MIRRORED"), MiniPics.Wrist(96, 72, 1, null, "OFF"),
                MiniPics.Keyboard(96, 72, true), MiniPics.Controllers(96, 72, true),
            };
            using var sheet2 = new Bitmap(pics.Count * 104, 80);
            using (var gg = Graphics.FromImage(sheet2))
            { gg.Clear(Color.White); for (int i = 0; i < pics.Count; i++) gg.DrawImage(pics[i], i * 104 + 4, 4); }
            sheet2.Save(Path.Combine(dir, "0_minipics.png"));
            foreach (var b in pics)
                b.Dispose();
        }
        Snap("00_overview", f.tree.Nodes[0]);
        Snap("1_settings", f.tree.Nodes[1]);
        foreach (TreeNode pn in f.tree.Nodes[1].Nodes)
            Snap("1_settings_" + ((SettingsPage)pn.Tag).Key, pn);
        f.search.Text = "zoom";
        for (int i = 0; i < 40; i++)
        { Application.DoEvents(); System.Threading.Thread.Sleep(10); }
        f.BuildTree();
        Snap("1_search_zoom", f.tree.Nodes[1].Nodes.Count > 0 ? f.tree.Nodes[1].Nodes[0] : f.tree.Nodes[1]);
        f.search.Text = "";
        f.BuildTree();
        Snap("2a_controller_right", f.bindingRoots[1]);
        Snap("2a_controller_left", f.bindingRoots[0]);
        ControllerButtonsPage.StartWith(1, "primary");
        Snap("2a_controller_right_a", f.bindingRoots[1]);
        ControllerButtonsPage.StartWith(1, "thumbstick");
        Snap("2_binding", f.bindingRoots[1].Nodes.Count > 0 ? f.bindingRoots[1].Nodes[0] : null);
        Snap("3_binding_chord", f.bindingRoots[1].Nodes.Count > 1 ? f.bindingRoots[1].Nodes[1] : null);
        Snap("4_wheel", f.wheelsRoot.Nodes.Count > 0 ? f.wheelsRoot.Nodes[0] : null);
        Snap("4b_wheel_last", f.wheelsRoot.Nodes.Count > 1 ? f.wheelsRoot.Nodes[f.wheelsRoot.Nodes.Count - 2] : null);
        Snap("6_wrist", f.wristRoots[0]);
        Snap("7_wrist_page", f.wristRoots[0].Nodes.Count > 0 ? f.wristRoots[0].Nodes[0] : null);
        Snap("8_wrist_button", f.wristRoots[0].Nodes.Count > 0 && f.wristRoots[0].Nodes[0].Nodes.Count > 2 ? f.wristRoots[0].Nodes[0].Nodes[2] : null);
        Snap("9_wrist_right", f.wristRoots[1]);
        Snap("5_item", f.wheelsRoot.Nodes.Count > 0 && f.wheelsRoot.Nodes[0].Nodes.Count > 0 ? f.wheelsRoot.Nodes[0].Nodes[0] : null);
        f.Fold(f.bindingRoots[1]);
        f.Fold(f.wheelsRoot);
        Snap("10_folded", f.tree.Nodes[0]);
        // Drop logic check (in memory only, never saved).
        var log = new StringBuilder();
        var b0 = (BindingModel)f.bindingRoots[1].Nodes[0].Tag;
        f.DoDrop(f.bindingRoots[1].Nodes[0], f.bindingRoots[1].Nodes[1], true);
        log.AppendLine("binding 0 dropped after binding 1 -> now at index " + f.Profile.Bindings.IndexOf(b0) + " (expect 1)");
        var w0 = f.wheelsRoot.Nodes[0];
        var w1 = f.wheelsRoot.Nodes[1];
        var it0 = (WheelItemModel)w0.Nodes[0].Tag;
        int before0 = ((WheelModel)w0.Tag).Items.Count, before1 = ((WheelModel)w1.Tag).Items.Count;
        f.DoDrop(Find(f.tree.Nodes, it0), f.wheelsRoot.Nodes[1], true);
        log.AppendLine($"item moved to other wheel -> source {before0}->{((WheelModel)w0.Tag).Items.Count}, target {before1}->{((WheelModel)w1.Tag).Items.Count}, last={ReferenceEquals(((WheelModel)w1.Tag).Items.Last(), it0)}");
        var tgtFirst = Find(f.tree.Nodes, ((WheelModel)f.wheelsRoot.Nodes[1].Tag).Items[0]);
        f.DoDrop(Find(f.tree.Nodes, it0), tgtFirst, false);
        log.AppendLine("item dropped before first -> index " + ((WheelModel)f.wheelsRoot.Nodes[1].Tag).Items.IndexOf(it0) + " (expect 0)");
        File.WriteAllText(Path.Combine(dir, "drop.txt"), log.ToString());
        f.dirty = false;
        f.Close();
    }
}
