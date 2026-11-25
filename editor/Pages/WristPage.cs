using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;
using static OverviewPrototype.Fields;

namespace OverviewPrototype;

// A wrist panel (hand 0 = left forearm, 1 = right): its pages as tabs, the page's button grid (click a button to edit
// it beside the picture, drag one onto another to swap), and on the right the page's buttons or the panel's settings.
// Port of the WinForms SettingsEditor wrist page (WristButtons + BuildWrist). List rows: drag ≡ or ↑ ↓.
public class WristPage : UserControl
{
    static bool reopenSettings; // rebuilt after Mirror changed: open on the settings tab again
    readonly ProfileModel p = Shell.Store.Profile;
    readonly int hand;
    readonly WristModel wp;     // this arm's panel settings
    readonly WristModel owner;  // whose pages it shows (the other panel's when it mirrors)
    WheelModel shown;
    WheelItemModel editing;
    readonly WheelView picture = new() { VerticalAlignment = Avalonia.Layout.VerticalAlignment.Top, IsWristPage = true, Width = 300, Height = 300, Margin = new Thickness(0, 0, 16, 0) };
    readonly WrapPanel tabs = new() { MaxWidth = 880 };
    readonly TextBox pageTitle = new() { Width = 220 };
    readonly CheckBox first = new() { Content = "Shown first" };
    readonly StackPanel editBar = new() { Orientation = Orientation.Horizontal, Spacing = 8, IsVisible = false, Margin = new Thickness(0, 0, 0, 8) };
    // a fixed least height (like the WinForms editor): a button with fewer settings doesn't make the page shorter, so
    // the page doesn't jump (scroll) when you click from one button to another
    readonly ContentControl editHost = new() { Width = 560, MinHeight = 720 };
    readonly StackPanel rows = new() { Spacing = 4 };
    bool filling;

    public WristPage(int wristHand, bool settingsTab = false)
    {
        hand = wristHand;
        SettingsPageView.EnsureParts(p);
        wp = p.WristFor(hand);
        owner = p.WristPages(hand);
        shown = owner.Pages.GetValueOrDefault(owner.Start ?? "") ?? owner.Pages.Values.FirstOrDefault();
        picture.GridColumns = wp.Columns;
        picture.ItemClicked += Edit;
        picture.ItemDropped += (a, b) =>
        {
            if (shown == null) return;
            int i = shown.Items.IndexOf(a), j = shown.Items.IndexOf(b);
            if (i < 0 || j < 0) return;
            (shown.Items[i], shown.Items[j]) = (shown.Items[j], shown.Items[i]);
            Structure();
            Edit(a);
        };

        // ---- left card: pages, page row, add buttons, picture + the chosen button's settings
        pageTitle.TextChanged += (s, e) =>
        {
            if (filling || shown == null || (pageTitle.Text ?? "") == (shown.Title ?? "")) return;
            shown.Title = pageTitle.Text;
            picture.InvalidateVisual();
            Shell.Store.MarkDirty();
        };
        pageTitle.LostFocus += (s, e) => ShowTabs();
        first.IsCheckedChanged += (s, e) =>
        {
            if (filling || shown == null || first.IsChecked != true || owner.Start == shown.Name) return;
            owner.Start = shown.Name;
            Shell.Store.MarkDirty();
            ShowTabs();
        };
        var dupPage = Btn("Duplicate page", DuplicatePage);
        ToolTip.SetTip(dupPage, "A copy of this page with all its buttons. Nothing opens it yet: add a 'Go to another page' button for it.");
        var pageRow = new StackPanel
        {
            Orientation = Orientation.Horizontal, Spacing = 10,
            Children = { new TextBlock { Text = "Page title:", VerticalAlignment = VerticalAlignment.Center }, pageTitle, first, dupPage, Btn("Delete page", DeletePage, red: true) },
        };
        editBar.Children.Add(Btn("◀ Move", () => Move(-1)));
        editBar.Children.Add(Btn("Move ▶", () => Move(1)));
        editBar.Children.Add(Btn("Duplicate", DuplicateButton));
        editBar.Children.Add(Btn("Delete button", DeleteButton, red: true));
        var left = new StackPanel { Spacing = 10 };
        if (!ReferenceEquals(owner, wp))
            left.Children.Add(new TextBlock { Classes = { "desc" }, Text = "This panel mirrors the " + (hand == 1 ? "left" : "right") + " panel: these are its pages." });
        left.Children.Add(tabs);
        left.Children.Add(pageRow);
        left.Children.Add(new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Children = { Btn("+ Add button", AddButton, blue: true), Btn("+ Add page", AddPage), Btn("+ Add page from…", () => _ = WristsPage.PickAndCopy(this, hand)) } });
        left.Children.Add(new WrapPanel { Children = { picture, new StackPanel { Children = { editBar, editHost } } } });

