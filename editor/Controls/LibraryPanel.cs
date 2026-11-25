using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Shared by the wheel page and the wrist page (port of the WinForms ItemLists): the sort bar for their lists, and the
// library of every button, item and binding in the profile with "+ Add here" to put a copy on the wheel / page shown.
public static class ItemSort
{
    // 0 = by place, 1 = switched-on ones first (each group still in place order); the WinForms setting, shared
    public static IEnumerable<WheelItemModel> Sorted(IEnumerable<WheelItemModel> items) => ItemLists.Sorted(items);

    public static Control Bar(Action changed)
    {
        var box = new ChoiceCombo(new[] { new Choice("0", "By place (the order you see them in)"), new Choice("1", "On first, then off") }, 300) { Code = ItemLists.Sort.ToString() };
        box.SelectionChanged += (s, e) =>
        {
            int v = box.Code == "1" ? 1 : 0;
            if (v == ItemLists.Sort) return;
            ItemLists.Sort = v; // a view setting, not the profile
            changed();
        };
        return new StackPanel
        {
            Orientation = Orientation.Horizontal, Spacing = 6, Margin = new Thickness(0, 0, 0, 6),
            Children = { new TextBlock { Text = "Sort:", Foreground = Brush.Parse("#AAB2BF"), VerticalAlignment = VerticalAlignment.Center }, box },
        };
    }
}

public class LibraryPanel : StackPanel
{
    record Entry(string Label, string Icon, string Color, string Description, ActionModel Action, string From, bool On);

    readonly ProfileModel p = Shell.Store.Profile;
    readonly Func<WheelModel> target;
    readonly Action<WheelItemModel> added;
    readonly TextBox search = new() { Width = 560, Watermark = "Search: name, key, wheel, button (e.g. F5, map, thumbrest)" };
    readonly StackPanel rows = new() { Spacing = 4 };

    public LibraryPanel(Func<WheelModel> target, Action<WheelItemModel> added, string here)
    {
        this.target = target;
        this.added = added;
        Spacing = 6;
        Width = 580;
        Children.Add(new TextBlock { Classes = { "desc" }, MaxWidth = 560, Text = $"Everything you have made: wheel items, wrist buttons and controller bindings. '+ Add here' puts a copy on {here}, so you don't have to make it again." });
        Children.Add(search);
        Children.Add(new ScrollViewer { MaxHeight = 620, Content = rows });
        search.TextChanged += (s, e) => Fill();
        // pick up what was made since, whenever it is shown again
        PropertyChanged += (s, e) => { if (e.Property == IsVisibleProperty && IsVisible) Fill(); };
        Fill();
    }

    IEnumerable<Entry> AllEntries()
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

    void Fill()
    {
        rows.Children.Clear();
        string q = (search.Text ?? "").Trim();
        var list = AllEntries().Where(e => q.Length == 0 || $"{e.Label} {EditorApi.Plain(e.Action)} {e.From}".Contains(q, StringComparison.OrdinalIgnoreCase)).ToList();
        foreach (var e in list)
        {
            var entry = e;
            var add = new Button { Content = "+ Add here", Classes = { "go" }, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(6, 0) };
            add.Click += (s, ev) =>
            {
                var w = target();
                if (w == null) return;
                if (w.Items.Count >= 16) { Shell.SetStatus("That holds at most 16. Remove one or add another page / sub-wheel.", true); return; }
                var it = new WheelItemModel { Label = entry.Label, Icon = entry.Icon, Color = entry.Color, Description = entry.Description, Action = ItemLists.Clone(entry.Action ?? new ActionModel()) };
                w.Items.Add(it);
                Shell.Store.MarkDirty();
                added(it);
                Fill();
            };
            var text = new StackPanel
            {
                Margin = new Thickness(8, 4),
                Children =
                {
                    new TextBlock { Text = (string.IsNullOrWhiteSpace(e.Label) ? "(no label)" : e.Label) + (e.On ? "" : "  (off)"), FontWeight = FontWeight.SemiBold,
                                    Foreground = Brush.Parse(e.On ? "#E6E9EE" : "#7F8896"), TextTrimming = TextTrimming.CharacterEllipsis },
                    new TextBlock { Text = EditorApi.Plain(e.Action) + "  ·  " + e.From, Foreground = Brush.Parse("#AAB2BF"), TextTrimming = TextTrimming.CharacterEllipsis },
                },
            };
            var grid = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto") };
            Grid.SetColumn(add, 1);
            grid.Children.Add(text);
            grid.Children.Add(add);
            rows.Children.Add(new Border { Classes = { "row" }, Child = grid });
        }
        if (list.Count == 0)
            rows.Children.Add(new TextBlock { Classes = { "desc" }, Text = q.Length == 0 ? "Nothing made yet." : "Nothing matches." });
    }
}
