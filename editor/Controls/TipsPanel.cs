using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform;
using Avalonia.Threading;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// "Getting started" on the right of the Overview: a guide that plays by itself (pauses while the mouse is over it;
// ◀ ▶ step by hand). Built from the BUILT-IN default profile (Native.DefaultProfile, the layer's own copy), so it always
// matches what ships:
//   1. Start from the Default: both wrist panels drawn, every button visible.
//   2. How to show a panel (GIF).
//   3. Per panel page: every button and what it does by default.
//   4. Per button (once each, left panel first): that button lit on the drawn page, what it does, what you can do with it,
//      and a GIF where one fits (Calibrate -> calibrate + fingertip, a wheel button -> wheels).
//   5. General tips: wheels from a controller, how you press, save.
// GIFs: tips/<id>.gif next to the exe, else Assets/tips/<id>.gif inside the editor (drawn by tools/make_tip_gifs.py).
public class TipsPanel : Border
{
    protected override Type StyleKeyOverride => typeof(Border); // styled as a card (styles match the exact type)

    sealed record Slide(string Section, string Title, string Text, Func<Control> Picture = null, string Gif = null,
                        string Go = null, string GoText = null, double Seconds = 9);

    static List<Slide> slides;
    static int shown = int.TryParse(Environment.GetEnvironmentVariable("VRO_SHOT_TIP"), out int t0) ? t0 : 0; // env var: screenshot checks
    const double Width_ = 540;

    readonly ContentControl host = new();
    readonly Border progress = new() { Height = 3, Background = Brush.Parse("#F08C1E"), HorizontalAlignment = HorizontalAlignment.Left, CornerRadius = new CornerRadius(2) };
    readonly DispatcherTimer timer = new() { Interval = TimeSpan.FromMilliseconds(50) };
    double elapsed;
    bool paused;

    public TipsPanel()
    {
        Classes.Add("card");
        Width = Width_;
        MinHeight = 520; // the page below doesn't jump as slides change length
        VerticalAlignment = VerticalAlignment.Top;
        BorderBrush = Brush.Parse("#F08C1E");
        BorderThickness = new Thickness(2);
        slides ??= Build();
        Child = host;
        timer.Tick += (s, e) =>
        {
            if (paused) return;
            elapsed += 0.05;
            double len = slides[shown].Seconds;
            progress.Width = Math.Max(0, (Width_ - 34) * Math.Min(1, elapsed / len));
            if (elapsed >= len) Step(1);
        };
        PointerEntered += (s, e) => { paused = true; ShowPaused(); };
        PointerExited += (s, e) => { paused = false; ShowPaused(); };
        AttachedToVisualTree += (s, e) => timer.Start();
        DetachedFromVisualTree += (s, e) => timer.Stop();
        Show();
    }

    TextBlock pausedText;
    void ShowPaused() { if (pausedText != null) pausedText.Text = paused ? "paused while you read" : ""; }

    public int SlideCount => slides.Count;
    public void TestStep(int d) => Step(d); // --tipstest

    void Step(int d) { shown = (shown + slides.Count + d) % slides.Count; Show(); }