        // ---- right: this page's buttons | the panel's settings
        var listCard = Cards.Make("Buttons on this page", new StackPanel
        {
            Spacing = 6, Width = 560,
            Children = { new TextBlock { Classes = { "desc" }, MaxWidth = 560, Text = "Click one to change it. Drag ≡ (or ↑ ↓) to reorder it (or drag a button in the picture onto another to swap them). The circle switches it on or off: off = not on the panel, kept here." }, ItemSort.Bar(RefreshList), rows },
        });
        var panelCard = Cards.Make("Panel", PanelSettings());
        // the tip inside the arm card, under its numbers (the column is too narrow for it beside them)
        var armRow = Cards.Make("On the arm", new StackPanel { Spacing = 12, Children = { ArmSettings(), new CalibrationTip("panel", "panel", "bigger / smaller", "above") { Width = double.NaN, Margin = new Thickness(0) } } });
        var showCard = Cards.Make("When it shows", ShowSettings());
        var libCard = Cards.Make("All buttons and bindings", new LibraryPanel(() => shown, it => { Structure(); Edit(it); }, "the page shown on the left"));
        var tabButtons = Btn("☰  Buttons on this page", null);
        var tabSettings = Btn("⚙  Panel settings", null);
        var tabLib = Btn("🔍  All buttons", null);
        void ShowRight(int which)
        {
            listCard.IsVisible = which == 0;
            panelCard.IsVisible = armRow.IsVisible = showCard.IsVisible = which == 1;
            libCard.IsVisible = which == 2;
            tabButtons.Classes.Set("tabon", which == 0);
            tabSettings.Classes.Set("tabon", which == 1);
            tabLib.Classes.Set("tabon", which == 2);
        }
        tabButtons.Click += (s, e) => ShowRight(0);
        tabSettings.Click += (s, e) => ShowRight(1);
        tabLib.Click += (s, e) => ShowRight(2);
        ShowRight(settingsTab || reopenSettings ? 1 : 0);
        reopenSettings = false;

