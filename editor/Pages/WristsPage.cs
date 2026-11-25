using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Wrist panels, like the Wheels page: your two panels as cards (click to edit), and below them every wrist panel from
// your other profiles and the built-in standard, to copy onto the left or right arm as a whole (all its pages and
// buttons, its columns). Where the panel sits on the arm (size, side, offsets) stays: that belongs to each arm.
public class WristsPage : UserControl
{
    readonly ProfileModel p = Shell.Store.Profile;

    // A panel to copy from: where it comes from, and the panel itself.
    sealed record Source(string From, int Hand, WristModel Panel, ProfileModel Profile);

    public WristsPage()
    {
        var body = new StackPanel { Margin = new Thickness(16, 12), Spacing = 6 };
        body.Children.Add(new TextBlock { Text = "Wrist panels", FontSize = 24, FontWeight = FontWeight.SemiBold });
        body.Children.Add(new TextBlock { Classes = { "desc" }, Text = "Buttons on your forearms. Click a panel to edit it, or copy a whole panel from another profile onto one of your arms." });
        body.Children.Add(new HowYouPress("wrist"));
        body.Children.Add(Heading("Your panels"));
        body.Children.Add(new WrapPanel { Children = { OwnTile(0), OwnTile(1) } });

        var sources = Sources();
        body.Children.Add(Heading("Copy a whole panel"));
        body.Children.Add(new TextBlock
        {
            Classes = { "desc" }, MaxWidth = 900, HorizontalAlignment = HorizontalAlignment.Left,
            Text = "Every wrist panel in your other profiles and in the standard profile: open a profile to see its panels. 'Copy to left / right' asks whether to replace that arm's panel or add these as next pages (its place on your arm stays). Save to keep it.",
        });
        // one folded group per profile, so many profiles stay a short list
        foreach (var group in sources.GroupBy(x => x.From))
        {
            var rows = new StackPanel { Spacing = 4, Margin = new Thickness(0, 4, 0, 0) };
            foreach (var src in group)
                rows.Children.Add(SourceRow(src));
            body.Children.Add(new Expander
            {
                Header = new TextBlock { Text = $"{group.Key}   ({group.Count()} panel{(group.Count() == 1 ? "" : "s")})", FontWeight = FontWeight.SemiBold },
                Content = rows, IsExpanded = false, MaxWidth = 900, HorizontalAlignment = HorizontalAlignment.Left, Width = 900, Margin = new Thickness(0, 0, 0, 4),
            });
        }
        if (sources.Count == 0)
            body.Children.Add(new TextBlock { Classes = { "desc" }, Text = "No other wrist panels found." });
        Content = new ScrollViewer { Content = body };
    }

    static TextBlock Heading(string text) => new()
    {
        Text = text.ToUpperInvariant(), FontWeight = FontWeight.Bold, FontSize = 12, Foreground = Brush.Parse("#7FA8F0"), Margin = new Thickness(0, 12, 0, 4),
    };

    static WheelModel FirstPage(WristModel w) => w.Pages.GetValueOrDefault(w.Start ?? "") ?? w.Pages.Values.FirstOrDefault();

    static WheelView Picture(WristModel w) => new()
    {
        Wheel = FirstPage(w), IsWristPage = true, GridColumns = w.Columns, Width = 150, Height = 150, IsHitTestVisible = false,
        VerticalAlignment = VerticalAlignment.Top,
    };

    static string Summary(WristModel w)
    {
        int buttons = w.Pages.Values.Sum(pg => pg.Items.Count(i => i.Enabled));
        return $"{w.Pages.Count} page(s) · {buttons} buttons · {w.Columns} columns";
    }

    // The card (a button that opens the panel), with a large on / off switch above it.
    Control OwnTile(int hand)
    {
        var w0 = p.WristFor(hand);
        var sw = new ToggleSwitch { IsChecked = w0.Enabled, OnContent = "Panel on", OffContent = "Panel off", FontSize = 15, FontWeight = FontWeight.SemiBold, Margin = new Thickness(4, 0, 0, 4) };
        sw.IsCheckedChanged += (o, e) =>
        {
            if (w0.Enabled == (sw.IsChecked == true)) return; // only a real change
            w0.Enabled = sw.IsChecked == true;
            Shell.Store.MarkDirty();
            Shell.Refresh(); // the card shows On / Off
        };
        // drawn about 1.5x its normal size: easy to hit
        var big = new LayoutTransformControl { LayoutTransform = new ScaleTransform(1.5, 1.5), Child = sw, HorizontalAlignment = HorizontalAlignment.Left };
        return new StackPanel { Margin = new Thickness(0, 0, 12, 12), Children = { big, OwnCard(hand) } };
    }

