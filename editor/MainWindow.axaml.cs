using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.Media;
using Avalonia.Media.Imaging;

namespace OverviewPrototype;

public class RailItem : Notify
{
    public string Key { get; init; }
    public string Glyph { get; init; }
    public string Name { get; init; }
    public string Badge { get; init; }
    bool current;
    public bool IsCurrent { get => current; set { current = value; Changed(); } }
}

public partial class MainWindow : Window
{
    readonly List<RailItem> rail;
    bool fillingProfiles;

    public MainWindow() : this(ProfileStore.RealDataDir, false) { }

    public MainWindow(string dataDir, bool canSave)
    {
        AvaloniaXamlLoader.Load(this);
        SetupZoom();
        this.FindControl<TextBlock>("VersionText")!.Text = "OpenXR Companion v" + Updater.AppVersion.ToString(3);
        SetupMirrorButton();
        if (!Program.TestRun) RestoreWindow(); // tests keep the fixed size they ask for
        if (!Program.TestRun && canSave && dataDir == ProfileStore.RealDataDir)
            Opened += async (s, e) => { await Task.Delay(300); await FirstRunAsync(); };

        var store = Shell.Store = new ProfileStore(dataDir, canSave);
        // like the WinForms editor: the layer refreshes default.json (the standard profile) from its built-in one first.
        // Only on the real folder (the layer always works on the real one).
        if (canSave && dataDir == ProfileStore.RealDataDir)
            OpenXRCompanionEditor.Native.PrepareDefaults();
        store.Load(store.ActiveName);

        rail = Shell.RailItems.Select(r => new RailItem
        {
            Key = r.key, Glyph = r.glyph, Name = r.name,
            Badge = r.key == "wrist0" ? "L" : r.key == "wrist1" ? "R" : null,
        }).ToList();
        this.FindControl<ItemsControl>("Rail")!.ItemsSource = rail;

        var box = this.FindControl<ComboBox>("ProfileBox")!;
        FillProfiles();
        box.SelectionChanged += async (s, e) =>
        {
            if (fillingProfiles || box.SelectedItem is not string name || name == store.CurrentName)
                return;
            if (!await ConfirmDiscard())
            { FillProfiles(); return; }
            store.Load(name);
            store.SetActive(name); // like WinForms: the profile picked here is the one games use (a copy only, in the port)
            Shell.Refresh();
        };
        this.FindControl<Button>("NewProfileButton")!.Click += async (s, e) =>
        {
            if (!await ConfirmDiscard()) return;
            var exe = new AutoCompleteBox { Width = 300, Watermark = "the game's program, e.g. bannerlord",
                ItemsSource = Directory.Exists(Path.Combine(store.DataDir, "logs"))
                    ? Directory.GetFiles(Path.Combine(store.DataDir, "logs"), "*.log").Select(Path.GetFileNameWithoutExtension).ToList() : new List<string>() };
            var from = new ComboBox { Width = 300, ItemsSource = store.ProfileNames().ToList(), SelectedItem = store.ActiveName };
            var body = new StackPanel { Spacing = 4, Children = { new TextBlock { Text = "Game exe name" }, exe,
                new TextBlock { Text = "Start from", Margin = new Thickness(0, 8, 0, 0) }, from,
                new TextBlock { Text = "Games the layer has already seen are in the list (from its logs).", Classes = { "desc" } } } };
            if (!await Dialogs.Ask(this, "New game profile", "A profile used only by one game, named after its program.", body))
                return;
            string why = store.NewProfile(exe.Text, from.SelectedItem as string);
            Shell.SetStatus(why ?? "Made " + store.CurrentName + "; your games now use it.", why != null);
            FillProfiles();
            Shell.Refresh();
        };
        this.FindControl<Button>("RenameProfileButton")!.Click += async (s, e) =>
        {
            if (!await ConfirmDiscard()) return;
            var name = new TextBox { Width = 320, Text = Path.GetFileNameWithoutExtension(store.CurrentName) };
            if (!await Dialogs.Ask(this, "Rename profile", "New name for " + store.CurrentName + ".\n\nA game profile is named after the game's program (bannerlord.json is used by Bannerlord.exe): after renaming, it applies to the program with the new name.", name))
                return;
            string why = store.Rename(name.Text);
            Shell.SetStatus(why ?? "Renamed to " + store.CurrentName, why != null);
            FillProfiles();
            Shell.Refresh();
        };
        this.FindControl<Button>("DeleteProfileButton")!.Click += async (s, e) =>
        {
            if (!await Dialogs.Confirm(this, "Delete", store.DeleteQuestion, "Delete")) return;
            string old = store.CurrentName, why = store.Delete();
            Shell.SetStatus(why ?? "Deleted " + old, why != null);
            FillProfiles();
            Shell.Refresh();
        };
        this.FindControl<Button>("SaveButton")!.Click += async (s, e) =>
        {
            var plan = store.PlanSave();
            if (plan.Error != null)
            { Shell.SetStatus(plan.Error, true); return; }
            if (plan.LayerSays != null && !await Dialogs.Confirm(this, "Problem",
                    "The layer would reject this profile:\n\n" + plan.LayerSays + "\n\nSave anyway? (the game keeps the last good version until fixed)", "Save anyway"))
                return;
            if (plan.AskFork && !await Dialogs.Confirm(this, "Saving changes to the standard profile",
                    "You changed default.json, the standard profile. It stays as it is, and you already have your own user_defaults.json, which won't be replaced.\n\n" +
                    "These changes will be saved as a new profile: " + plan.ForkTo + "\n\n" +
                    "The new profile becomes the selected one, so your games use it. Pick user_defaults.json in the profile list to go back to it.", "Save"))
                return;
            string said = store.Save(plan);
            Shell.SetStatus(said, plan.LayerSays != null);
            FillProfiles();
        };
        this.FindControl<Button>("RevertButton")!.Click += (s, e) => { store.Revert(); Shell.Refresh(); Shell.SetStatus("Reverted to the saved " + store.CurrentName); };
        this.FindControl<Button>("BackButton")!.Click += (s, e) => Shell.GoBack();
        this.FindControl<Button>("TutorialButton")!.Click += (s, e) => Shell.Go("tutorials");
        void Explore(string dir)
        {
            try { Directory.CreateDirectory(dir); System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo("explorer.exe", "\"" + dir + "\"") { UseShellExecute = true }); }
            catch (Exception ex) { Shell.SetStatus("Couldn't open " + dir + ": " + ex.Message, true); }
        }
        this.FindControl<Button>("FolderButton")!.Click += (s, e) => Explore(store.ProfilesDir);
        this.FindControl<Button>("LogsButton")!.Click += (s, e) => Explore(Path.Combine(store.DataDir, "logs"));
        this.FindControl<Button>("ShowMeButton")!.Click += (s, e) => { if (Tours.SectionTour(Shell.Current) is string t) StartTutorial(t); };
        this.FindControl<Button>("HomeButton")!.Click += (s, e) => Shell.Go("overview");

