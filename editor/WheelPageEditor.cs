namespace OpenXRCompanionEditor;

// One radial wheel on one page: the wheel picture (click to edit an item, drag one onto another to swap them) with the
// item's settings beside it, and on the right either the list of its items (on/off, drag to reorder) or the buttons
// that open it (on/off, delete, + add your own). Replaces the list on the left for wheels.
public class WheelPageEditor : EditorBase
{
    readonly ProfileModel p;
    readonly WheelModel w;
    readonly WheelPreview preview = new() { Size = new Size(320, 320), Margin = new Padding(0, 0, 16, 0) };
    readonly Panel editHost = new() { Size = new Size(560, 640), Margin = new Padding(0, 4, 0, 0) };
    readonly FlowLayoutPanel editBar = new() { AutoSize = true, WrapContents = false, Margin = Padding.Empty, Visible = false };
    readonly FlowLayoutPanel itemRows = new() { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Margin = Padding.Empty };
    readonly FlowLayoutPanel openerRows = new() { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Margin = Padding.Empty };
    WheelItemModel editing, dragged;
    internal Action EditFirstItem, ShowOpeners, ShowLibrary; // screenshots

    public WheelPageEditor(ProfileModel profile, WheelModel wheel, Action<string, string> renamed)
    {
        p = profile;
        w = wheel;
        BackColor = Theme.Page;
        preview.Wheel = w;
        preview.HandleCreated += (o, e) => preview.BackColor = Theme.Card;
        preview.ItemClicked += Edit;
        preview.ItemDropped += (a, b) =>
        {
            int i = w.Items.IndexOf(a), j = w.Items.IndexOf(b);
            if (i < 0 || j < 0) return;
            (w.Items[i], w.Items[j]) = (w.Items[j], w.Items[i]);
            Structure();
            Edit(a);
        };

        // ---- left: name, title, + Add item, the picture and the chosen item's settings
        var names = new FormPanel();
        var name = new TextBox { Width = 220, Text = w.Name };
        var title = new TextBox { Width = 220, Text = w.Title };
        names.AddRow("Name", name, "Used by 'Open a wheel' actions. Renaming updates them.");
        names.AddRow("Title", title, "Shown in the middle of the wheel.");
        name.Leave += (s, e) =>
        {
            string n = name.Text.Trim();
            if (n.Length == 0 || n == w.Name) { name.Text = w.Name; return; }
            if (p.Wheels.ContainsKey(n)) { MessageBox.Show(FindForm(), "A wheel called " + n + " already exists."); name.Text = w.Name; return; }
            string old = w.Name;
            w.Name = n;
            renamed(old, n);
            Fire();
        };
        title.TextChanged += (s, e) => { w.Title = title.Text; preview.Invalidate(); Fire(); };
        var add = Btn("+ Add item", () =>
        {
            if (w.Items.Count >= 16) { MessageBox.Show(FindForm(), "A wheel holds at most 16 items. Make a sub-wheel for more."); return; }
            var it = new WheelItemModel { Label = "New item", Action = new ActionModel { Type = ActionType.Key, Keys = "F1" } };
            w.Items.Add(it);
            Structure();
            Edit(it);
        }, Color.FromArgb(40, 110, 200));
        editBar.Controls.Add(Btn("◀ Move", () => Move(-1)));
        editBar.Controls.Add(Btn("Move ▶", () => Move(1)));
        editBar.Controls.Add(Btn("Delete item", () =>
        {
            if (editing == null) return;
            w.Items.Remove(editing);
            Structure();
            Edit(null);
        }, Color.FromArgb(170, 50, 50)));
        var editCol = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Margin = Padding.Empty };
        editCol.Controls.Add(editBar);
        editCol.Controls.Add(editHost);
        var pic = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = Padding.Empty };
        pic.Controls.Add(preview);
        pic.Controls.Add(editCol);
        var leftBody = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Margin = Padding.Empty };
        leftBody.Controls.Add(names);
        leftBody.Controls.Add(add);
        leftBody.Controls.Add(pic);
        var leftCard = Card("Wheel", "", leftBody);

        // ---- right: items list | opened by
        var itemsBody = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Margin = Padding.Empty };
        itemsBody.Controls.Add(Note("Click one to change it. Drag ≡ to reorder (or drag an item in the picture onto another to swap them). The circle switches it on or off: off = not on the wheel, kept here."));
        itemsBody.Controls.Add(ItemLists.SortBar(RefreshItems));
        itemsBody.Controls.Add(itemRows);
        var itemsCard = Card("Items on this wheel", "", itemsBody);
        var openBody = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Margin = Padding.Empty };
        openBody.Controls.Add(Note("The controller buttons that open this wheel. The circle switches one on or off; ✕ deletes it. Click one for all its settings."));
        openBody.Controls.Add(openerRows);
        openBody.Controls.Add(Btn("+ Add a button that opens it", AddOpener, Color.FromArgb(40, 110, 200)));
        var openCard = Card("Opened by", "", openBody);

        var libCard = Card("All buttons and bindings", "\uE721", ItemLists.Library(p, () => w, it => { Structure(); Edit(it); }, "this wheel"));
        var tabItems = Tab("\u2630  Items on this wheel");
        var tabOpen = Tab("\u25CE  Opened by");
        var tabLib = Tab("\uD83D\uDD0D  All buttons");
        void ShowRight(int which)
        {
            itemsCard.Visible = which == 0;
            openCard.Visible = which == 1;
            libCard.Visible = which == 2;
            foreach (var (b, on) in new[] { (tabItems, which == 0), (tabOpen, which == 1), (tabLib, which == 2) })
            { b.BackColor = on ? Color.FromArgb(40, 110, 200) : Theme.Button; b.ForeColor = on ? Color.White : Theme.Text; }
        }
        tabItems.Click += (s, e) => ShowRight(0);
        tabOpen.Click += (s, e) => ShowRight(1);
        tabLib.Click += (s, e) => ShowRight(2);
        ShowOpeners = () => ShowRight(1);
        ShowLibrary = () => ShowRight(2);
        var switcher = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0, 0, 0, 10) };
        switcher.Controls.Add(tabItems);
        switcher.Controls.Add(tabOpen);
        switcher.Controls.Add(tabLib);
        var column = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Margin = Padding.Empty };
        column.Controls.Add(switcher);
        column.Controls.Add(itemsCard);
        column.Controls.Add(openCard);
        column.Controls.Add(libCard);
        itemsCard.Margin = openCard.Margin = libCard.Margin = new Padding(0, 0, 0, 14);
        ShowRight(0);

        var whole = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, WrapContents = true, Margin = Padding.Empty };
        leftCard.Margin = new Padding(0, 0, 14, 14);
        whole.Controls.Add(leftCard);
        whole.Controls.Add(column);
        Stack(Title("Wheel: " + (string.IsNullOrWhiteSpace(w.Title) ? w.Name : w.Title)),
              FormPanel.Header("A radial menu (max 16 items). Click an item in the picture or the list to change it; where wheels appear is under Settings > Wheel."),
              whole);
        EditFirstItem = () => { if (w.Items.Count > 0) Edit(w.Items[0]); };
        RefreshItems();
        RefreshOpeners();
        Edit(null);
    }

    // ---- items
    void Edit(WheelItemModel it)
    {
        editing = it;
        preview.Highlight = it;
        preview.Invalidate();
        RefreshItems();
        foreach (Control c in editHost.Controls.Cast<Control>().ToList()) c.Dispose();
        editHost.Controls.Clear();
        editBar.Visible = it != null;
        if (it == null)
        {
            editHost.Controls.Add(new Label { Text = "Click an item in the picture or the list to change it here.", AutoSize = true, ForeColor = Theme.SubText, Location = new Point(4, 8) });
            return;
        }
        var ie = new ItemEditor(w, it) { Dock = DockStyle.Fill, Padding = new Padding(0) };
        ie.Preview.Parent.Visible = false; // the picture is right beside it
        ie.Changed += () => { preview.Invalidate(); RefreshItems(); Fire(); };
        if (Theme.IsDark) ThemeWalker.Apply(ie);
        editHost.Controls.Add(ie);
    }

    void Move(int by)
    {
        if (editing == null) return;
        int i = w.Items.IndexOf(editing), j = i + by;
        if (i < 0 || j < 0 || j >= w.Items.Count) return;
        w.Items.RemoveAt(i);
        w.Items.Insert(j, editing);
        Structure();
    }

    void Structure() { Fire(); MainForm.Current?.RebuildList(); preview.Invalidate(); RefreshItems(); }

    void RefreshItems()
    {
        itemRows.SuspendLayout();
        foreach (Control c in itemRows.Controls.Cast<Control>().ToList()) c.Dispose();
        itemRows.Controls.Clear();
        foreach (var it in ItemLists.Sorted(w.Items)) itemRows.Controls.Add(ItemRow(it));
        if (w.Items.Count == 0)
            itemRows.Controls.Add(new Label { Text = "No items yet: + Add item.", AutoSize = true, ForeColor = Theme.SubText });
        itemRows.ResumeLayout();
    }

    Control ItemRow(WheelItemModel it)
    {
        bool sel = ReferenceEquals(it, editing);
        var r = RowPanel(sel);
        var grip = Cell("≡", Theme.SubText, new Font("Segoe UI", 15f), ContentAlignment.MiddleCenter);
        grip.Cursor = Cursors.SizeAll;
        var name = Cell(string.IsNullOrWhiteSpace(it.Label) ? "(no label)" : it.Label, sel ? Color.White : it.Enabled ? Theme.Text : Theme.OffText,
                        new Font("Segoe UI", 10f, it.Enabled ? FontStyle.Bold : FontStyle.Italic), ContentAlignment.MiddleLeft);
        var does = Cell(MainForm.Plain(it.Action), sel ? Color.FromArgb(220, 230, 255) : it.Enabled ? Theme.SubText : Theme.OffText, null, ContentAlignment.MiddleLeft);
        var onOff = Switch(it.Enabled, () => { it.Enabled = !it.Enabled; Structure(); }, "the wheel");
        r.Controls.Add(grip, 0, 0); r.Controls.Add(name, 1, 0); r.Controls.Add(does, 2, 0); r.Controls.Add(onOff, 3, 0);
        foreach (var c in new Control[] { r, name, does })
            c.Click += (o, e) => Edit(it);
        grip.MouseDown += (o, e) => { if (e.Button == MouseButtons.Left) { dragged = it; grip.DoDragDrop(it, DragDropEffects.Move); dragged = null; } };
        void Over(object o, DragEventArgs e) => e.Effect = dragged != null ? DragDropEffects.Move : DragDropEffects.None;
        void Drop(object o, DragEventArgs e)
        {
            var d = dragged; dragged = null;
            if (d == null || ReferenceEquals(d, it)) return;
            int to = w.Items.IndexOf(it);
            w.Items.Remove(d);
            w.Items.Insert(Math.Min(to, w.Items.Count), d);
            BeginInvoke(new Action(() => { Structure(); Edit(d); }));
        }
        foreach (var c in new Control[] { r, grip, name, does, onOff })
        { c.AllowDrop = true; c.DragEnter += Over; c.DragOver += Over; c.DragDrop += Drop; }
        return r;
    }

    // ---- what opens it
    void RefreshOpeners()
    {
        openerRows.SuspendLayout();
        foreach (Control c in openerRows.Controls.Cast<Control>().ToList()) c.Dispose();
        openerRows.Controls.Clear();
        var binds = p.Bindings.Where(b => b.Action?.Type == ActionType.Wheel && b.Action.Wheel == w.Name).ToList();
        foreach (var b in binds) openerRows.Controls.Add(OpenerRow(b));
        // other wheels' items and wrist buttons that open it: shown, edited where they are
        foreach (var o in p.Wheels.Values)
            foreach (var i in o.Items.Where(i => i.Action?.Type == ActionType.Wheel && i.Action.Wheel == w.Name))
                openerRows.Controls.Add(Note($"Also: the '{i.Label}' item on wheel '{o.Title ?? o.Name}'" + (i.Enabled ? "" : " (off)")));
        for (int h = 0; h < 2; h++)
            foreach (var o in p.WristFor(h).Pages.Values)
                foreach (var i in o.Items.Where(i => i.Action?.Type == ActionType.Wheel && i.Action.Wheel == w.Name))
                    openerRows.Controls.Add(Note($"Also: the '{i.Label}' button on the {(h == 0 ? "left" : "right")} wrist panel" + (i.Enabled ? "" : " (off)")));
        if (openerRows.Controls.Count == 0)
            openerRows.Controls.Add(Note("Nothing opens it yet."));
        openerRows.ResumeLayout();
    }

    static string GestureWord(string g) => g switch
    {
        GestureKind.DoubleTap => "Double-tap", GestureKind.Hold => "Hold", GestureKind.WhileHeld => "Hold (while held)",
        GestureKind.Tap => "Quick tap", GestureKind.DoubleTapHold => "Double-tap and hold", _ => "Press",
    };

    Control OpenerRow(BindingModel b)
    {
        var r = RowPanel(false);
        var how = Cell(GestureWord(b.Gesture), b.Enabled ? Theme.SubText : Theme.OffText, null, ContentAlignment.MiddleCenter);
        var what = Cell(InputText.Friendly(b.Input), b.Enabled ? Theme.Text : Theme.OffText, new Font("Segoe UI", 10f, b.Enabled ? FontStyle.Bold : FontStyle.Italic), ContentAlignment.MiddleLeft);
        var del = Cell("✕", Color.FromArgb(230, 90, 90), new Font("Segoe UI", 12f, FontStyle.Bold), ContentAlignment.MiddleCenter);
        del.Cursor = Cursors.Hand;
        FormPanel.Tips.SetToolTip(del, "Delete this binding");
        del.Click += (o, e) =>
        {
            if (MessageBox.Show(FindForm(), $"Delete '{b.Name}'?", "Delete binding", MessageBoxButtons.OKCancel, MessageBoxIcon.Warning) != DialogResult.OK) return;
            p.Bindings.Remove(b);
            Fire(); MainForm.Current?.RebuildList(); RefreshOpeners();
        };
        var onOff = Switch(b.Enabled, () => { b.Enabled = !b.Enabled; Fire(); MainForm.Current?.RebuildList(); RefreshOpeners(); }, "the controller");
        r.ColumnStyles[0].Width = 130;
        r.ColumnStyles[2].Width = 44;
        r.Controls.Add(how, 0, 0); r.Controls.Add(what, 1, 0); r.Controls.Add(del, 2, 0); r.Controls.Add(onOff, 3, 0);
        FormPanel.Tips.SetToolTip(what, "Click for all of this binding's settings");
        foreach (var c in new Control[] { r, how, what })
        { c.Cursor = Cursors.Hand; c.Click += (o, e) => MainForm.Current?.GoTo(b); }
        return r;
    }

    // Your own opener: any controller button, pressed, held or double-tapped.
    void AddOpener()
    {
        var layout = ControllerLayouts.ById(ControllerPictureChoice.Load(p));
        var choices = new List<Choice>();
        foreach (int h in new[] { 0, 1 })
            foreach (var part in layout.PartsFor(h).Where(x => x.IsUsable))
                foreach (var way in part.Ways)
                    choices.Add(new Choice(ControllerLayouts.InputOf(h, part, way), QuickBindingDialogs.NameOf(layout, h, part, way)));
        var gestures = new[] { GestureKind.DoubleTap, GestureKind.Hold, GestureKind.Press, GestureKind.DoubleTapHold }
                       .Select(g => new Choice(g, GestureWord(g))).ToArray();
        var button = new ChoiceBox(choices, 380);
        var gesture = new ChoiceBox(gestures, 380);
        var box = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Margin = Padding.Empty };
        box.Controls.Add(new Label { Text = "Button", AutoSize = true, Margin = new Padding(0, 0, 0, 2) });
        box.Controls.Add(button);
        box.Controls.Add(new Label { Text = "How", AutoSize = true, Margin = new Padding(0, 10, 0, 2) });
        box.Controls.Add(gesture);
        string answer = QuickBindingDialogs.Ask(FindForm(), "Open '" + (w.Title ?? w.Name) + "'",
            "Which controller button opens this wheel, and how? Double-tap or hold leaves a normal press for the game.", box, () => button.Code + "|" + gesture.Code);
        if (answer == null) return;
        var parts = answer.Split('|');
        string input = parts[0], g = parts[1];
        bool touch = input.EndsWith(".touch") || input.Contains(".thumbrest");
        var made = new BindingModel
        {
            Name = $"{GestureWord(g)} {InputText.Friendly(input)}: wheel '{w.Title ?? w.Name}'",
            Input = input,
            Gesture = g,
            Block = touch || g != GestureKind.Press ? "none" : "trigger",
            Action = new ActionModel { Type = ActionType.Wheel, Wheel = w.Name },
        };
        if (g == GestureKind.Hold && !touch) { made.TapPassthrough = true; made.Block = "trigger"; }
        p.Bindings.Add(made);
        Fire(); MainForm.Current?.RebuildList(); RefreshOpeners();
    }

    // ---- bits
    static TableLayoutPanel RowPanel(bool sel)
    {
        var r = new TableLayoutPanel { ColumnCount = 4, RowCount = 1, Width = 560, Height = 46, Margin = new Padding(0, 0, 0, 4), Cursor = Cursors.Hand,
                                       BackColor = sel ? Color.FromArgb(40, 90, 160) : Theme.Button };
        r.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 34));
        r.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        r.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 230));
        r.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 44));
        r.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        return r;
    }
    static Label Cell(string text, Color color, Font font, ContentAlignment align) => new()
    { Text = text, ForeColor = color, Font = font ?? new Font("Segoe UI", 9.5f), Dock = DockStyle.Fill, TextAlign = align, AutoEllipsis = true, Margin = Padding.Empty };
    static Label Switch(bool on, Action flip, string where)
    {
        var l = Cell(on ? "✔" : "○", on ? Color.FromArgb(60, 190, 110) : Theme.SubText, new Font("Segoe UI", 14f, FontStyle.Bold), ContentAlignment.MiddleCenter);
        l.Cursor = Cursors.Hand;
        FormPanel.Tips.SetToolTip(l, on ? $"On: click to switch it off (kept here)" : $"Off: click to switch it on");
        l.Click += (o, e) => flip();
        return l;
    }
    static Label Note(string text) => new() { Text = text, AutoSize = true, MaximumSize = new Size(560, 0), ForeColor = Theme.SubText, Margin = new Padding(0, 0, 0, 8) };
    static Button Btn(string text, Action click, Color? color = null)
    {
        var b = new Button { Text = text, AutoSize = true, Height = 34, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, Margin = new Padding(0, 0, 8, 8),
                             Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), Tag = "swatch",
                             BackColor = color ?? Theme.Button, ForeColor = color != null ? Color.White : Theme.Text };
        b.FlatAppearance.BorderColor = Theme.CardBorder;
        b.FlatAppearance.BorderSize = color != null ? 0 : 1;
        b.Click += (o, e) => click();
        return b;
    }
    static Button Tab(string text)
    {
        var b = new Button { Text = text, AutoSize = true, Height = 38, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, Margin = new Padding(0, 0, 8, 0), Padding = new Padding(8, 0, 8, 0),
                             Font = new Font("Segoe UI", 10.5f, FontStyle.Bold), Tag = "swatch" };
        b.FlatAppearance.BorderSize = 0;
        return b;
    }
    static Card Card(string title, string glyph, Control body) =>
        new(title, glyph, "Saved with profile", Theme.Saved, "Saved with this profile: changes take effect in the game when you press Save (Ctrl+S).", body) { Dock = DockStyle.None };

    // Content a comfortable width, centred (wide: the item list sits beside the wheel).
    protected override void OnResize(EventArgs e)
    {
        base.OnResize(e);
        int side = Math.Max(12, (ClientSize.Width - 1800) / 2);
        if (Padding.Left != side)
            Padding = new Padding(side, 8, side, 8);
    }
}