    Button OwnCard(int hand)
    {
        var w = p.WristFor(hand);
        var shown = p.WristPages(hand); // a mirroring panel shows the other one's pages
        string state = !w.Enabled ? "Off" : w.Mirror ? $"Mirrors the {(hand == 0 ? "right" : "left")} panel" : "On";
        var tile = new Button
        {
            Classes = { "option" }, Width = 420, HorizontalContentAlignment = HorizontalAlignment.Left,
            Content = new StackPanel
            {
                Orientation = Orientation.Horizontal, Spacing = 14,
                Children =
                {
                    Picture(shown),
                    new StackPanel
                    {
                        Width = 220, VerticalAlignment = VerticalAlignment.Center, Spacing = 4,
                        Children =
                        {
                            new TextBlock { Text = hand == 0 ? "Left wrist panel" : "Right wrist panel", FontSize = 16, FontWeight = FontWeight.SemiBold },
                            new TextBlock { Text = Summary(shown), Foreground = Brush.Parse("#AAB2BF"), TextWrapping = TextWrapping.Wrap },
                            new TextBlock { Text = state, Foreground = Brush.Parse(w.Enabled ? "#3CBE6E" : "#F0A040") },
                        },
                    },
                },
            },
        };
        tile.Click += (s, e) => Shell.Go("wrist" + hand);
        return tile;
    }