    void Show()
    {
        elapsed = 0;
        progress.Width = 0;
        var t = slides[shown];
        var back = new Button { Classes = { "tool" }, Content = "◀", MinWidth = 36 };
        var next = new Button { Classes = { "tool" }, Content = "Next ▶" };
        back.Click += (s, e) => Step(-1);
        next.Click += (s, e) => Step(1);
        pausedText = new TextBlock { Foreground = Brush.Parse("#8A93A0"), FontStyle = FontStyle.Italic, VerticalAlignment = VerticalAlignment.Center, FontSize = 12 };
        ShowPaused();

        var body = new StackPanel { Spacing = 10 };
        body.Children.Add(new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
            Children =
            {
                new StackPanel { Spacing = 2, Children =
                {
                    new TextBlock { Text = "💡  Getting started", Classes = { "title" } },
                    new TextBlock { Text = t.Section.ToUpperInvariant(), FontSize = 11, FontWeight = FontWeight.Bold, Foreground = Brush.Parse("#7FA8F0") },
                } },
                // ◀ 3 / 21 Next ▶ in the header: the same spot on every slide, so the mouse can stay on Next
                new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6, [Grid.ColumnProperty] = 1, VerticalAlignment = VerticalAlignment.Top, Children =
                {
                    back,
                    new TextBlock { Text = $"{shown + 1} / {slides.Count}", Foreground = Brush.Parse("#8A93A0"), VerticalAlignment = VerticalAlignment.Center, MinWidth = 44, TextAlignment = TextAlignment.Center },
                    next,
                } },
            },
        });
        // the progress bar is one control kept across slides: take it out of the previous slide before adding it again
        (progress.Parent as Border)?.SetValue(Border.ChildProperty, null);
        body.Children.Add(new Border { Background = Brush.Parse("#2A303A"), CornerRadius = new CornerRadius(2), Height = 3, Child = progress });
        body.Children.Add(new TextBlock { Text = t.Title, FontSize = 18, FontWeight = FontWeight.SemiBold, TextWrapping = TextWrapping.Wrap });

        var text = new TextBlock { Text = t.Text, TextWrapping = TextWrapping.Wrap, Foreground = Brush.Parse("#DCE4F0"), LineHeight = 21 };
        if (t.Picture?.Invoke() is Control pic)
        {
            // picture on the left, words beside it
            var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*") };
            g.Children.Add(pic);
            text.Margin = new Thickness(14, 0, 0, 0);
            Grid.SetColumn(text, 1);
            g.Children.Add(text);
            body.Children.Add(g);
        }
        else
            body.Children.Add(text);
        if (t.Gif != null && Gif(t.Gif) is Control gif)
            body.Children.Add(new Border { CornerRadius = new CornerRadius(6), ClipToBounds = true, Background = Brush.Parse("#15181C"), Child = gif, HorizontalAlignment = HorizontalAlignment.Left });

        var bottom = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto"), Margin = new Thickness(0, 4, 0, 0) };
        pausedText.HorizontalAlignment = HorizontalAlignment.Center;
        Grid.SetColumn(pausedText, 1);
        bottom.Children.Add(pausedText);
        if (t.Go != null)
        {
            var go = new Button { Classes = { "go" }, Content = t.GoText + "  ▶" };
            go.Click += (s, e) => Shell.Go(t.Go);
            Grid.SetColumn(go, 2);
            bottom.Children.Add(go);
        }
        body.Children.Add(bottom);
        host.Content = body;
    }

    // ---- building the guide from the built-in default ---------------------------------------------------------------

    static ProfileModel DefaultProfile()
    {
        try { var t = Native.DefaultProfile(); if (!string.IsNullOrEmpty(t)) return ProfileIO.FromText(t); } catch { }
        try { return ProfileIO.FromText(File.ReadAllText(Path.Combine(Shell.Store.ProfilesDir, "default.json"))); } catch { }
        return null;
    }

    static Func<Control> PagePicture(WheelModel page, int cols, WheelItemModel lit = null, double size = 210) => () =>
        new WheelView { Wheel = page, IsWristPage = true, GridColumns = cols, Highlight = lit, Width = size, Height = size, IsHitTestVisible = false, VerticalAlignment = VerticalAlignment.Top };

    // the panel's pages: its first page, then the others in the order their buttons lead to them
    static List<WheelModel> PagesOf(WristModel w)
    {
        var list = new List<WheelModel>();
        void Add(string name)
        {
            if (name == null || !w.Pages.TryGetValue(name, out var pg) || list.Contains(pg) || pg.Items.Count(i => i.Enabled) == 0) return;
            list.Add(pg);
            foreach (var it in pg.Items.Where(i => i.Enabled && i.Action?.Type == "page"))
                Add(it.Action.Page);
        }
        Add(w.Start);
        foreach (var n in w.Pages.Keys) Add(n);
        return list;
    }

    // What you can do with each kind of button (beyond what it does by default), and the GIF that shows it.
    static (string can, string gif) More(ActionModel a) => a?.Type switch
    {
        "adjust" => ("Press it, then point at any panel, wheel, keyboard or screen, hold the grip and move your hand: it follows. " +
                     "Thumbstick up / down resizes what you hold. A fingertip shows on each controller too: grab it to set where your touches land. Press again to save.", "calibrate"),
        "wheel" => ("Opens a radial wheel right from the panel. Wheels open from controller buttons too (double-tap the thumb rest); point at an item and pull the trigger.", "wheels"),
        "key" => ("Presses a keyboard key or combo in the game, like the real key. On its page here you can change the key (or record one), or make it a toggle: press once to keep it down, again to let go. On a controller button the key stays down while you hold it.", "key"),
        "sequence" => ("A macro: several keys and waits in a row.", null),
        "page" => ("Goes to another page of this panel. Add pages of your own with '+ Add page' and link them with buttons like this.", "page"),
        "screen" => ("Shows a desktop monitor or a single window (Discord, a guide, chat) on your wrist or in front of you. Point at it to use the mouse; its bar picks the monitor or window.", "screen"),
        "keyboard" => ("A keyboard in VR for chat, names and search boxes. Grab it with the grip to move it; while calibrating, the stick resizes it.", "keyboard"),
        "controller_offsets" => ("Opens a panel in the headset that turns and moves the game's hands and guns until they match your real ones. Saved for that game only.", "offsets"),
        "headset_mirror" => ("A window on your desktop showing what you see in the headset, panels and wheels included: for recording, OBS or people watching. Settings > Headset mirror sets eye and zoom.", "headset_mirror"),
        "settings_menu" => ("The overlay's settings inside the headset: the same things as this editor, without taking the headset off.", "settings_menu"),
        "reload" => ("Reads the profile from disk again. Saving here does this by itself within a second, so you rarely need it.", "reload"),
        "close" => ("Hides the panel until you look away and back. Handy when it is in the way.", "close"),
        "xr_input" => ("Presses a controller button the game sees, so a panel button can stand in for a hard-to-reach one.", "key"),
        "layer" => ("Switches a layer: while it is on, buttons do their second job.", "key"),
        _ => ("Change what it does on the Wrist panels page: click the button in the picture.", null),
    };

    static List<Slide> Build()
    {
        var s = new List<Slide>();
        var p = DefaultProfile();
        const string start = "Start here";
        if (p != null)
        {
            var panels = new[] { (h: 0, w: p.WristLeft, name: "Left"), (h: 1, w: p.WristRight, name: "Right") }
                .Select(x => (x.h, w: p.WristPages(x.h), x.name, cols: x.w.Columns)).ToList();
            var first = panels.Select(x => PagesOf(x.w).FirstOrDefault()).ToList();
            s.Add(new Slide(start, "Start from the Default profile",
                "The Default profile comes with a wrist panel on each arm, holding every configuration menu and extra feature of the Companion layer. " +
                "Make it the basis of your setup: keep what you like, change the rest. The next slides walk through every button.",
                () => new StackPanel { Orientation = Orientation.Vertical, Spacing = 6, Children =
                {
                    PagePicture(first[0], panels[0].cols, null, 150)(),
                    PagePicture(first[1], panels[1].cols, null, 150)(),
                } }, null, "wrists", "Wrist panels", 11));
            s.Add(new Slide(start, "Show a wrist panel",
                "Turn your forearm toward your face, like checking a watch, and look at it: the panel appears. Press its buttons with your other hand: touch them with your fingertip, or point at them and pull the trigger.",
                null, "show_panel", Seconds: 10));

            var seen = new HashSet<string>();
            foreach (var (h, w, name, cols) in panels)
            {
                var pages = PagesOf(w);
                for (int k = 0; k < pages.Count; k++)
                {
                    var pg = pages[k];
                    string where = $"{name} panel, {(k == 0 ? "first page" : $"page {k + 1}")}" + (string.IsNullOrWhiteSpace(pg.Title) ? "" : $" '{pg.Title}'");
                    var items = pg.Items.Where(i => i.Enabled).ToList();
                    var fresh = items.Where(i => !seen.Contains(Key(i))).ToList();
                    if (fresh.Count == 0) continue;
                    s.Add(new Slide(where, $"Every button on this page",
                        string.Join("\n", items.Select((i, n) => $"{n + 1}. {Clean(i.Label)}: {ActionText.Plain(i.Action)}")),
                        PagePicture(pg, cols), null, "wrist" + h, $"Edit the {name.ToLowerInvariant()} panel", 6 + items.Count * 0.9));
                    foreach (var it in fresh)
                    {
                        seen.Add(Key(it));
                        var (can, gif) = More(it.Action);
                        s.Add(new Slide(where, Clean(it.Label),
                            "By default: " + ActionText.Plain(it.Action) + ".\n\n" + can,
                            PagePicture(pg, cols, it, 170), gif, Seconds: gif != null ? 13 : 9));
                        if (it.Action?.Type == "adjust") // the fingertip is set in the same mode
                            s.Add(new Slide(where, "While calibrating: your fingertip",
                                "A small fingertip shows in front of each controller. Grab it with the other hand's grip and move it to where your real finger is: touching panel buttons then lands exactly where you expect.",
                                PagePicture(pg, cols, it, 170), "fingertip", "settings:pointing", "Laser and fingertip", 12));
                    }
                }
            }
        }
        s.Add(new Slide("More", "Open a radial wheel",
            "Double-tap the thumb rest (the flat spot next to the face buttons) to open that hand's wheel. Point at an item and pull the trigger, or push the thumbstick toward it.",
            null, "wheels", "wheels", "Wheels", 11));
        s.Add(new Slide("More", "Laser, touch, or both",
            "If buttons trigger by accident, choose one way to press: 'How you press' at the top of the Wheels and Wrist panels pages sets it for everything at once.",
            null, "how_press", "wrists", "Wrist panels"));
        s.Add(new Slide("More", "Change things here, see them in VR",
            "Edit anything in this window and press Save (Ctrl+S): the game picks it up within a second, no restart. The tutorials walk you through making your own wheels and buttons.",
            null, "save", "tutorials", "Tutorials"));
        return s;
    }

    // one slide per kind of button, even when both panels have it
    static string Key(WheelItemModel i) => $"{i.Action?.Type}|{i.Action?.Mode}|{i.Action?.Keys}|{i.Action?.Wheel}|{i.Action?.Page}";
    static string Clean(string label) => string.Join(" ", (label ?? "").Split(new[] { ' ', '\n', '\r' }, StringSplitOptions.RemoveEmptyEntries));

    static Control Gif(string id)
    {
        foreach (var ext in new[] { ".gif", ".png", ".jpg" })
        {
            try
            {
                string file = Path.Combine(AppContext.BaseDirectory, "tips", id + ext);
                var uri = new Uri($"avares://OpenXRCompanionEditor/Assets/tips/{id}{ext}");
                Stream st = File.Exists(file) ? File.OpenRead(file) : AssetLoader.Exists(uri) ? AssetLoader.Open(uri) : null;
                if (st == null) continue;
                using (st)
                {
                    var g = new GifImage(st) { MaxWidth = Width_ - 34, MaxHeight = 280 };
                    if (g.HasPicture) return g;
                }
            }
            catch { }
        }
        return null;
    }
}
