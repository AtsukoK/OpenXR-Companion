using System.Drawing;

namespace OpenXRCompanionEditor;

// "Which controllers do you use?" Sets the buttons that open the two main wheels to ones that
// exist on that controller. Asked once on first run, and from the "Controllers" toolbar button.
public partial class MainForm
{
    static string ControllerFlag => Path.Combine(DataDir, "controller_chosen");

    static readonly (string id, string label, string how)[] ControllerKinds =
    {
        ("touch", "Meta Quest / Rift, Pico (Touch-style controllers)",
                  "Double-tap the thumbrest (just touch it, no press) to open that hand's wheel."),
        ("index", "Valve Index, Pimax Crystal, Bigscreen (Index-style)",
                  "Double-click the thumbstick (press it in twice quickly) to open that hand's wheel."),
        ("frame", "Steam Frame",
                  "Double-click the thumbstick (press it in twice quickly) to open that hand's wheel."),
        ("wmr",   "Windows Mixed Reality / HP Reverb",
                  "Hold the menu button to open that hand's wheel, point the stick, let go to pick. A quick tap still reaches the game."),
        ("vive",  "HTC Vive / Vive Pro wands",
                  "Hold the menu button to open that hand's wheel, point at an item on the trackpad, let go to pick. A quick tap still reaches the game."),
    };

    void AskControllerOnFirstRun()
    {
        if (File.Exists(ControllerFlag) || Environment.GetCommandLineArgs().Length > 1)
        { OfferTutorialOnFirstRun(); return; }
        ChooseControllers(firstRun: true);
        OfferTutorialOnFirstRun();
    }