    // A compact row: a small picture, which arm and what is on it, and the two copy buttons.
    Border SourceRow(Source s)
    {
        var toLeft = new Button { Content = "Copy to left", Classes = { "go" } };
        var toRight = new Button { Content = "Copy to right", Classes = { "go" } };
        toLeft.Click += async (o, e) => await CopyTo(this, s, 0);
        toRight.Click += async (o, e) => await CopyTo(this, s, 1);
        var pic = Picture(s.Panel);
        pic.Width = pic.Height = 64;
        var grid = new Grid { ColumnDefinitions = new ColumnDefinitions("76,*,Auto") };
        var text = new StackPanel
        {
            VerticalAlignment = VerticalAlignment.Center, Spacing = 2,
            Children =
            {
                new TextBlock { Text = s.Hand == 0 ? "Left panel" : "Right panel", FontWeight = FontWeight.SemiBold },
                new TextBlock { Text = Summary(s.Panel), Foreground = Brush.Parse("#AAB2BF") },
            },
        };
        var buttons = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6, VerticalAlignment = VerticalAlignment.Center, Children = { toLeft, toRight } };
        Grid.SetColumn(text, 1);
        Grid.SetColumn(buttons, 2);
        grid.Children.Add(pic);
        grid.Children.Add(text);
        grid.Children.Add(buttons);
        return new Border { Classes = { "row" }, Padding = new Thickness(6), Child = grid };
    }

    Border SourceTile(Source s)
    {
        var toLeft = new Button { Content = "Copy to left", Classes = { "go" } };
        var toRight = new Button { Content = "Copy to right", Classes = { "go" } };
        toLeft.Click += async (o, e) => await CopyTo(this, s, 0);
        toRight.Click += async (o, e) => await CopyTo(this, s, 1);
        return new Border
        {
            Classes = { "card" }, Width = 420, Margin = new Thickness(0, 0, 12, 12),
            Child = new StackPanel
            {
                Orientation = Orientation.Horizontal, Spacing = 14,
                Children =
                {
                    Picture(s.Panel),
                    new StackPanel
                    {
                        Width = 220, Spacing = 4,
                        Children =
                        {
                            new TextBlock { Text = (s.Hand == 0 ? "Left panel" : "Right panel"), FontSize = 16, FontWeight = FontWeight.SemiBold },
                            new TextBlock { Text = "from " + s.From, Foreground = Brush.Parse("#7FA8F0"), TextWrapping = TextWrapping.Wrap },
                            new TextBlock { Text = Summary(s.Panel), Foreground = Brush.Parse("#AAB2BF"), TextWrapping = TextWrapping.Wrap },
                            new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6, Margin = new Thickness(0, 6, 0, 0), Children = { toLeft, toRight } },
                        },
                    },
                },
            },
        };
    }

    // "+ Add page from…" on a wrist panel's own page: the same list of panels (one folded group per profile) in a
    // window; "Use this panel" then asks the same questions as Copy to left / right here (replace or add as next pages,
    // mirror from the other arm), for that panel's arm.
    public static async Task PickAndCopy(Avalonia.Visual owner, int hand)
    {
        var helper = new WristsPage(true);
        var sources = helper.Sources();
        var win = new Window
        {
            Title = "Add pages from another panel", Width = 760, Height = 640, WindowStartupLocation = WindowStartupLocation.CenterOwner,
            Background = Brush.Parse("#1B1E23"),
        };
        Source picked = null;
        var body = new StackPanel { Margin = new Thickness(18), Spacing = 6 };
        body.Children.Add(new TextBlock { Text = "Pick a panel", FontSize = 20, FontWeight = FontWeight.SemiBold });
        body.Children.Add(new TextBlock { Classes = { "desc" }, TextWrapping = TextWrapping.Wrap,
            Text = $"Every wrist panel in your other profiles and the standard profile. Next you choose whether it replaces your {(hand == 0 ? "left" : "right")} panel or is added as next pages." });
        foreach (var group in sources.GroupBy(x => x.From))
        {
            var rows = new StackPanel { Spacing = 4, Margin = new Thickness(0, 4, 0, 0) };
            foreach (var src in group)
            {
                var use = new Button { Content = "Use this panel", Classes = { "go" }, VerticalAlignment = VerticalAlignment.Center };
                var chosen = src;
                use.Click += (o, e) => { picked = chosen; win.Close(); };
                var pic = Picture(src.Panel);
                pic.Width = pic.Height = 64;
                var text = new StackPanel { VerticalAlignment = VerticalAlignment.Center, Spacing = 2, Children = {
                    new TextBlock { Text = src.Hand == 0 ? "Left panel" : "Right panel", FontWeight = FontWeight.SemiBold },
                    new TextBlock { Text = Summary(src.Panel), Foreground = Brush.Parse("#AAB2BF") } } };
                var grid = new Grid { ColumnDefinitions = new ColumnDefinitions("76,*,Auto") };
                Grid.SetColumn(text, 1);
                Grid.SetColumn(use, 2);
                grid.Children.Add(pic);
                grid.Children.Add(text);
                grid.Children.Add(use);
                rows.Children.Add(new Border { Classes = { "row" }, Padding = new Thickness(6), Child = grid });
            }
            body.Children.Add(new Expander
            {
                Header = new TextBlock { Text = $"{group.Key}   ({group.Count()} panel{(group.Count() == 1 ? "" : "s")})", FontWeight = FontWeight.SemiBold },
                Content = rows, IsExpanded = false, HorizontalAlignment = HorizontalAlignment.Stretch,
            });
        }
        if (sources.Count == 0)
            body.Children.Add(new TextBlock { Classes = { "desc" }, Text = "No other wrist panels found." });
        win.Content = new ScrollViewer { Content = body };
        if (TopLevel.GetTopLevel(owner) is not Window parent)
            return;
        await win.ShowDialog(parent);
        if (picked != null)
            await helper.CopyTo(owner, picked, hand);
    }

    // Every panel with pages of its own in the other profiles and the built-in standard (not this profile's own two).
    public static List<(string from, int hand, WristModel panel, ProfileModel profile)> AllSources(ProfileModel p) =>
        new WristsPage(true).Sources().Select(x => (x.From, x.Hand, x.Panel, x.Profile)).ToList();

    WristsPage(bool forTests) { }

    List<Source> Sources()
    {
        var list = new List<Source>();
        void Add(string from, ProfileModel prof)
        {
            foreach (var (k, w) in prof.Wheels)
                w.Name = k;
            for (int h = 0; h < 2; h++)
            {
                var w = prof.WristFor(h);
                if (w == null || w.Mirror || w.Pages.Count == 0 || w.Pages.Values.All(pg => pg.Items.Count == 0))
                    continue;
                foreach (var (k, pg) in w.Pages)
                    pg.Name = k;
                list.Add(new Source(from, h, w, prof));
            }
        }
        foreach (var name in Shell.Store.ProfileNames().Where(n => !string.Equals(n, Shell.Store.CurrentName, StringComparison.OrdinalIgnoreCase)))
        {
            try { Add(name, ProfileIO.FromText(File.ReadAllText(Path.Combine(Shell.Store.ProfilesDir, name)))); }
            catch (Exception) { } // a profile that can't be read is just not offered
        }
        try
        {
            string builtIn = Native.DefaultProfile();
            if (!string.IsNullOrEmpty(builtIn))
                Add("the standard profile (built in)", ProfileIO.FromText(builtIn));
        }
        catch (Exception) { }
        return list;
    }

    // owner: the page or window the question is asked over
    async Task CopyTo(Avalonia.Visual owner, Source s, int hand)
    {
        var target = p.WristFor(hand);
        string arm = hand == 0 ? "left" : "right", fromArm = s.Hand == 0 ? "left" : "right";
        var bring = WheelsToBring(p, s.Profile, s.Panel);
        int sided = Sided(s.Panel);
        int now = target.Pages.Values.Sum(pg => pg.Items.Count);
        var firstPage = target.Mirror ? null : FirstPage(target);
        // the two ways: replace this arm's panel, or add the copied pages after it (with a button to them)
        var replace = new RadioButton { GroupName = "how", IsChecked = now == 0 || target.Mirror, Content = new TextBlock { TextWrapping = TextWrapping.Wrap, MaxWidth = 480,
            Text = $"Replace my {arm} panel" + (now > 0 && !target.Mirror ? $" (its {target.Pages.Count} page(s) and {now} buttons go)" : "") } };
        var append = new RadioButton { GroupName = "how", IsChecked = !(now == 0 || target.Mirror), IsEnabled = firstPage != null, Content = new TextBlock { TextWrapping = TextWrapping.Wrap, MaxWidth = 480,
            Text = firstPage == null ? "Add as next page(s): your panel has no pages of its own yet"
                 : $"Add as next page(s): keep my panel, and put a button on '{firstPage.Title ?? firstPage.Name}' that goes to them" } };
        // from the other arm: buttons that put something on a side of the arm can follow this arm
        var mirror = new CheckBox { IsChecked = true, IsVisible = s.Hand != hand && sided > 0, Content = new TextBlock { TextWrapping = TextWrapping.Wrap, MaxWidth = 480,
            Text = $"Mirror it for this arm: {sided} button(s) that show the screen on the {fromArm} wrist show it on the {arm} wrist instead" } };
        string notes = "Where the panel sits on your arm stays as it is." +
                       (bring.Count > 0 ? $"\n\nIts buttons open wheels this profile doesn't have; they are copied along: {string.Join(", ", bring)}." : "") +
                       (target.Mirror ? $"\n\nYour {arm} panel mirrored the other arm: it gets its own pages now." : "");
        var box = new StackPanel { Spacing = 8, Children = { replace, append, mirror, new TextBlock { Classes = { "desc" }, MaxWidth = 500, Text = notes } } };
        if (!await Dialogs.Ask(owner, "Copy a wrist panel", $"Copy the {fromArm} panel from {s.From} onto your {arm} arm:", box))
            return;
        bool doMirror = mirror.IsVisible && mirror.IsChecked == true;
        string said;
        if (append.IsChecked == true)
        {
            var link = Append(p, s.Profile, s.Panel, hand, doMirror);
            said = link ? $"Added after your {arm} panel, with a button on its first page that goes there."
                        : $"Added after your {arm} panel. Its first page is full (16 buttons): add a 'Go to another page' button for it yourself.";
        }
        else
        {
            Copy(p, s.Profile, s.Panel, hand, doMirror);
            said = $"Copied onto your {arm} arm.";
        }
        if (bring.Count > 0)
            said += $" Wheels copied along: {string.Join(", ", bring)}.";
        Shell.Store.MarkDirty();
        Shell.SetStatus(said + " Save (Ctrl+S) to keep it.");
        if (Shell.Current == "wrist" + hand)
            Shell.Refresh(); // started from that panel's own page ("+ Add page from…"): show what came in
        else
            Shell.Go("wrist" + hand);
    }

    // Buttons whose action sits on one side: the screen shown on the left / right wrist.
    static int Sided(WristModel panel) =>
        panel.Pages.Values.SelectMany(pg => pg.Items).Count(i => i.Action?.Type == ActionType.Screen && i.Action.Place is "wrist_left" or "wrist_right");

    static void MirrorSides(IEnumerable<WheelModel> pages)
    {
        foreach (var it in pages.SelectMany(pg => pg.Items))
            if (it.Action?.Type == ActionType.Screen)
                it.Action.Place = it.Action.Place switch { "wrist_left" => "wrist_right", "wrist_right" => "wrist_left", var x => x };
    }

    // The wheels the panel's buttons open that this profile lacks, with the sub-wheels those open in turn (from the
    // panel's own profile). A wheel this profile already has by that name is used as it is.
    public static List<string> WheelsToBring(ProfileModel p, ProfileModel from, WristModel panel)
    {
        var bring = new List<string>();
        void Need(string name)
        {
            if (name == null || p.Wheels.ContainsKey(name) || bring.Contains(name) || from?.Wheels.ContainsKey(name) != true)
                return;
            bring.Add(name);
            foreach (var it in from.Wheels[name].Items)
                if (it.Action?.Type == ActionType.Wheel)
                    Need(it.Action.Wheel);
        }
        foreach (var it in panel.Pages.Values.SelectMany(pg => pg.Items))
            if (it.Action?.Type == ActionType.Wheel)
                Need(it.Action.Wheel);
        return bring;
    }

    static void BringWheels(ProfileModel p, ProfileModel from, WristModel panel)
    {
        foreach (var name in WheelsToBring(p, from, panel))
        {
            var w = ItemLists.Clone(from.Wheels[name]);
            w.Name = name;
            p.Wheels[name] = w;
        }
    }

    // Page names for copied pages: unique over both arms ("Main (left)" when that name is taken).
    public static Dictionary<string, string> NamesFor(ProfileModel p, WristModel source, int hand, bool keepOwnNames = false)
    {
        string arm = hand == 0 ? "left" : "right";
        var taken = new HashSet<string>(p.WristFor(1 - hand).Pages.Keys, StringComparer.Ordinal);
        if (keepOwnNames) // adding to this arm: its own pages stay, so their names are taken too
            taken.UnionWith(p.WristFor(hand).Pages.Keys);
        var rename = new Dictionary<string, string>();
        foreach (var name in source.Pages.Keys)
        {
            string n = name;
            for (int k = 2; taken.Contains(n); k++)
                n = $"{name} ({arm}{(k > 2 ? " " + k : "")})";
            rename[name] = n;
            taken.Add(n);
        }
        return rename;
    }

    // Copies of the source's pages under their new names, their "go to page" buttons following.
    static List<WheelModel> CopiedPages(WristModel source, Dictionary<string, string> rename, bool mirror)
    {
        var list = new List<WheelModel>();
        foreach (var (name, pg) in source.Pages)
        {
            var c = ItemLists.Clone(pg);
            c.Name = rename[name];
            foreach (var it in c.Items)
                if (it.Action?.Type == ActionType.Page && it.Action.Page != null && rename.TryGetValue(it.Action.Page, out var to))
                    it.Action.Page = to;
            list.Add(c);
        }
        if (mirror)
            MirrorSides(list);
        return list;
    }

    // Replace: the source's pages, first page and columns replace this arm's; its place on the arm stays. The wheels
    // its buttons open come along when this profile lacks them.
    public static void Copy(ProfileModel p, ProfileModel from, WristModel source, int hand, bool mirror)
    {
        BringWheels(p, from, source);
        var rename = NamesFor(p, source, hand);
        var target = p.WristFor(hand);
        target.Pages = CopiedPages(source, rename, mirror).ToDictionary(pg => pg.Name);
        target.Start = source.Start != null && rename.TryGetValue(source.Start, out var st) ? st : target.Pages.Keys.First();
        target.Columns = source.Columns;
        target.Mirror = false;
        target.Enabled = true;
    }

    // Add as next pages: this arm keeps its pages; the copied ones come after, and its first page gets a button that
    // goes to the copied first page (false when that page is full: 16 buttons).
    public static bool Append(ProfileModel p, ProfileModel from, WristModel source, int hand, bool mirror)
    {
        BringWheels(p, from, source);
        var target = p.WristFor(hand);
        var rename = NamesFor(p, source, hand, keepOwnNames: true);
        foreach (var c in CopiedPages(source, rename, mirror))
            target.Pages[c.Name] = c;
        string copiedStart = source.Start != null && rename.TryGetValue(source.Start, out var st) ? st : rename.Values.First();
        var first = FirstPage(target);
        if (first == null || first.Items.Count >= 16)
            return false;
        var to = target.Pages[copiedStart];
        first.Items.Add(new WheelItemModel { Label = to.Title ?? to.Name, Action = new ActionModel { Type = ActionType.Page, Page = copiedStart } });
        return true;
    }
}
