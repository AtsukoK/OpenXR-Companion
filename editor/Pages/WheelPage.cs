using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// One radial wheel: its family as tabs (main wheel and sub-wheels), name and title, the picture (click an item to edit
// it, drag one onto another to swap) with the item's settings beside it, and on the right its items or what opens it.
// Port of the WinForms WheelPageEditor (not yet: dragging list rows).
public class WheelPage : UserControl
{
    readonly ProfileModel p = Shell.Store.Profile;
    readonly WheelModel w;
    readonly WheelView picture = new() { VerticalAlignment = Avalonia.Layout.VerticalAlignment.Top, Width = 320, Height = 320, Margin = new Thickness(0, 0, 16, 0) };
    // a fixed least height (like the WinForms editor): a button with fewer settings doesn't make the page shorter, so
    // the page doesn't jump (scroll) when you click from one button to another
    readonly ContentControl editHost = new() { Width = 560, MinHeight = 720 };
    readonly StackPanel editBar = new() { Orientation = Orientation.Horizontal, Spacing = 8, IsVisible = false, Margin = new Thickness(0, 0, 0, 8) };
    readonly StackPanel itemRows = new() { Spacing = 4 };
    readonly StackPanel openerRows = new() { Spacing = 4 };
    WheelItemModel editing;