        string arm = hand == 0 ? "Left" : "Right";
        Content = new ScrollViewer
        {
            Content = new StackPanel
            {
                Margin = new Thickness(16, 12), Spacing = 6,
                Children =
                {
                    new TextBlock { Text = arm + " wrist panel", FontSize = 24, FontWeight = FontWeight.SemiBold },
                    new TextBlock { Classes = { "desc" }, Text = $"Buttons on your {arm.ToLowerInvariant()} forearm: turn it toward your face and look at it, then press with your other hand." },
                    new HowYouPress("wrist" + hand),
                    new WrapPanel
                    {
                        Children =
                        {
                            Cards.Make("Buttons", left),
                            new StackPanel
                            {
                                Spacing = 10,
                                Children = { new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Children = { tabButtons, tabLib, tabSettings } }, listCard, panelCard, armRow, showCard, libCard },
                            },
                        },
                    },
                },
            },
        };
        ShowTabs();
        Edit(null);
        AddHandler(KeyDownEvent, (s, e) =>
        {
            if (editing == null || e.Source is TextBox || e.Source is NumericUpDown || e.Source is ComboBox)
                return;
            if (e.Key == Key.Delete) { DeleteButton(); e.Handled = true; }
            else if (e.Key == Key.D && e.KeyModifiers == KeyModifiers.Control) { DuplicateButton(); e.Handled = true; }
            else if (e.Key == Key.Left && e.KeyModifiers == KeyModifiers.Alt) { Move(-1); e.Handled = true; }
            else if (e.Key == Key.Right && e.KeyModifiers == KeyModifiers.Alt) { Move(1); e.Handled = true; }
        }, Avalonia.Interactivity.RoutingStrategies.Tunnel);
    }

    // ---- panel settings (WinForms BuildWrist: the wrist, wristArm, wristNums and wristShow forms)
    Form PanelSettings()
    {
        string armName = hand == 1 ? "right" : "left", otherHand = hand == 1 ? "left" : "right";
        var other = p.WristFor(1 - hand);
        var f = new Form { LabelWidth = 200 };
        var pageChoices = wp.Pages.Keys.Select(k => new Choice(k, k)).ToList();
        if (!wp.Pages.ContainsKey(wp.Start ?? ""))
            pageChoices.Add(new Choice(wp.Start ?? "", (wp.Start ?? "") + "  (missing: add a page with this name)"));
        f.AddRow("", Check($"Show buttons on my {armName} forearm", () => wp.Enabled, v => wp.Enabled = v),
            $"Appears only when you turn the {armName} forearm toward your eyes AND look at it. Your {otherHand} hand presses the buttons.");
        Control lFirst = null;
        f.AddRow("", Check($"Mirror the {otherHand} panel (same pages and buttons)", () => wp.Mirror, v =>
        {
            wp.Mirror = v;
            if (v) other.Mirror = false;
            Form.Show(lFirst, !v);
            // the left side shows the pages this panel now uses (its own, or the other panel's): rebuild the page, staying on this tab
            reopenSettings = true;
            Avalonia.Threading.Dispatcher.UIThread.Post(Shell.Refresh);
        }),
            $"Untick to give the {armName} forearm its own pages. Ticking this unticks it on the {otherHand} panel (they can't both mirror). Size, side and the settings below stay separate either way.");
        lFirst = f.AddRow("First page", Pick(pageChoices, () => wp.Start, v => wp.Start = v),
            "The page shown when the panel appears. Its pages are listed under it on the left.");
        Form.Show(lFirst, !wp.Mirror);
        f.AddRow("Press buttons by", new TextBlock { Classes = { "desc" }, Text = "Set at the top of this page (How you press).", VerticalAlignment = Avalonia.Layout.VerticalAlignment.Center });
        f.AddRow("Columns", Int(1, 4, 1, () => wp.Columns, v => { wp.Columns = v; picture.GridColumns = v; picture.InvalidateVisual(); }));
        return f;
    }


    Form ArmSettings()
    {
        var f = new Form { LabelWidth = 200 };
        Control lRot = null;
        f.AddRow("Side of the arm", Pick(new Choice[] { new("inner", "Inner side / underside (palm side)"), new("back", "Back of the wrist (like a watch)") }, () => wp.Face, v => wp.Face = v));
        f.AddRow("Text direction", Pick(new Choice[] { new("fingers", "Top toward my fingers (hand held up, palm to face)"), new("between", "In between (arm half raised)"), new("along_arm", "Along the forearm (arm held across my body)"), new("custom", "Custom angle (under Fine-tune, or turn it in adjust mode)") },
            () => wp.TextUp ?? "fingers", v => { wp.TextUp = v; Form.Show(lRot, v == "custom"); }));
        f.AddRow("Placed by hand", ResetPose(() => wp.Pose != null, () => wp.Pose = null),
            "Adjust positions in the headset: grab the panel with your other hand's grip and put it anywhere on the arm, at any angle. That spot replaces side, text direction, toward the elbow, off the arm and sideways.");
        f.AddRow("Panel size (m)", Num(0.05m, 0.4m, 0.01m, 2, () => wp.Size, v => wp.Size = v), "0.11 m is about the width of a large phone.");
        lRot = f.AddRow("Custom angle (deg)", Num(-180, 180, 5, 0, () => wp.RotateDeg, v => wp.RotateDeg = v),
            "0 = text along the forearm. Or place the panel by hand in the headset (Adjust positions): grab it and turn your hand.");
        Form.Show(lRot, wp.TextUp == "custom");
        f.AddRow("Toward the elbow (m)", Num(-0.1m, 0.3m, 0.01m, 2, () => wp.TowardElbow, v => wp.TowardElbow = v), "Distance back along the forearm from the controller.");
        f.AddRow("Off the arm (m)", Num(-0.1m, 0.15m, 0.005m, 3, () => wp.Lift, v => wp.Lift = v));
        f.AddRow("Sideways (m)", Num(-0.2m, 0.2m, 0.005m, 3, () => wp.Side, v => wp.Side = v), "Across the forearm. Easiest to set by hand: Adjust positions in the headset.");
        return f;
    }

    Form ShowSettings()
    {
        var f = new Form { LabelWidth = 200 };
        var cones = new ConesView { GazeDeg = wp.GazeDeg, FacingDeg = wp.FacingDeg };
        f.AddRow("Must look within (deg)", Num(5, 60, 1, 0, () => wp.GazeDeg, v => { wp.GazeDeg = v; cones.GazeDeg = v; }), "How close to the centre of your view the panel must be.");
        f.AddRow("Arm must face me within (deg)", Num(5, 80, 1, 0, () => wp.FacingDeg, v => { wp.FacingDeg = v; cones.FacingDeg = v; }), "Smaller = you must turn the arm more deliberately.");
        f.AddRow("", cones);
        f.AddRow("Show after looking (ms)", Int(0, 2000, 50, () => wp.ShowMs, v => wp.ShowMs = v));
        f.AddRow("Hide after looking away (ms)", Int(0, 3000, 50, () => wp.HideMs, v => wp.HideMs = v));
        return f;
    }

    // ---- pages
    void ShowTabs()
    {
        tabs.Children.Clear();
        // the order you go through them: the first page, then each page a button opens, in button order
        var order = new List<WheelModel>();
        void Visit(WheelModel w)
        {
            if (w == null || order.Contains(w)) return;
            order.Add(w);
            foreach (var it in w.Items)
                if (it.Action?.Type == ActionType.Page && it.Action.Page != null)
                    Visit(owner.Pages.GetValueOrDefault(it.Action.Page));
        }
        Visit(owner.Pages.GetValueOrDefault(owner.Start ?? ""));
        foreach (var w in owner.Pages.Values)
            Visit(w); // pages nothing opens go last
        int num = 0;
        foreach (var pg in order)
        {
            var page = pg;
            num++;
            string nth = num switch { 1 => "1st page", 2 => "2nd page", 3 => "3rd page", var n => n + "th page" };
            if (tabs.Children.Count > 0)
                tabs.Children.Add(new TextBlock { Text = "→", FontSize = 18, VerticalAlignment = VerticalAlignment.Bottom, Margin = new Thickness(0, 0, 8, 12), Foreground = Brush.Parse("#AAB2BF") });
            var b = Btn(string.IsNullOrWhiteSpace(page.Title) ? page.Name : page.Title, () => { shown = page; ShowTabs(); Edit(null); });
            if (ReferenceEquals(page, shown))
                b.Classes.Add("current");
            // the page's place in the order above it, so it reads as a page (not an ordinary button)
            tabs.Children.Add(new StackPanel { Spacing = 2, Margin = new Thickness(0, 0, 8, 8), Children =
            {
                new TextBlock { Text = nth + (page.Name == owner.Start ? " · shown first" : ""), FontSize = 11, FontWeight = FontWeight.Bold,
                                Foreground = Brush.Parse("#7FA8F0"), HorizontalAlignment = HorizontalAlignment.Center },
                b,
            } });
        }
        picture.Wheel = shown;
        picture.InvalidateVisual();
        filling = true;
        pageTitle.Text = shown?.Title ?? "";
        first.IsChecked = shown != null && owner.Start == shown.Name;
        first.IsEnabled = first.IsChecked != true; // a page stops being first when another one is made first
        filling = false;
        RefreshList();
    }

    void AddPage()
    {
        string name = "page" + (owner.Pages.Count + 1);
        while (owner.Pages.ContainsKey(name)) name += "_";
        var np = new WheelModel { Name = name, Title = "Page " + (owner.Pages.Count + 1) };
        owner.Pages[name] = np;
        if (shown != null && shown.Items.Count < 16) // a way to reach it from the page you were on
            shown.Items.Add(new WheelItemModel { Label = "Next page", Action = new ActionModel { Type = ActionType.Page, Page = name } });
        Shell.Store.MarkDirty();
        shown = np;
        ShowTabs();
        Edit(null);
    }

    void DuplicatePage()
    {
        if (shown == null) return;
        var c = ItemLists.Clone(shown);
        string n = shown.Name + " copy";
        for (int k = 2; owner.Pages.ContainsKey(n); k++) n = shown.Name + " copy " + k;
        c.Name = n;
        c.Title = (shown.Title ?? shown.Name) + " copy";
        owner.Pages[n] = c;
        Shell.Store.MarkDirty();
        shown = c;
        ShowTabs();
        Edit(null);
    }

    async void DeletePage()
    {
        if (shown == null) return;
        if (owner.Pages.Count <= 1) { Shell.SetStatus("A panel needs at least one page. Switch the panel off instead.", true); return; }
        if (!await Dialogs.Confirm(this, "Delete page", $"Delete the page '{shown.Title ?? shown.Name}' and its {shown.Items.Count} buttons?\n\nButtons on other pages that opened it are removed too.", "Delete"))
            return;
        string gone = shown.Name;
        owner.Pages.Remove(gone);
        foreach (var pg in owner.Pages.Values) // nothing may open a page that isn't there
            pg.Items.RemoveAll(i => i.Action?.Type == ActionType.Page && i.Action.Page == gone);
        if (owner.Start == gone)
            owner.Start = owner.Pages.Keys.First();
        shown = owner.Pages.GetValueOrDefault(owner.Start) ?? owner.Pages.Values.First();
        Shell.Store.MarkDirty();
        ShowTabs();
        Edit(null);
    }

    // ---- buttons
    void Edit(WheelItemModel it)
    {
        editing = it;
        picture.Highlight = it;
        RefreshList();
        editBar.IsVisible = it != null;
        if (it == null || shown == null)
        {
            editHost.Content = new TextBlock { Text = "Click a button in the picture to change it here.", Classes = { "desc" }, Margin = new Thickness(4, 8) };
            return;
        }
        var panel = new ItemPanel(shown, it, owner);
        panel.Changed += () => { picture.InvalidateVisual(); RefreshList(); Shell.Store.MarkDirty(); };
        editHost.Content = panel;
    }

    void AddButton()
    {
        if (shown == null) return;
        if (shown.Items.Count >= 16) { Shell.SetStatus("A page holds at most 16 buttons. Add another page.", true); return; }
        var it = new WheelItemModel { Label = "New button", Action = new ActionModel { Type = ActionType.Key, Keys = "F1" } };
        shown.Items.Add(it);
        Structure();
        Edit(it);
    }

    void DuplicateButton()
    {
        if (editing == null || shown == null) return;
        if (shown.Items.Count >= 16) { Shell.SetStatus("A page holds at most 16 buttons.", true); return; }
        var c = ItemLists.Clone(editing);
        shown.Items.Insert(shown.Items.IndexOf(editing) + 1, c);
        Structure();
        Edit(c);
    }

    void DeleteButton()
    {
        if (editing == null || shown == null) return;
        shown.Items.Remove(editing);
        Structure();
        Edit(null);
    }

    void Move(int by) => MoveItem(editing, by);

    void MoveItem(WheelItemModel it, int by)
    {
        if (it == null || shown == null) return;
        int i = shown.Items.IndexOf(it), j = i + by;
        if (i < 0 || j < 0 || j >= shown.Items.Count) return;
        shown.Items.RemoveAt(i);
        shown.Items.Insert(j, it);
        Structure();
    }

    void Structure()
    {
        Shell.Store.MarkDirty();
        picture.InvalidateVisual();
        RefreshList();
    }

    void RefreshList()
    {
        rows.Children.Clear();
        if (shown != null)
            foreach (var it in ItemSort.Sorted(shown.Items))
                rows.Children.Add(Row(it));
        if (shown == null || shown.Items.Count == 0)
            rows.Children.Add(new TextBlock { Classes = { "desc" }, Text = "No buttons yet: + Add button." });
    }

    Control Row(WheelItemModel it)
    {
        var up = Glyph("↑", () => MoveItem(it, -1), "Move up");
        var down = Glyph("↓", () => MoveItem(it, 1), "Move down");
        var onOff = Glyph(it.Enabled ? "✔" : "○", () => { it.Enabled = !it.Enabled; Structure(); },
                          it.Enabled ? "On: click to take it off the panel (kept in this list)" : "Off: click to put it back on the panel");
        onOff.Classes.Add("onoff");
        onOff.Classes.Set("on", it.Enabled);
        var grid = new Grid { ColumnDefinitions = new ColumnDefinitions("26,30,30,*,230,44") };
        var label = new TextBlock { Text = string.IsNullOrWhiteSpace(it.Label) ? "(no label)" : it.Label, Classes = { "rowmain" } };
        var does = new TextBlock { Text = EditorApi.Plain(it.Action), Classes = { "rowsub" } };
        Grid.SetColumn(up, 1); Grid.SetColumn(down, 2); Grid.SetColumn(label, 3); Grid.SetColumn(does, 4); Grid.SetColumn(onOff, 5);
        grid.Children.Add(up); grid.Children.Add(down); grid.Children.Add(label); grid.Children.Add(does); grid.Children.Add(onOff);
        var row = new Border { Classes = { "row" }, Child = grid, Cursor = new Cursor(StandardCursorType.Hand), Tag = it };
        grid.Children.Add(RowDrag.Handle(rows, row, it, () => shown.Items, Structure)); // ≡ in column 0
        row.Classes.Set("selected", ReferenceEquals(it, editing));
        row.Classes.Set("off", !it.Enabled);
        row.PointerReleased += (s, e) => { if (e.Source is not Button && (e.Source as Control)?.Parent is not Button) Edit(it); };
        return row;
    }

    static Button Btn(string text, Action click, bool blue = false, bool red = false)
    {
        var b = new Button { Content = text, Classes = { "pill" }, Margin = new Thickness(0) };
        if (blue) b.Classes.Add("go");
        if (red) b.Classes.Add("danger");
        if (click != null)
            b.Click += (s, e) => click();
        return b;
    }

    static Button Glyph(string text, Action click, string tip)
    {
        var b = new Button { Content = text, Classes = { "glyph" } };
        ToolTip.SetTip(b, tip);
        b.Click += (s, e) => click();
        return b;
    }
}