    void ChooseControllers(bool firstRun)
    {
        if (Profile == null)
            return;
        using var dlg = new Form { Text = "Your controllers", FormBorderStyle = FormBorderStyle.FixedDialog, StartPosition = FormStartPosition.CenterParent, MinimizeBox = false, MaximizeBox = false, ShowInTaskbar = false, ClientSize = new Size(620, 400), Font = new Font("Segoe UI", 10.5f) };
        dlg.Controls.Add(new Label { Text = (firstRun ? "Welcome to OpenXR Companion!\n\n" : "") + "Which controllers do you use? This sets the buttons that open the right and left hand wheels in '" + Path.GetFileName(currentFile) + "'. Nothing else changes, and you can change it later with the 'Controllers' button at the top.", Location = new Point(18, 14), Size = new Size(584, firstRun ? 86 : 60) });
        var how = new Label { Location = new Point(18, 300), Size = new Size(584, 44), ForeColor = Color.FromArgb(60, 70, 90) };
        var radios = new List<RadioButton>();
        int y = firstRun ? 108 : 82;
        foreach (var k in ControllerKinds)
        {
            var r = new RadioButton { Text = k.label, Tag = k, Location = new Point(24, y), Size = new Size(570, 30) };
            r.CheckedChanged += (s, e) => { if (r.Checked) how.Text = k.how; };
            dlg.Controls.Add(r);
            radios.Add(r);
            y += 36;
        }
        // Preselect what this profile uses now, so "Use these buttons" never switches it by surprise.
        string current = CurrentControllerKind(Profile);
        (radios.FirstOrDefault(r => ((ValueTuple<string, string, string>)r.Tag).Item1 == current) ?? radios[0]).Checked = true;
        dlg.Controls.Add(how);
        var bar = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 54, FlowDirection = FlowDirection.RightToLeft, Padding = new Padding(10) };
        Button B(string t, DialogResult r, bool main = false)
        {
            var b = new Button
            {
                Text = t,
                AutoSize = true,
                Height = 34,
                DialogResult = r,
                FlatStyle = FlatStyle.Flat,
                Margin = new Padding(8, 0, 0, 0),
                Font = new Font("Segoe UI", 10f, FontStyle.Bold),
                BackColor = main ? Color.FromArgb(240, 140, 30) : Color.White,
                ForeColor = main ? Color.White : Color.FromArgb(40, 50, 70)
            };
            bar.Controls.Add(b);
            return b;
        }
        dlg.AcceptButton = B("Use these buttons", DialogResult.OK, true);
        B("Keep my current buttons", DialogResult.Cancel);
        dlg.Controls.Add(bar);
        ThemeWalker.Apply(dlg);
        dlg.HandleCreated += (s, e) => ThemeWalker.TitleBar(dlg, Theme.IsDark);
        var res = dlg.ShowDialog(this);
        var kind = ((ValueTuple<string, string, string>)radios.First(r => r.Checked).Tag).Item1;
        try
        { // the flag file says "asked already" and remembers the choice for next time
            Directory.CreateDirectory(DataDir);
            File.WriteAllText(ControllerFlag, res == DialogResult.OK ? kind : current ?? "");
        }
        catch { }
        if (res != DialogResult.OK)
            return;
        int n = ApplyControllerOpeners(Profile, kind);
        if (n == 0)
        { MessageBox.Show(this, "This profile has no bindings that open the 'right' or 'left' wheel, so nothing was changed."); return; }
        MarkDirty();
        BuildTree();
        if (Save())
            MessageBox.Show(this, "Done: " + ControllerKinds.First(k => k.id == kind).how, "Controllers");
    }

    internal static string DataFolder => DataDir;
    internal static string SetupKindOf(ProfileModel p) => CurrentControllerKind(p);

    // The controller kind to preselect: the one chosen last time, else what the profile's wheel openers look like.
    static string CurrentControllerKind(ProfileModel p)
    {
        try
        {
            string saved = File.Exists(ControllerFlag) ? File.ReadAllText(ControllerFlag).Trim() : "";
            if (ControllerKinds.Any(k => k.id == saved))
                return saved;
        }
        catch { }
        foreach (var b in p.Bindings)
        {
            if (b.Action?.Type != ActionType.Wheel || (b.Action.Wheel != "right" && b.Action.Wheel != "left") || b.Input == null)
                continue;
            if (b.Input.EndsWith(".thumbrest"))
                return "touch";
            if (b.Input.EndsWith(".thumbstick") && b.Gesture == GestureKind.DoubleTap)
                return "index";
            if (b.Input.EndsWith(".menu"))
                return "wmr";
        }
        return null;
    }

    // Rewrites the bindings that open the main 'right' / 'left' wheels for this controller kind.
    // Returns how many bindings were changed. Also used by the --selftest check.
    public static int ApplyControllerOpeners(ProfileModel p, string kind)
    {
        int n = 0;
        foreach (var b in p.Bindings)
        {
            if (b.Action?.Type != ActionType.Wheel || !string.IsNullOrEmpty(b.Layers))
                continue;
            string hand = b.Action.Wheel == "right" ? "right" : b.Action.Wheel == "left" ? "left" : null;
            if (hand == null)
                continue;
            string side = hand == "right" ? "Right" : "Left";
            switch (kind)
            {
                case "touch":
                    b.Input = hand + ".thumbrest";
                    b.Gesture = GestureKind.DoubleTap;
                    b.DoubleTapMs = 400;
                    b.Block = "none";
                    b.TapPassthrough = false;
                    b.Action.Confirm = "stick_return";
                    b.Name = side + " hand wheel";
                    break;
                case "index":
                case "frame":
                    b.Input = hand + ".thumbstick";
                    b.Gesture = GestureKind.DoubleTap;
                    b.DoubleTapMs = 400;
                    b.Block = "none";
                    b.TapPassthrough = false;
                    b.Action.Confirm = "stick_return";
                    b.Name = side + " hand wheel (double-click stick)";
                    break;
                default: // wmr, vive
                    b.Input = hand + ".menu";
                    b.Gesture = GestureKind.Hold;
                    b.HoldMs = 500;
                    b.Block = "trigger";
                    b.TapPassthrough = true;
                    b.Action.Confirm = "release";
                    b.Name = side + " hand wheel (hold menu)";
                    break;
            }
            n++;
        }
        // WMR and Vive have no B/Y: the grip goes back / closes instead.
        if (n > 0)
            p.Selection.CancelButton = kind == "wmr" || kind == "vive" ? "grip" : "secondary";
        return n;
    }
}

// Which controller the Right / Left controller pages draw (editor only): the one last picked there, else the one
// that fits the choice in "Your controllers".
internal static class ControllerPictureChoice
{
    static string FilePath => Path.Combine(MainForm.DataFolder, "controller_picture.txt");

    public static string Load(ProfileModel profile)
    {
        try
        {
            string saved = File.Exists(FilePath) ? File.ReadAllText(FilePath).Trim() : "";
            if (ControllerLayouts.All.Any(l => l.Id == saved))
                return saved;
        }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
        return ControllerLayouts.ForSetupKind(MainForm.SetupKindOf(profile));
    }