    // Duplicate wheel / Delete wheel (WinForms page bar). A copy gets a free name ("<name>_copy"); deleting says how many
    // buttons still open the wheel (they then open nothing until changed, like WinForms).
    Control WheelBar()
    {
        var dup = new Button { Content = "Duplicate wheel", Classes = { "pill" } };
        var del = new Button { Content = "Delete wheel", Classes = { "pill", "danger" } };
        dup.Click += (s, e) =>
        {
            var c = ItemLists.Clone(w);
            c.Name = w.Name + "_copy";
            while (p.Wheels.ContainsKey(c.Name)) c.Name += "_";
            c.Title = (string.IsNullOrWhiteSpace(w.Title) ? w.Name : w.Title) + " copy";
            p.Wheels[c.Name] = c;
            Shell.Store.MarkDirty();
            Shell.Go("wheel:" + c.Name);
            Shell.SetStatus("Made a copy: '" + c.Name + "'. Nothing opens it yet: add a button that opens it ('Opened by' tab).");
        };
        del.Click += async (s, e) =>
        {
            int openers = p.Bindings.Count(b => b.Action?.Type == ActionType.Wheel && b.Action.Wheel == w.Name)
                + p.Wheels.Values.Sum(o => o.Items.Count(i => i.Action?.Type == ActionType.Wheel && i.Action.Wheel == w.Name))
                + Enumerable.Range(0, 2).Sum(h => p.WristFor(h).Pages.Values.Sum(o => o.Items.Count(i => i.Action?.Type == ActionType.Wheel && i.Action.Wheel == w.Name)));
            string more = openers == 0 ? "" : $"\n\n{openers} button(s) open this wheel; they will open nothing until you change them.";
            if (!await Dialogs.Confirm(this, "Delete wheel", $"Delete wheel '{w.Name}' and its {w.Items.Count} items?" + more, "Delete")) return;
            foreach (var k in p.Wheels.Where(kv => ReferenceEquals(kv.Value, w)).Select(kv => kv.Key).ToList())
                p.Wheels.Remove(k);
            Shell.Store.MarkDirty();
            Shell.Go("wheels");
            Shell.SetStatus("Deleted wheel '" + w.Name + "'.");
        };
        return new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Margin = new Thickness(0, 4, 0, 4), Children = { dup, del } };
    }

    public WheelPage(WheelModel wheel)
    {
        w = wheel;
        picture.Wheel = w;
        picture.ItemClicked += Edit;
        picture.ItemDropped += (a, b) =>
        {
            int i = w.Items.IndexOf(a), j = w.Items.IndexOf(b);
            if (i < 0 || j < 0) return;
            (w.Items[i], w.Items[j]) = (w.Items[j], w.Items[i]);
            Structure();
            Edit(a);
        };

        // ---- left: tabs, name and title, add buttons, picture + item settings
        var name = new TextBox { Width = 220, Text = w.Name };
        var title = new TextBox { Width = 220, Text = w.Title };
        var names = new Form { LabelWidth = 80 };
        names.AddRow("Name", name, "Used by 'Open a wheel' actions. Renaming updates them.");
        names.AddRow("Title", title, "Shown in the middle of the wheel.");
        name.LostFocus += (s, e) => Rename(name);
        title.TextChanged += (s, e) => { if ((w.Title ?? "") == (title.Text ?? "")) return; w.Title = title.Text; picture.InvalidateVisual(); Shell.Store.MarkDirty(); };

        var addItem = Btn("+ Add item", AddItem, blue: true);
        var addSub = Btn("+ Add sub-wheel", AddSubWheel);
        ToolTip.SetTip(addSub, "Makes a new wheel with a Back item, and puts a 'More' item on this wheel that opens it.");
        editBar.Children.Add(Btn("◀ Move", () => Move(-1)));
        editBar.Children.Add(Btn("Move ▶", () => Move(1)));
        editBar.Children.Add(Btn("Duplicate", Duplicate));
        editBar.Children.Add(Btn("Delete item", Delete, red: true));

        var left = new StackPanel
        {
            Spacing = 10,
            Children =
            {
                Family(),
                names,
                new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Children = { addItem, addSub } },
                new WrapPanel { Children = { picture, new StackPanel { Children = { editBar, editHost } } } },
            },
        };

        // ---- right: Items | Opened by
        var itemsCard = Cards.Make("Items on this wheel", new StackPanel
        {
            Spacing = 6, Width = 560,
            Children = { Note("Click one to change it. Drag ≡ (or ↑ ↓) to reorder it (or drag an item in the picture onto another to swap them). On / Off switches it: off = not on the wheel, kept here."), ItemSort.Bar(RefreshItems), itemRows },
        });
        var openCard = Cards.Make("Opened by", new StackPanel
        {
            Spacing = 6, Width = 560,
            Children = { Note("The controller buttons that open this wheel. On / Off switches one; ✕ deletes it. Click one for all its settings."),
                         openerRows, Btn("+ Add a button that opens it", AddOpener, blue: true) },
        });
        var libCard = Cards.Make("All buttons and bindings", new LibraryPanel(() => w, it => { Structure(); Edit(it); }, "this wheel"));
        var tabItems = Btn("☰  Items on this wheel", null);
        var tabOpen = Btn("◎  Opened by", null);
        var tabLib = Btn("🔍  All buttons", null);
        void ShowRight(int which)
        {
            itemsCard.IsVisible = which == 0;
            openCard.IsVisible = which == 1;
            libCard.IsVisible = which == 2;
            tabItems.Classes.Set("tabon", which == 0);
            tabOpen.Classes.Set("tabon", which == 1);
            tabLib.Classes.Set("tabon", which == 2);
        }
        tabItems.Click += (s, e) => ShowRight(0);
        tabOpen.Click += (s, e) => ShowRight(1);
        tabLib.Click += (s, e) => ShowRight(2);
        ShowRight(0);
        var right = new StackPanel
        {
            Spacing = 10,
            Children = { new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Children = { tabItems, tabOpen, tabLib } }, itemsCard, openCard, libCard },
        };

        Content = new ScrollViewer
        {
            Content = new StackPanel
            {
                Margin = new Thickness(16, 12), Spacing = 6,
                Children =
                {
                    new TextBlock { Text = "Wheel: " + (string.IsNullOrWhiteSpace(w.Title) ? w.Name : w.Title), FontSize = 24, FontWeight = FontWeight.SemiBold },
                    new TextBlock { Classes = { "desc" }, Text = "A radial menu (max 16 items). Click an item in the picture or the list to change it; where wheels appear is under Settings > Wheel." },
                    WheelBar(),
                    new HowYouPress("wheel"),
                    new WrapPanel { Children = { Cards.Make("Wheel", left), right } },
                },
            },
        };
        RefreshItems();
        RefreshOpeners();
        Edit(null);
        // Del, Ctrl+D, Alt+Left / Right on the chosen item (not while typing in a box)
        AddHandler(KeyDownEvent, (s, e) =>
        {
            if (editing == null || e.Source is TextBox || e.Source is NumericUpDown || e.Source is ComboBox)
                return;
            if (e.Key == Key.Delete) { Delete(); e.Handled = true; }
            else if (e.Key == Key.D && e.KeyModifiers == KeyModifiers.Control) { Duplicate(); e.Handled = true; }
            else if (e.Key == Key.Left && e.KeyModifiers == KeyModifiers.Alt) { Move(-1); e.Handled = true; }
            else if (e.Key == Key.Right && e.KeyModifiers == KeyModifiers.Alt) { Move(1); e.Handled = true; }
        }, Avalonia.Interactivity.RoutingStrategies.Tunnel);
    }

    // The wheel and its sub-wheels as tabs, in the order you go through them.
    Control Family()
    {
        WheelModel ParentOf(WheelModel x) => p.Wheels.Values.FirstOrDefault(o => !ReferenceEquals(o, x) && o.Items.Any(i => i.Action?.Type == ActionType.Wheel && i.Action.Wheel == x.Name));
        var root = w;
        for (int guard = 0; guard < 16 && ParentOf(root) is WheelModel up && !ReferenceEquals(up, w); guard++)
            root = up;
        var family = new List<WheelModel>();
        void Visit(WheelModel x)
        {
            if (x == null || family.Contains(x)) return;
            family.Add(x);
            foreach (var i in x.Items)
                if (i.Action?.Type == ActionType.Wheel && i.Action.Wheel != null)
                    Visit(p.Wheels.GetValueOrDefault(i.Action.Wheel));
        }
        Visit(root);
        var tabs = new WrapPanel { MaxWidth = 900 };
        foreach (var x in family)
        {
            if (tabs.Children.Count > 0)
                tabs.Children.Add(new TextBlock { Text = "→", FontSize = 18, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, 0, 8, 8), Foreground = Brush.Parse("#AAB2BF") });
            var target = x;
            var t = Btn((string.IsNullOrWhiteSpace(x.Title) ? x.Name : x.Title) + (ReferenceEquals(x, root) ? "  (main)" : ""),
                        () => { if (!ReferenceEquals(target, w)) Shell.Go("wheel:" + target.Name); });
            t.Margin = new Thickness(0, 0, 8, 8);
            if (ReferenceEquals(x, w))
                t.Classes.Add("current");
            tabs.Children.Add(t);
        }
        return tabs;
    }

    // ---- editing
    void Rename(TextBox box)
    {
        string n = (box.Text ?? "").Trim();
        if (n.Length == 0 || n == w.Name) { box.Text = w.Name; return; }
        if (p.Wheels.ContainsKey(n)) { Shell.SetStatus("A wheel called " + n + " already exists.", true); box.Text = w.Name; return; }
        string old = w.Name;
        // same as the WinForms RenameWheel: re-key it, and point every 'open a wheel' at the new name
        p.Wheels.Remove(old);
        w.Name = n;
        p.Wheels[n] = w;
        foreach (var b in p.Bindings)
            if (b.Action?.Type == ActionType.Wheel && b.Action.Wheel == old)
                b.Action.Wheel = n;
        foreach (var ww in p.Wheels.Values.Concat(p.WristLeft.Pages.Values).Concat(p.WristRight.Pages.Values))
            foreach (var i in ww.Items)
                if (i.Action?.Type == ActionType.Wheel && i.Action.Wheel == old)
                    i.Action.Wheel = n;
        Shell.Store.MarkDirty();
        Shell.SetStatus($"Renamed '{old}' to '{n}' (and everything that opens it).");
    }

    void Edit(WheelItemModel it)
    {
        editing = it;
        picture.Highlight = it;
        RefreshItems();
        editBar.IsVisible = it != null;
        if (it == null)
        {
            editHost.Content = new TextBlock { Text = "Click an item in the picture or the list to change it here.", Classes = { "desc" }, Margin = new Thickness(4, 8) };
            return;
        }
        var panel = new ItemPanel(w, it);
        panel.Changed += () => { picture.InvalidateVisual(); RefreshItems(); Shell.Store.MarkDirty(); };
        editHost.Content = panel;
    }

    void AddItem()
    {
        if (w.Items.Count >= 16) { Shell.SetStatus("A wheel holds at most 16 items. Make a sub-wheel for more.", true); return; }
        var it = new WheelItemModel { Label = "New item", Action = new ActionModel { Type = ActionType.Key, Keys = "F1" } };
        w.Items.Add(it);
        Structure();
        Edit(it);
    }

    void AddSubWheel()
    {
        if (w.Items.Count >= 16) { Shell.SetStatus("This wheel is full (16 items): remove one first.", true); return; }
        string n = w.Name + " sub-wheel";
        for (int k = 2; p.Wheels.ContainsKey(n); k++) n = w.Name + " sub-wheel " + k;
        var sub = new WheelModel { Name = n, Title = "More" };
        sub.Items.Add(new WheelItemModel { Label = "Back", Action = new ActionModel { Type = ActionType.Back } });
        p.Wheels[n] = sub;
        w.Items.Add(new WheelItemModel { Label = "More", Action = new ActionModel { Type = ActionType.Wheel, Wheel = n } });
        Shell.Store.MarkDirty();
        Shell.Go("wheel:" + n);
    }

    void Duplicate()
    {
        if (editing == null) return;
        if (w.Items.Count >= 16) { Shell.SetStatus("A wheel holds at most 16 items.", true); return; }
        var c = ItemLists.Clone(editing);
        w.Items.Insert(w.Items.IndexOf(editing) + 1, c);
        Structure();
        Edit(c);
    }

    void Delete()
    {
        if (editing == null) return;
        w.Items.Remove(editing);
        Structure();
        Edit(null);
    }

    void Move(int by) => MoveItem(editing, by);

    void MoveItem(WheelItemModel it, int by)
    {
        if (it == null) return;
        int i = w.Items.IndexOf(it), j = i + by;
        if (i < 0 || j < 0 || j >= w.Items.Count) return;
        w.Items.RemoveAt(i);
        w.Items.Insert(j, it);
        Structure();
    }

    void Structure()
    {
        Shell.Store.MarkDirty();
        picture.InvalidateVisual();
        RefreshItems();
    }

    // ---- the lists
    void RefreshItems()
    {
        itemRows.Children.Clear();
        foreach (var it in ItemSort.Sorted(w.Items))
            itemRows.Children.Add(ItemRow(it));
        if (w.Items.Count == 0)
            itemRows.Children.Add(Note("No items yet: + Add item."));
    }

    Control ItemRow(WheelItemModel it)
    {
        bool sel = ReferenceEquals(it, editing);
        var up = Glyph("↑", () => MoveItem(it, -1), "Move up");
        var down = Glyph("↓", () => MoveItem(it, 1), "Move down");
        var onOff = Glyph(it.Enabled ? "On" : "Off", () => { it.Enabled = !it.Enabled; Structure(); }, it.Enabled ? "On: click to switch it off (kept here)" : "Off: click to switch it on");
        onOff.Classes.Add("onoff");
        onOff.Classes.Set("on", it.Enabled);
        var grid = new Grid { ColumnDefinitions = new ColumnDefinitions("26,30,30,*,230,44") };
        var label = new TextBlock { Text = string.IsNullOrWhiteSpace(it.Label) ? "(no label)" : it.Label, Classes = { "rowmain" } };
        var does = new TextBlock { Text = EditorApi.Plain(it.Action), Classes = { "rowsub" } };
        Grid.SetColumn(up, 1); Grid.SetColumn(down, 2); Grid.SetColumn(label, 3); Grid.SetColumn(does, 4); Grid.SetColumn(onOff, 5);
        grid.Children.Add(up); grid.Children.Add(down); grid.Children.Add(label); grid.Children.Add(does); grid.Children.Add(onOff);
        var row = new Border { Classes = { "row" }, Child = grid, Cursor = new Cursor(StandardCursorType.Hand), Tag = it };
        grid.Children.Add(RowDrag.Handle(itemRows, row, it, () => w.Items, Structure)); // ≡ in column 0
        row.Classes.Set("selected", sel);
        row.Classes.Set("off", !it.Enabled);
        row.PointerReleased += (s, e) => { if (e.Source is not Button && (e.Source as Control)?.Parent is not Button) Edit(it); };
        return row;
    }

    static string GestureWord(string g) => g switch
    {
        GestureKind.DoubleTap => "Double-tap", GestureKind.Hold => "Hold", GestureKind.WhileHeld => "Hold (while held)",
        GestureKind.Tap => "Quick tap", GestureKind.DoubleTapHold => "Double-tap and hold", _ => "Press",
    };

    void RefreshOpeners()
    {
        openerRows.Children.Clear();
        foreach (var b in p.Bindings.Where(b => b.Action?.Type == ActionType.Wheel && b.Action.Wheel == w.Name).ToList())
            openerRows.Children.Add(OpenerRow(b));
        foreach (var o in p.Wheels.Values)
            foreach (var i in o.Items.Where(i => i.Action?.Type == ActionType.Wheel && i.Action.Wheel == w.Name))
                openerRows.Children.Add(Note($"Also: the '{i.Label}' item on wheel '{o.Title ?? o.Name}'" + (i.Enabled ? "" : " (off)")));
        for (int h = 0; h < 2; h++)
            foreach (var o in p.WristFor(h).Pages.Values)
                foreach (var i in o.Items.Where(i => i.Action?.Type == ActionType.Wheel && i.Action.Wheel == w.Name))
                    openerRows.Children.Add(Note($"Also: the '{i.Label}' button on the {(h == 0 ? "left" : "right")} wrist panel" + (i.Enabled ? "" : " (off)")));
        if (openerRows.Children.Count == 0)
            openerRows.Children.Add(Note("Nothing opens it yet."));
    }

    Control OpenerRow(BindingModel b)
    {
        var grid = new Grid { ColumnDefinitions = new ColumnDefinitions("130,*,44,44") };
        var how = new TextBlock { Text = GestureWord(b.Gesture), Classes = { "rowsub" }, HorizontalAlignment = HorizontalAlignment.Center };
        var what = new TextBlock { Text = InputText.Friendly(b.Input), Classes = { "rowmain" } };
        var del = Glyph("✕", async () =>
        {
            if (!await Dialogs.Confirm(this, "Delete binding", $"Delete '{b.Name}'?", "Delete")) return;
            p.Bindings.Remove(b);
            Shell.Store.MarkDirty();
            RefreshOpeners();
        }, "Delete this binding");
        del.Classes.Add("del");
        var onOff = Glyph(b.Enabled ? "On" : "Off", () => { b.Enabled = !b.Enabled; Shell.Store.MarkDirty(); RefreshOpeners(); }, b.Enabled ? "On: click to switch it off" : "Off: click to switch it on");
        onOff.Classes.Add("onoff");
        onOff.Classes.Set("on", b.Enabled);
        Grid.SetColumn(what, 1); Grid.SetColumn(del, 2); Grid.SetColumn(onOff, 3);
        grid.Children.Add(how); grid.Children.Add(what); grid.Children.Add(del); grid.Children.Add(onOff);
        var row = new Border { Classes = { "row" }, Child = grid, Cursor = new Cursor(StandardCursorType.Hand) };
        row.Classes.Set("off", !b.Enabled);
        ToolTip.SetTip(row, "Click for all of this binding's settings");
        row.PointerReleased += (s, e) => { if (e.Source is not Button && (e.Source as Control)?.Parent is not Button) Shell.Go("binding:" + p.Bindings.IndexOf(b)); };
        return row;
    }

    static readonly string[] OpenerGestures = { GestureKind.DoubleTap, GestureKind.Hold, GestureKind.Press, GestureKind.DoubleTapHold };

    // Your own opener: any controller button, pressed, held or double-tapped (WinForms AddOpener + Opener).
    async void AddOpener()
    {
        var layout = ControllerLayouts.ById(EditorApi.PictureChoice(p));
        var choices = new List<Choice>();
        foreach (int h in new[] { 0, 1 })
            foreach (var part in layout.PartsFor(h).Where(x => x.IsUsable))
                foreach (var way in part.Ways)
                    choices.Add(new Choice(ControllerLayouts.InputOf(h, part, way), QuickBindings.NameOf(layout, h, part, way)));
        var button = new ChoiceCombo(choices, 380) { SelectedIndex = 0 };
        var gesture = new ChoiceCombo(OpenerGestures.Select(g => new Choice(g, GestureWord(g))), 380) { SelectedIndex = 0 };
        var box = new StackPanel { Spacing = 4, Children = { new TextBlock { Text = "Button" }, button, new TextBlock { Text = "How", Margin = new Thickness(0, 8, 0, 0) }, gesture } };
        if (!await Dialogs.Ask(this, "Open '" + (w.Title ?? w.Name) + "'",
                "Which controller button opens this wheel, and how? Double-tap or hold leaves a normal press for the game.", box))
            return;
        string input = button.Code, g = gesture.Code;
        bool touch = input.EndsWith(".touch") || input.Contains(".thumbrest");
        var made = new BindingModel
        {
            Name = $"{GestureWord(g)} {InputText.Friendly(input)}: wheel '{w.Title ?? w.Name}'",
            Input = input, Gesture = g,
            Block = touch || g != GestureKind.Press ? "none" : "trigger",
            Action = new ActionModel { Type = ActionType.Wheel, Wheel = w.Name },
        };
        if (g == GestureKind.Hold && !touch) { made.TapPassthrough = true; made.Block = "trigger"; }
        p.Bindings.Add(made);
        Shell.Store.MarkDirty();
        RefreshOpeners();
    }

    // ---- bits
    static TextBlock Note(string text) => new() { Text = text, Classes = { "desc" }, MaxWidth = 560 };

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
        if (text is "On" or "Off") { b.Classes.Add("onoff"); b.Classes.Set("on", text == "On"); }
        ToolTip.SetTip(b, tip);
        b.Click += (s, e) => click();
        return b;
    }
}
