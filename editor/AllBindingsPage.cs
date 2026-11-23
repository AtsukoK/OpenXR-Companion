namespace OpenXRCompanionEditor;

// Every controller binding in one list (the rail's Bindings icon, under Controllers): search, sort, switch on/off,
// duplicate, delete, click for all its settings. Bindings on no button of the chosen controller's picture (made for
// another controller, or with an input the picture doesn't have) are listed first, so none can get lost.
public class AllBindingsPage : EditorBase
{
    readonly ProfileModel p;
    readonly FlowLayoutPanel rows = new() { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Margin = Padding.Empty };
    readonly TextBox search = new() { Width = 420 };
    static int sort; // 0 = by hand and button, 1 = on first, 2 = name
    static string show = "all"; // all, controller, wheels, wrist

    public void FocusSearch() { search.Focus(); }

    public AllBindingsPage(ProfileModel profile)
    {
        p = profile;
        BackColor = Theme.Page;
        var bar = new FlowLayoutPanel { AutoSize = true, WrapContents = true, Dock = DockStyle.Top, Margin = Padding.Empty, Padding = new Padding(0, 4, 0, 10) };
        bar.Controls.Add(new Label { Text = "Search:", AutoSize = true, Margin = new Padding(0, 6, 6, 0) });
        bar.Controls.Add(search);
        bar.Controls.Add(new Label { Text = "Sort:", AutoSize = true, Margin = new Padding(18, 6, 6, 0) });
        var sortBox = new ChoiceBox(new[] { new Choice("0", "By hand and button"), new Choice("1", "On first, then off"), new Choice("2", "By name") }, 220) { Code = sort.ToString() };
        bar.Controls.Add(sortBox);
        bar.Controls.Add(new Label { Text = "Show:", AutoSize = true, Margin = new Padding(18, 6, 6, 0) });
        var showBox = new ChoiceBox(new[] { new Choice("all", "Everything"), new Choice("controller", "Controller bindings"), new Choice("wheels", "Wheel items"), new Choice("wrist", "Wrist buttons") }, 200) { Code = show };
        showBox.SelectedIndexChanged += (s, e) => { show = showBox.Code ?? "all"; Fill(); };
        bar.Controls.Add(showBox);
        var add = new Button { Text = "+ Add binding", AutoSize = true, Height = 32, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, Margin = new Padding(18, 0, 0, 0),
                               BackColor = Color.FromArgb(40, 110, 200), ForeColor = Color.White, Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), Tag = "swatch" };
        add.FlatAppearance.BorderSize = 0;
        FormPanel.Tips.SetToolTip(add, "A blank binding, opened in the binding editor. For the quick way, click a button on the Controllers screen.");
        add.Click += (s, e) =>
        {
            var b = new BindingModel { Name = "New binding", Input = "right.primary", Gesture = GestureKind.Press, Action = new ActionModel { Type = ActionType.Key, Keys = "F1" } };
            p.Bindings.Add(b);
            Fire();
            MainForm.Current?.RebuildList();
            MainForm.Current?.GoTo(b);
        };
        bar.Controls.Add(add);
        search.TextChanged += (s, e) => Fill();
        sortBox.SelectedIndexChanged += (s, e) => { sort = int.TryParse(sortBox.Code, out var v) ? v : 0; Fill(); };
        var list = new Panel { Dock = DockStyle.Top, AutoSize = true };
        list.Controls.Add(rows);
        Stack(Title("All bindings"),
              FormPanel.Header("Everything in this profile: controller bindings, wheel items and wrist buttons. The circle switches one on or off, ⧉ duplicates it, ✕ deletes it; click one for all its settings."),
              bar, list);
        Fill();
    }

    void Changed() { Fire(); MainForm.Current?.RebuildList(); Fill(); }

    void Fill()
    {
        var layout = ControllerLayouts.ById(ControllerPictureChoice.Load(p));
        var inputs = new List<string>();
        foreach (int h in new[] { 0, 1 })
            foreach (var part in layout.PartsFor(h).Where(x => x.IsUsable))
                foreach (var way in part.Ways)
                    inputs.Add(ControllerLayouts.InputOf(h, part, way));
        bool OnPicture(BindingModel b) => inputs.Any(i => QuickBindings.FiredBy(b, i));
        string q = search.Text.Trim();
        var all = p.Bindings.Where(b => q.Length == 0 || $"{b.Name} {InputText.Friendly(b.Input)} {MainForm.Plain(b.Action)}".Contains(q, StringComparison.OrdinalIgnoreCase));
        all = sort switch
        {
            1 => all.OrderBy(b => b.Enabled ? 0 : 1),
            2 => all.OrderBy(b => b.Name, StringComparer.OrdinalIgnoreCase),
            _ => all.OrderBy(b => b.Input ?? "", StringComparer.OrdinalIgnoreCase),
        };
        var list = all.ToList();
        rows.SuspendLayout();
        foreach (Control c in rows.Controls.Cast<Control>().ToList()) c.Dispose();
        rows.Controls.Clear();
        if (show is not ("all" or "controller")) list.Clear();
        else if (show == "all" && list.Count > 0) rows.Controls.Add(Heading("What your controller buttons do", null));
        var off = list.Where(b => !OnPicture(b)).ToList();
        if (off.Count > 0)
        {
            rows.Controls.Add(Heading($"Not on the {layout.Name} picture ({off.Count})",
                "Made for another controller, or an input this controller doesn't have. They still work wherever the input exists."));
            foreach (var b in off) rows.Controls.Add(Row(b, true));
            rows.Controls.Add(Heading("What your controller buttons do", null));
        }
        foreach (var b in list.Where(OnPicture)) rows.Controls.Add(Row(b, false));
        int shown = list.Count;
        if (q.Length > 1 && show == "all") // settings pages with that word: on top, they open with it lit
        {
            var hits = SettingsPage.All.Where(sp => SettingsEditor.WordsOf(p, sp.Key).Contains(q, StringComparison.OrdinalIgnoreCase)).ToList();
            if (hits.Count > 0)
            {
                var head = Heading("Settings", null);
                rows.Controls.Add(head);
                rows.Controls.SetChildIndex(head, 0);
                int at = 1;
                foreach (var sp in hits)
                {
                    var b = new Button { Text = sp.Title + "   ·   " + sp.Sub, Width = 1100, Height = 40, TextAlign = ContentAlignment.MiddleLeft, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand,
                                         Margin = new Padding(0, 0, 0, 4), BackColor = Theme.Button, ForeColor = Theme.Text, Font = new Font("Segoe UI", 10f, FontStyle.Bold), Tag = "swatch", Padding = new Padding(10, 0, 0, 0) };
                    b.FlatAppearance.BorderSize = 0;
                    var page = sp;
                    b.Click += (s, e) => MainForm.Current?.GoTo(page);
                    rows.Controls.Add(b);
                    rows.Controls.SetChildIndex(b, at++);
                    shown++;
                }
            }
        }
        bool Match(WheelItemModel i, string from) => q.Length == 0 || $"{i.Label} {MainForm.Plain(i.Action)} {from}".Contains(q, StringComparison.OrdinalIgnoreCase);
        IEnumerable<WheelItemModel> Order(IEnumerable<WheelItemModel> items) => sort switch
        {
            1 => items.OrderBy(i => i.Enabled ? 0 : 1),
            2 => items.OrderBy(i => i.Label, StringComparer.OrdinalIgnoreCase),
            _ => items,
        };
        if (show is "all" or "wheels")
            foreach (var w in p.Wheels.Values)
            {
                string from = $"Wheel '{w.Title ?? w.Name}'";
                var items = Order(w.Items.Where(i => Match(i, from))).ToList();
                if (items.Count == 0) continue;
                rows.Controls.Add(Heading(from, null));
                var target = w;
                foreach (var i in items) rows.Controls.Add(ItemRow(i, target.Items, () => MainForm.Current?.GoTo(target)));
                shown += items.Count;
            }
        if (show is "all" or "wrist")
            for (int h = 0; h < 2; h++)
                foreach (var w in p.WristFor(h).Pages.Values)
                {
                    string from = $"{(h == 0 ? "Left" : "Right")} wrist panel: {w.Title ?? w.Name}";
                    var items = Order(w.Items.Where(i => Match(i, from))).ToList();
                    if (items.Count == 0) continue;
                    rows.Controls.Add(Heading(from, null));
                    string page = h == 0 ? "wrist0" : "wrist1";
                    var target = w;
                    foreach (var i in items) rows.Controls.Add(ItemRow(i, target.Items, () => MainForm.Current?.GoTo(page)));
                    shown += items.Count;
                }
        if (shown == 0)
            rows.Controls.Add(new Label { Text = q.Length == 0 ? "Nothing here yet." : "Nothing matches.", AutoSize = true, ForeColor = Theme.SubText });
        rows.ResumeLayout();
    }

    static Control Heading(string text, string note)
    {
        var box = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, WrapContents = false, Margin = new Padding(0, 10, 0, 6) };
        box.Controls.Add(new Label { Text = text.ToUpperInvariant(), AutoSize = true, ForeColor = Theme.Heading, Font = new Font("Segoe UI", 9f, FontStyle.Bold) });
        if (note != null)
            box.Controls.Add(new Label { Text = note, AutoSize = true, ForeColor = Theme.SubText, MaximumSize = new Size(900, 0) });
        return box;
    }

    Control Row(BindingModel b, bool lost)
    {
        var r = new TableLayoutPanel { ColumnCount = 6, RowCount = 1, Width = 1100, Height = 44, Margin = new Padding(0, 0, 0, 4), Cursor = Cursors.Hand,
                                       BackColor = lost ? Color.FromArgb(70, 60, 40) : Theme.Button };
        foreach (var w in new[] { 44f, 400f, 36f, 0f, 40f, 40f })
            r.ColumnStyles.Add(w == 0 ? new ColumnStyle(SizeType.Percent, 100) : new ColumnStyle(SizeType.Absolute, w));
        r.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        Color text = b.Enabled ? Theme.Text : Theme.OffText, sub = b.Enabled ? Theme.SubText : Theme.OffText;
        Label L(string t, Color c, Font f = null, ContentAlignment a = ContentAlignment.MiddleLeft) =>
            new() { Text = t, ForeColor = c, Font = f ?? new Font("Segoe UI", 9.5f), Dock = DockStyle.Fill, TextAlign = a, AutoEllipsis = true, Margin = Padding.Empty };
        var onOff = L(b.Enabled ? "✔" : "○", b.Enabled ? Color.FromArgb(60, 190, 110) : Theme.SubText, new Font("Segoe UI", 14f, FontStyle.Bold), ContentAlignment.MiddleCenter);
        FormPanel.Tips.SetToolTip(onOff, b.Enabled ? "On: click to switch it off (kept here)" : "Off: click to switch it on");
        onOff.Click += (s, e) => { b.Enabled = !b.Enabled; Changed(); };
        var where = L((b.Gesture == GestureKind.WhileHeld ? "Hold" : Gesture(b.Gesture)) + " " + InputText.Friendly(b.Input) + (b.Enabled ? "" : "   (off)"), text, new Font("Segoe UI", 10f, b.Enabled ? FontStyle.Bold : FontStyle.Italic));
        var how = L("→", sub, new Font("Segoe UI", 12f, FontStyle.Bold), ContentAlignment.MiddleCenter);
        var what = L(MainForm.Plain(b.Action), text);
        foreach (var c in new Control[] { where, what }) FormPanel.Tips.SetToolTip(c, b.Name + "  (click for all its settings)");
        var dup = L("⧉", Theme.Text, new Font("Segoe UI", 13f), ContentAlignment.MiddleCenter);
        FormPanel.Tips.SetToolTip(dup, "Duplicate (a copy right below it, switched off)");
        dup.Click += (s, e) =>
        {
            var c = ItemLists.Clone(b);
            c.Name += " copy";
            c.Enabled = false; // two of the same would both fire
            p.Bindings.Insert(p.Bindings.IndexOf(b) + 1, c);
            Changed();
        };
        var del = L("✕", Color.FromArgb(230, 90, 90), new Font("Segoe UI", 12f, FontStyle.Bold), ContentAlignment.MiddleCenter);
        FormPanel.Tips.SetToolTip(del, "Delete");
        del.Click += (s, e) =>
        {
            if (MessageBox.Show(FindForm(), $"Delete '{b.Name}'?", "Delete binding", MessageBoxButtons.OKCancel, MessageBoxIcon.Warning) != DialogResult.OK) return;
            p.Bindings.Remove(b);
            Changed();
        };
        foreach (var c in new Control[] { onOff, dup, del }) c.Cursor = Cursors.Hand;
        r.Controls.Add(onOff, 0, 0); r.Controls.Add(where, 1, 0); r.Controls.Add(how, 2, 0); r.Controls.Add(what, 3, 0); r.Controls.Add(dup, 4, 0); r.Controls.Add(del, 5, 0);
        foreach (var c in new Control[] { r, where, how, what })
            c.Click += (s, e) => MainForm.Current?.GoTo(b);
        return r;
    }

    // A wheel item or wrist button: on/off, duplicate, delete; click opens its wheel / wrist page.
    Control ItemRow(WheelItemModel it, List<WheelItemModel> owner, Action open)
    {
        var r = new TableLayoutPanel { ColumnCount = 5, RowCount = 1, Width = 1100, Height = 44, Margin = new Padding(0, 0, 0, 4), Cursor = Cursors.Hand, BackColor = Theme.Button };
        foreach (var w in new[] { 44f, 350f, 0f, 40f, 40f })
            r.ColumnStyles.Add(w == 0 ? new ColumnStyle(SizeType.Percent, 100) : new ColumnStyle(SizeType.Absolute, w));
        r.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        Label L(string t, Color c, Font f = null, ContentAlignment a = ContentAlignment.MiddleLeft) =>
            new() { Text = t, ForeColor = c, Font = f ?? new Font("Segoe UI", 9.5f), Dock = DockStyle.Fill, TextAlign = a, AutoEllipsis = true, Margin = Padding.Empty };
        var onOff = L(it.Enabled ? "✔" : "○", it.Enabled ? Color.FromArgb(60, 190, 110) : Theme.SubText, new Font("Segoe UI", 14f, FontStyle.Bold), ContentAlignment.MiddleCenter);
        FormPanel.Tips.SetToolTip(onOff, it.Enabled ? "On: click to switch it off (kept here)" : "Off: click to switch it on");
        onOff.Click += (s, e) => { it.Enabled = !it.Enabled; Changed(); };
        var name = L(string.IsNullOrWhiteSpace(it.Label) ? "(no label)" : it.Label, it.Enabled ? Theme.Text : Theme.OffText, new Font("Segoe UI", 10f, it.Enabled ? FontStyle.Bold : FontStyle.Italic));
        var what = L(MainForm.Plain(it.Action), it.Enabled ? Theme.SubText : Theme.OffText);
        var dup = L("⧉", Theme.Text, new Font("Segoe UI", 13f), ContentAlignment.MiddleCenter);
        FormPanel.Tips.SetToolTip(dup, "Duplicate (a copy right after it)");
        dup.Click += (s, e) =>
        {
            if (owner.Count >= 16) { MessageBox.Show(FindForm(), "That holds at most 16."); return; }
            owner.Insert(owner.IndexOf(it) + 1, ItemLists.Clone(it));
            Changed();
        };
        var del = L("✕", Color.FromArgb(230, 90, 90), new Font("Segoe UI", 12f, FontStyle.Bold), ContentAlignment.MiddleCenter);
        del.Click += (s, e) =>
        {
            if (MessageBox.Show(FindForm(), $"Delete '{it.Label}'?", "Delete", MessageBoxButtons.OKCancel, MessageBoxIcon.Warning) != DialogResult.OK) return;
            owner.Remove(it);
            Changed();
        };
        foreach (var c in new Control[] { onOff, dup, del }) c.Cursor = Cursors.Hand;
        r.Controls.Add(onOff, 0, 0); r.Controls.Add(name, 1, 0); r.Controls.Add(what, 2, 0); r.Controls.Add(dup, 3, 0); r.Controls.Add(del, 4, 0);
        foreach (var c in new Control[] { r, name, what })
            c.Click += (s, e) => open();
        return r;
    }

    static string Gesture(string g) => g switch
    {
        GestureKind.DoubleTap => "Double-tap", GestureKind.Hold => "Hold", GestureKind.WhileHeld => "While held",
        GestureKind.Tap => "Quick tap", GestureKind.DoubleTapHold => "2× hold", _ => "Press",
    };
}