        store.Changed += () =>
        {
            this.FindControl<TextBlock>("DirtyText")!.IsVisible = store.Dirty;
            Title = "OpenXR Companion - Profile Editor (Avalonia port) - " + store.CurrentName + (store.Dirty ? " *" : "");
        };
        Shell.Navigated += Show;
        Shell.Status += (text, error) =>
        {
            var st = this.FindControl<TextBlock>("StatusText")!;
            st.Text = text;
            st.Foreground = error ? Brush.Parse("#FF7D73") : Brush.Parse("#C8CDD6");
        };

        Shell.SetStatus(Program.DocsShot ? "" : canSave
            ? (dataDir == ProfileStore.RealDataDir ? "Your profiles (" + dataDir + ")." : "Data folder: " + dataDir + " (a copy).")
            : "Preview: your profiles are shown but nothing is saved.");
        SetupLayerAndUpdates();
        Shell.Go("overview");
    }

    // Unsaved changes before switching or renaming profiles: true = go on (they are dropped).
    async Task<bool> ConfirmDiscard() =>
        !Shell.Store.Dirty || await Dialogs.Confirm(this, "Unsaved changes", "You have unsaved changes in " + Shell.Store.CurrentName + ". Drop them?", "Drop them");

    // Off for test runs (like WinForms, which never goes online when started with arguments).
    public static bool CheckUpdates = true;

    // The layer status, Install / Uninstall layer, and "Update available" when GitHub has a newer release.
    void SetupLayerAndUpdates()
    {
        var text = this.FindControl<TextBlock>("LayerText")!;
        void ShowLayer()
        {
            var (t, ok) = LayerTools.Status();
            text.Text = t;
            text.Foreground = Avalonia.Media.Brush.Parse(ok ? "#6ECD8C" : "#FF7D73");
        }
        ShowLayer();
        ToolTip.SetTip(text, "64-bit games use the main layer. 32-bit games (Fallout: New Vegas / Fallout 3 with TTWVR) use the 32-bit one in the x86 folder. Install layer sets up both.");
        void Run(string script)
        {
            string why = LayerTools.RunScript(script);
            if (why != null)
                Shell.SetStatus(why, true);
            ShowLayer();
        }
        this.FindControl<Button>("InstallButton")!.Click += (s, e) => Run("install.ps1");
        this.FindControl<Button>("UninstallButton")!.Click += (s, e) => Run("uninstall.ps1");
        var args = Environment.GetCommandLineArgs();
        if (args.Length == 3 && args[1] == "--updated") // started by the updater
        {
            Opened += (s, e) => Shell.SetStatus("Updated from v" + args[2] + " to v" + Updater.AppVersion.ToString(3) +
                ". The previous files are in the _previous_version folder. A running VR game keeps the old layer until you restart it.");
            return;
        }
        if (!CheckUpdates || !SettingsPageView.UpdateCheckOn)
            return;
        Opened += async (s, e) =>
        {
            string tag = await LayerTools.NewerReleaseAsync();
            if (tag == null)
                return;
            var b = this.FindControl<Button>("UpdateButton")!;
            b.Content = "Update available: " + tag;
            ToolTip.SetTip(b, "You have v" + Updater.AppVersion.ToString(3) + ". Click to update.");
            b.Click += async (s2, e2) => await UpdateClicked(b, tag);
            b.IsVisible = true;
            Shell.SetStatus("A new version (" + tag + ") is available: click the blue Update button at the top to update.");
        };
    }

    // One click: download, check, keep the current files in _previous_version, swap, restart (Updater.cs).
    bool updating;
    async Task UpdateClicked(Button b, string tag)
    {
        if (updating) return;
        string why = Updater.CantUpdateReason();
        if (why != null)
        {
            if (await Dialogs.Confirm(this, "Update", "The editor can't update itself here: " + why + ".\n\nOpen the download page instead?", "Open the download page"))
                LayerTools.OpenReleasesPage(tag);
            return;
        }
        if (Shell.Store.Dirty)
        {
            await Dialogs.Choose(this, "Update", "You have unsaved changes in " + Shell.Store.CurrentName + ". Save them first (Ctrl+S), then click Update again.", "OK");
            return;
        }
        int pick = await Dialogs.Choose(this, "Update",
            "Update to " + tag + " now?\n\nThe editor downloads it from GitHub, checks it, keeps your current files in the _previous_version folder, and restarts. " +
            "Your profiles and settings are not touched.\n\nBest with your VR games closed: a running game keeps the old layer until you restart it.",
            "Update now", "Open the download page", "Cancel");
        if (pick == 1) { LayerTools.OpenReleasesPage(tag); return; }
        if (pick != 0) return;
        updating = true;
        b.IsEnabled = false;
        string was = Updater.AppVersion.ToString(3);
        try
        {
            var r = await Updater.LatestAsync();
            void Progress(int pct, string text) => Avalonia.Threading.Dispatcher.UIThread.Post(() =>
            { b.Content = pct >= 0 ? "Downloading " + pct + "%" : "Updating..."; Shell.SetStatus(text); });
            string zip = await Updater.DownloadAsync(r, Progress);
            string exe = await Task.Run(() => Updater.Install(zip, Progress));
            try { Updater.Restart(exe, was); }
            catch { await Dialogs.Choose(this, "Update", "Updated to " + r.Tag + ". Please start the editor again.", "OK"); }
            Close();
        }
        catch (Exception ex)
        {
            updating = false;
            b.IsEnabled = true;
            b.Content = "Update available: " + tag;
            Shell.SetStatus("Update failed: " + ex.Message, true);
            if (await Dialogs.Confirm(this, "Update", "The update didn't finish: " + ex.Message + "\n\nYour current version is unchanged. Open the download page instead?", "Open the download page"))
                LayerTools.OpenReleasesPage(tag);
        }
    }

    void FillProfiles()
    {
        var box = this.FindControl<ComboBox>("ProfileBox")!;
        fillingProfiles = true;
        var names = Shell.Store.ProfileNames().ToList();
        box.ItemsSource = names;
        box.SelectedItem = Shell.Store.CurrentName;
        fillingProfiles = false;
    }

    void Show(string key)
    {
        this.FindControl<ContentControl>("PageHost")!.Content = Shell.Make(key);
        string tip = Shell.TipOf(key);
        this.FindControl<Border>("TipBar")!.IsVisible = tip != null;
        this.FindControl<TextBlock>("TipText")!.Text = tip;
        this.FindControl<Button>("ShowMeButton")!.IsVisible = Tours.SectionTour(key) != null;
        this.FindControl<Button>("BackButton")!.IsEnabled = Shell.CanGoBack;
        string section = key.Split(':')[0] switch { "wheel" => "wheels", "binding" => "bindings", "wrist0" or "wrist1" => "wrists", var k => k };
        foreach (var r in rail)
            r.IsCurrent = r.Key == section;
    }

    void Rail_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button { Tag: string key })
        {
            Shell.Go(key);
        }
    }

    // Test hook: save the window as a PNG.
    public static double ShotScale = 1; // --shot scale=1.5: render as Windows display scaling 150% would
    public async Task ShotAsync(string file)
    {
        await Task.Delay(800);
        var size = new PixelSize((int)(Bounds.Width * ShotScale), (int)(Bounds.Height * ShotScale));
        using var bmp = new RenderTargetBitmap(size, new Vector(96 * ShotScale, 96 * ShotScale));
        bmp.Render(this);
        bmp.Save(file);
    }
}
