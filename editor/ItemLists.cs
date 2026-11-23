using System.Text.Json;

namespace OpenXRCompanionEditor;

// Shared by the wrist page and the wheel page: how their button / item lists are sorted, and the library of every
// button, item and binding in the profile to copy one from (so it needn't be made again).
public static class ItemLists
{
    // 0 = where they sit on the panel / wheel, 1 = switched-on ones first (each group still in place order)
    public static int Sort;

    public static IEnumerable<WheelItemModel> Sorted(IEnumerable<WheelItemModel> items) =>
        Sort == 1 ? items.OrderBy(i => i.Enabled ? 0 : 1) : items;

    public static Control SortBar(Action changed)
    {
        var row = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0, 0, 0, 8) };
        row.Controls.Add(new Label { Text = "Sort:", AutoSize = true, ForeColor = Theme.SubText, Margin = new Padding(0, 6, 6, 0) });
        var box = new ChoiceBox(new[] { new Choice("0", "By place (the order you see them in)"), new Choice("1", "On first, then off") }, 300) { Code = Sort.ToString() };
        box.SelectedIndexChanged += (s, e) => { Sort = box.Code == "1" ? 1 : 0; changed(); };
        row.Controls.Add(box);
        return row;
    }

    record Entry(string Label, string Icon, string Color, string Description, ActionModel Action, string From, bool On);

    static IEnumerable<Entry> AllEntries(ProfileModel p)
    {
        foreach (var w in p.Wheels.Values)
            foreach (var i in w.Items)
                yield return new(i.Label, i.Icon, i.Color, i.Description, i.Action, $"Wheel '{w.Title ?? w.Name}'", i.Enabled);
        for (int h = 0; h < 2; h++)
            foreach (var w in p.WristFor(h).Pages.Values)
                foreach (var i in w.Items)
                    yield return new(i.Label, i.Icon, i.Color, i.Description, i.Action, $"{(h == 0 ? "Left" : "Right")} wrist · {w.Title ?? w.Name}", i.Enabled);
        foreach (var b in p.Bindings.Where(b => b.Action != null))
            yield return new(b.Name, null, null, null, b.Action, "Controller: " + InputText.Friendly(b.Input), b.Enabled);
    }

    static readonly JsonSerializerOptions Opts = new() { PropertyNameCaseInsensitive = true };
    public static T Clone<T>(T o) => JsonSerializer.Deserialize<T>(JsonSerializer.Serialize(o, Opts), Opts);
    static ActionModel Copy(ActionModel a) => a == null ? new ActionModel() : JsonSerializer.Deserialize<ActionModel>(JsonSerializer.Serialize(a, Opts), Opts);

    // Every button, item and controller binding, with a search box; "+ Add here" puts a copy on the shown page / wheel.
    public static Control Library(ProfileModel p, Func<WheelModel> target, Action<WheelItemModel> added, string here)
    {
        var box = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Margin = Padding.Empty };
        box.Controls.Add(new Label { Text = $"Everything you have made: wheel items, wrist buttons and controller bindings. '+ Add here' puts a copy on {here}, so you don't have to make it again.", AutoSize = true, MaximumSize = new Size(560, 0), ForeColor = Theme.SubText, Margin = new Padding(0, 0, 0, 8) });
        var search = new TextBox { Width = 560, Margin = new Padding(0, 0, 0, 8), PlaceholderText = "\U0001F50D  Search: name, key, wheel, button (e.g. F5, map, thumbrest)" };
        box.Controls.Add(new Label { Text = "Search (name, key, wheel or button, e.g. F5, map, thumbrest):", AutoSize = true, ForeColor = Theme.Text, Margin = new Padding(0, 0, 0, 3) });
        box.Controls.Add(search);
        var rows = new Panel { Width = 580, Height = 620, AutoScroll = true, Margin = Padding.Empty };
        var inner = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Location = Point.Empty };
        rows.Controls.Add(inner);
        box.Controls.Add(rows);
        void Fill()
        {
            inner.SuspendLayout();
            foreach (Control c in inner.Controls.Cast<Control>().ToList()) c.Dispose();
            inner.Controls.Clear();
            string q = search.Text.Trim();
            var list = AllEntries(p).Where(e => q.Length == 0 || $"{e.Label} {MainForm.Plain(e.Action)} {e.From}".Contains(q, StringComparison.OrdinalIgnoreCase)).ToList();
            foreach (var e in list)
            {
                var r = new TableLayoutPanel { ColumnCount = 2, RowCount = 2, Width = 556, Height = 52, Margin = new Padding(0, 0, 0, 4), BackColor = Theme.Button };
                r.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
                r.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 120));
                r.RowStyles.Add(new RowStyle(SizeType.Percent, 50));
                r.RowStyles.Add(new RowStyle(SizeType.Percent, 50));
                r.Controls.Add(new Label { Text = (string.IsNullOrWhiteSpace(e.Label) ? "(no label)" : e.Label) + (e.On ? "" : "  (off)"), Dock = DockStyle.Fill, AutoEllipsis = true, Margin = new Padding(8, 0, 0, 0),
                                           TextAlign = ContentAlignment.BottomLeft, Font = new Font("Segoe UI", 10f, FontStyle.Bold), ForeColor = e.On ? Theme.Text : Theme.OffText }, 0, 0);
                r.Controls.Add(new Label { Text = MainForm.Plain(e.Action) + "  ·  " + e.From, Dock = DockStyle.Fill, AutoEllipsis = true, Margin = new Padding(8, 0, 0, 0),
                                           TextAlign = ContentAlignment.TopLeft, ForeColor = Theme.SubText }, 0, 1);
                var add = new Button { Text = "+ Add here", Dock = DockStyle.Fill, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, Margin = new Padding(6, 10, 6, 10), BackColor = Color.FromArgb(40, 110, 200), ForeColor = Color.White, Tag = "swatch" };
                add.FlatAppearance.BorderSize = 0;
                var entry = e;
                add.Click += (s, ev) =>
                {
                    var w = target();
                    if (w == null) return;
                    if (w.Items.Count >= 16) { MessageBox.Show(box.FindForm(), "That holds at most 16. Remove one or add another page / sub-wheel."); return; }
                    var it = new WheelItemModel { Label = entry.Label, Icon = entry.Icon, Color = entry.Color, Description = entry.Description, Action = Copy(entry.Action) };
                    w.Items.Add(it);
                    added(it);
                    Fill();
                };
                r.Controls.Add(add, 1, 0);
                r.SetRowSpan(add, 2);
                inner.Controls.Add(r);
            }
            if (list.Count == 0)
                inner.Controls.Add(new Label { Text = q.Length == 0 ? "Nothing made yet." : "Nothing matches.", AutoSize = true, ForeColor = Theme.SubText });
            inner.ResumeLayout();
        }
        search.TextChanged += (s, e) => Fill();
        Fill();
        box.VisibleChanged += (s, e) => { if (box.Visible) Fill(); }; // pick up what was made since
        return box;
    }
}
