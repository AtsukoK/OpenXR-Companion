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
              FormPanel.Header("Every controller binding in this profile. The circle switches one on or off, ⧉ duplicates it, ✕ deletes it; click one for all its settings."),
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
        var off = list.Where(b => !OnPicture(b)).ToList();
        if (off.Count > 0)
        {
            rows.Controls.Add(Heading($"Not on the {layout.Name} picture ({off.Count})",
                "Made for another controller, or an input this controller doesn't have. They still work wherever the input exists."));
            foreach (var b in off) rows.Controls.Add(Row(b, true));
            rows.Controls.Add(Heading("On this controller", null));
        }
        foreach (var b in list.Where(OnPicture)) rows.Controls.Add(Row(b, false));
        if (list.Count == 0)
            rows.Controls.Add(new Label { Text = q.Length == 0 ? "No bindings yet." : "Nothing matches.", AutoSize = true, ForeColor = Theme.SubText });
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
        foreach (var w in new[] { 44f, 250f, 100f, 0f, 40f, 40f })
            r.ColumnStyles.Add(w == 0 ? new ColumnStyle(SizeType.Percent, 100) : new ColumnStyle(SizeType.Absolute, w));
        r.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        Color text = b.Enabled ? Theme.Text : Theme.OffText, sub = b.Enabled ? Theme.SubText : Theme.OffText;
        Label L(string t, Color c, Font f = null, ContentAlignment a = ContentAlignment.MiddleLeft) =>
            new() { Text = t, ForeColor = c, Font = f ?? new Font("Segoe UI", 9.5f), Dock = DockStyle.Fill, TextAlign = a, AutoEllipsis = true, Margin = Padding.Empty };
        var onOff = L(b.Enabled ? "✔" : "○", b.Enabled ? Color.FromArgb(60, 190, 110) : Theme.SubText, new Font("Segoe UI", 14f, FontStyle.Bold), ContentAlignment.MiddleCenter);
        FormPanel.Tips.SetToolTip(onOff, b.Enabled ? "On: click to switch it off (kept here)" : "Off: click to switch it on");
        onOff.Click += (s, e) => { b.Enabled = !b.Enabled; Changed(); };
        var where = L(InputText.Friendly(b.Input), text, new Font("Segoe UI", 10f, b.Enabled ? FontStyle.Bold : FontStyle.Italic));
        var how = L(Gesture(b.Gesture), sub);
        var what = L(b.Name + "  ·  " + MainForm.Plain(b.Action), sub);
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

    static string Gesture(string g) => g switch
    {
        GestureKind.DoubleTap => "Double-tap", GestureKind.Hold => "Hold", GestureKind.WhileHeld => "While held",
        GestureKind.Tap => "Quick tap", GestureKind.DoubleTapHold => "2× hold", _ => "Press",
    };
}