    public static void Save(string layoutId)
    {
        try
        { AtomicFile.WriteAllText(FilePath, layoutId); }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }
}

public partial class MainForm
{
    // --controller-pics <dir>: every controller picture, both hands, as PNG (with the built-in default profile's bindings).
    public static void ControllerPics(string dir)
    {
        Directory.CreateDirectory(dir);
        var profile = ProfileFromText(Native.DefaultProfile() ?? "{}");
        const int width = 860;
        var sheets = new List<Bitmap>();
        foreach (var layout in ControllerLayouts.All)
        {
            using var right = ControllerPicture.Render(layout, 1, null, p => QuickBindings.UseOf(profile.Bindings, 1, p), width, p => ControllerButtonsPage.TagLinesOf(profile.Bindings, 1, p));
            using var left = ControllerPicture.Render(layout, 0, null, p => QuickBindings.UseOf(profile.Bindings, 0, p), width, p => ControllerButtonsPage.TagLinesOf(profile.Bindings, 0, p));
            var sheet = new Bitmap(width, right.Height + left.Height + 64);
            using (var g = Graphics.FromImage(sheet))
            {
                g.Clear(Theme.Card);
                g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAliasGridFit;
                using var title = new Font("Segoe UI", 13f, FontStyle.Bold);
                using var small = new Font("Segoe UI", 9f);
                using var text = new SolidBrush(Theme.Text);
                using var sub = new SolidBrush(Theme.SubText);
                g.DrawString(layout.Name, title, text, 12, 8);
                g.DrawString("Right hand (top) and left hand (bottom). For: " + layout.Models, small, sub, 14, 36);
                g.DrawImage(right, 0, 60);
                g.DrawImage(left, 0, 60 + right.Height);
            }
            sheet.Save(Path.Combine(dir, "controller_" + layout.Id + ".png"));
            sheets.Add(sheet);
        }
        // All of them on one page, two across, at half size.
        int cols = 2, cellW = width / 2, cellH = sheets.Max(s => s.Height) / 2; // tags make some taller
        using var all = new Bitmap(cols * cellW, (sheets.Count + cols - 1) / cols * cellH);
        using (var g = Graphics.FromImage(all))
        {
            g.Clear(Theme.Page);
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            for (int i = 0; i < sheets.Count; i++)
                g.DrawImage(sheets[i], i % cols * cellW, i / cols * cellH, cellW, sheets[i].Height / 2);
        }
        all.Save(Path.Combine(dir, "controllers_all.png"));
        foreach (var s in sheets)
            s.Dispose();
    }
}

// The Steam Frame setting (all PCs' games, not one profile): the layer then asks SteamVR for the Frame controller
// itself, so all its buttons work, and gives SteamVR a Frame version of each game's bindings. Read when a game starts.
internal static class SteamFrameSetting
{
    static string FilePath => Path.Combine(MainForm.DataFolder, "steam_frame.json");

    public static bool Load()
    {
        try
        {
            if (!File.Exists(FilePath))
                return false;
            using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(FilePath));
            return doc.RootElement.TryGetProperty("native", out var v) && v.ValueKind == System.Text.Json.JsonValueKind.True;
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or System.Text.Json.JsonException)
        { return false; }
    }

    public static void Save(bool native)
    {
        try
        { AtomicFile.WriteAllText(FilePath, native ? "{ \"native\": true }" : "{ \"native\": false }"); }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }

    public static CheckBox NewCheckBox()
    {
        var box = new CheckBox
        {
            Text = "Use all the Steam Frame's buttons (restart the game after changing)",
            AutoSize = true,
            Checked = Load(),
            Margin = new Padding(16, 6, 0, 0),
        };
        new ToolTip { AutoPopDelay = 30000 }.SetToolTip(box,
            "Off: SteamVR shows the Frame to games as Index controllers, so its right X / Y, left D-pad, View and bumpers can't be used here.\n" +
            "On: OpenXR Companion asks SteamVR for the Frame controller itself and gives it the game's own bindings moved onto the Frame\n" +
            "(left X / Y or A / B on the D-pad down / up, the menu on View / Menu). Games that support the Frame keep their own bindings.\n" +
            "Your SteamVR binding changes for the game are not used while this is on.");
        box.CheckedChanged += (s, e) => Save(box.Checked);
        return box;
    }
}
