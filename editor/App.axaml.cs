using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;

namespace OverviewPrototype;

public partial class App : Application
{
    public override void Initialize() => AvaloniaXamlLoader.Load(this);

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            // --data <folder>: work on that folder (a copy) instead of your real one.
            // --shot <file.png> <width> <height> [page]: open, save a picture of the window, close (layout checks).
            var a = (desktop.Args ?? System.Array.Empty<string>()).ToList();
            string data = OverviewPrototype.ProfileStore.RealDataDir;
            // A normal start saves to your real profiles, like the WinForms editor. --preview: look only. Test modes
            // (any other argument) never save to the real folder; with --data they work on that copy.
            bool canSave = a.Count == 0;
            int d = a.IndexOf("--data");
            if (d >= 0 && d + 1 < a.Count)
            { data = a[d + 1]; canSave = true; a.RemoveRange(d, 2); }
            a.Remove("--preview");
            MainWindow.CheckUpdates = a.Count == 0; // test runs never go online
            var w = new MainWindow(data, canSave);
            desktop.MainWindow = w;
            // --data <copy> --clicktest <result.txt>: clicks real buttons on several pages and lists what each click changed.
            if (a.Count == 2 && a[0] == "--clicktest" && canSave
                && !string.Equals(System.IO.Path.GetFullPath(data).TrimEnd('\\', '/'), System.IO.Path.GetFullPath(OverviewPrototype.ProfileStore.RealDataDir).TrimEnd('\\', '/'), System.StringComparison.OrdinalIgnoreCase))
                w.Opened += async (s, e) => { await ClickTest.Run(w, a[1]); desktop.Shutdown(); };
            // --data <copy> --savetest <result.txt>: flip binding 0's Enabled box through the real control, Save, and list every
            // JSON path that differs between the profile before and the saved file (expected: only that one). Copies only.
            if (a.Count == 2 && a[0] == "--savetest" && canSave
                && !string.Equals(System.IO.Path.GetFullPath(data).TrimEnd('\\', '/'), System.IO.Path.GetFullPath(OverviewPrototype.ProfileStore.RealDataDir).TrimEnd('\\', '/'), System.StringComparison.OrdinalIgnoreCase))
            {
                w.Opened += async (s, e) =>
                {
                    var lines = new List<string>();
                    var store = Shell.Store;
                    string before = store.Serialize();
                    string from = store.CurrentName;
                    Shell.Go("binding:0");
                    await System.Threading.Tasks.Task.Delay(500);
                    var page = Avalonia.Controls.ControlExtensions.FindControl<Avalonia.Controls.ContentControl>(w, "PageHost")!.Content as Avalonia.Controls.Control;
                    var box = Avalonia.VisualTree.VisualExtensions.GetVisualDescendants(page!).OfType<Avalonia.Controls.CheckBox>()
                        .First(c => (c.Content as string) == "Enabled");
                    bool was = box.IsChecked == true;
                    box.IsChecked = !was; // what a click does
                    await System.Threading.Tasks.Task.Delay(200);
                    lines.Add($"binding 0 Enabled: {was} -> {box.IsChecked}; marked unsaved: {store.Dirty}");
                    string why = store.Save();
                    lines.Add("save: " + (why ?? "ok") + "; from " + from + " to " + store.CurrentName);
                    string saved = System.IO.File.ReadAllText(System.IO.Path.Combine(store.ProfilesDir, store.CurrentName));
                    var diffs = new List<string>();
                    void Diff(System.Text.Json.Nodes.JsonNode x, System.Text.Json.Nodes.JsonNode y, string path)
                    {
                        if (x is System.Text.Json.Nodes.JsonObject ox && y is System.Text.Json.Nodes.JsonObject oy)
                        {
                            foreach (var k in ox.Select(kv => kv.Key).Union(oy.Select(kv => kv.Key)))
                                Diff(ox[k], oy[k], path + "." + k);
                        }
                        else if (x is System.Text.Json.Nodes.JsonArray ax && y is System.Text.Json.Nodes.JsonArray ay)
                        {
                            for (int i = 0; i < Math.Max(ax.Count, ay.Count); i++)
                                Diff(i < ax.Count ? ax[i] : null, i < ay.Count ? ay[i] : null, path + "[" + i + "]");
                        }
                        else if ((x?.ToJsonString() ?? "null") != (y?.ToJsonString() ?? "null"))
                            diffs.Add(path + ": " + (x?.ToJsonString() ?? "null") + " -> " + (y?.ToJsonString() ?? "null"));
                    }
                    Diff(System.Text.Json.Nodes.JsonNode.Parse(before), System.Text.Json.Nodes.JsonNode.Parse(saved), "$");
                    lines.Add("differences: " + diffs.Count);
                    lines.AddRange(diffs);
                    System.IO.File.WriteAllLines(a[1], lines);
                    desktop.Shutdown();
                };
            }
            // --tourtest <result.txt> <shot.png>: every tour, every step: was its target found and framed; then "throw away"
            // must put the profile back exactly. A picture of the 4th step of the main tour.
            // --wristcopytest <result.txt>: every wrist panel copy, both arms, both ways, checked (in memory, nothing written)
            if (a.Count == 2 && a[0] == "--wristcopytest")
                w.Opened += (s, e) => Avalonia.Threading.Dispatcher.UIThread.Post(() => { WristCopyTest.Run(a[1]); desktop.Shutdown(); });
            if (a.Count == 3 && a[0] == "--tourtest")
                w.Opened += async (s, e) => { await w.TourTest(a[1], a[2]); desktop.Shutdown(); };
            // --nochange <result.txt>: open every page that edits something, then check the profile text didn't change
            // (opening a page must never change the profile).
            if (a.Count == 2 && a[0] == "--nochange")
            {
                w.Opened += async (s, e) =>
                {
                    var lines = new List<string>();
                    foreach (var name in Shell.Store.ProfileNames().ToList())
                    {
                        Shell.Store.Load(name);
                        string before = Shell.Store.Serialize();
                        var keys = new List<string> { "overview", "bindings" };
                        keys.AddRange(Enumerable.Range(0, Shell.Store.Profile.Bindings.Count).Select(i => "binding:" + i));
                        keys.Add("wheels");
                        keys.Add("controllers");
                        foreach (var cp in new[] { "0.thumbstick", "1.thumbstick", "1.thumbrest", "0.trigger", "1.grip", "1.primary" })
                            keys.Add("controllers:" + cp);
                        keys.Add("wrists");
                        keys.Add("wrist0");
                        keys.Add("wrist1");
                        keys.Add("wrist0:settings");
                        keys.Add("wrist1:settings");
                        keys.Add("settings");
                        keys.Add("bindings:steady");
                        keys.Add("settings:pointing?steady");
                        keys.AddRange(SettingsPageView.Ported.Select(k => "settings:" + k));
                        keys.AddRange(Shell.Store.Profile.Wheels.Keys.Select(k => "wheel:" + k));
                        foreach (var key in keys)
                        {
                            Shell.Go(key);
                            await System.Threading.Tasks.Task.Delay(300);
                            string now = Shell.Store.Serialize();
                            lines.Add((now == before ? "same     " : "CHANGED  ") + name + "  " + key + (Shell.Store.Dirty ? "  (marked unsaved)" : ""));
                        }
                        Shell.Go("overview");
                    }
                    System.IO.File.WriteAllLines(a[1], lines);
                    desktop.Shutdown();
                };
            }
            // --tipstest <result.txt>: the Overview's Getting started guide, every slide forward then back (by its own Step)
            if (a.Count >= 2 && a[0] == "--tipstest")
            {
                w.Opened += async (s, e) =>
                {
                    var lines = new List<string>();
                    await System.Threading.Tasks.Task.Delay(600);
                    var tp = Avalonia.VisualTree.VisualExtensions.GetVisualDescendants(w).OfType<TipsPanel>().FirstOrDefault();
                    if (tp == null) lines.Add("FAIL no tips panel");
                    else
                    {
                        int n = tp.SlideCount, bad = 0;
                        foreach (int d in new[] { 1, -1 })
                            for (int i = 0; i < n + 1; i++)
                            {
                                try { tp.TestStep(d); await System.Threading.Tasks.Task.Delay(30); }
                                catch (Exception ex) { bad++; lines.Add($"FAIL step {d} #{i}: {ex.GetType().Name}: {ex.Message}"); }
                            }
                        lines.Add($"{2 * (n + 1) - bad}/{2 * (n + 1)} steps ok ({n} slides)");
                    }
                    System.IO.File.WriteAllLines(a[1], lines);
                    desktop.Shutdown();
                };
            }
            if (a.Count >= 4 && a[0] == "--shot")
            {
                // optional anywhere after the size: scale=1.5 (Windows display scaling), zoom=130 (the Size slider)
                foreach (var opt in a.Where(x => x.StartsWith("scale=") || x.StartsWith("zoom=")).ToList())
                {
                    double v = double.Parse(opt.Substring(opt.IndexOf('=') + 1), System.Globalization.CultureInfo.InvariantCulture);
                    if (opt.StartsWith("scale=")) MainWindow.ShotScale = v; else w.SetZoomPercent(v, false);
                    a.Remove(opt);
                }
                w.Width = double.Parse(a[2]);
                w.Height = double.Parse(a[3]);
                // several pages split by commas: each is opened in turn, 1.5 s apart, the last one is pictured
                var pages = a.Count >= 5 ? a[4].Split(',', StringSplitOptions.RemoveEmptyEntries) : Array.Empty<string>();
                if (pages.Length > 0)
                    Shell.Go(pages[0]);
                bool bottom = a.Count >= 6 && a[5] == "bottom"; // scrolled to the end of the page
                w.Opened += async (s, e) =>
                {
                    if (bottom)
                    {
                        await System.Threading.Tasks.Task.Delay(500);
                        foreach (var sv in Avalonia.VisualTree.VisualExtensions.GetVisualDescendants(w).OfType<Avalonia.Controls.ScrollViewer>())
                            sv.ScrollToEnd();
                    }
                    foreach (var pg in pages.Skip(1))
                    {
                        await System.Threading.Tasks.Task.Delay(1500);
                        Shell.Go(pg);
                    }
                    await System.Threading.Tasks.Task.Delay(1500);
                    await w.ShotAsync(a[1]);
                    desktop.Shutdown();
                };
            }
        }
        base.OnFrameworkInitializationCompleted();
    }
}
