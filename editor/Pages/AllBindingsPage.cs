using OpenXRCompanionEditor;

namespace OverviewPrototype;

// The rows of the All bindings list: headings, controller bindings, wheel items and wrist buttons.
// One flat list, so the list control only creates the rows that are on screen (virtualized).
public abstract class ListRow : Notify { }

public class HeadingRow : ListRow
{
    public string Text { get; init; }
    public string Note { get; init; }
    public bool HasNote => !string.IsNullOrEmpty(Note);
    public string Upper => Text?.ToUpperInvariant();
}

public class BindingRow : ListRow
{
    public BindingModel Binding { get; init; }
    public bool Lost { get; init; } // not on the chosen controller's picture
    public bool IsOn => Binding.Enabled;
    public string OnGlyph => Binding.Enabled ? "On" : "Off";
    public string Where => (Binding.Gesture == GestureKind.WhileHeld ? "Hold" : AllBindings.GestureWord(Binding.Gesture)) + " " +
                           InputText.Friendly(Binding.Input) + (Binding.Enabled ? "" : "   (off)");
    public string What => EditorApi.Plain(Binding.Action);
    public string Tip => Binding.Name + "  (click for all its settings)";
    // square tags for the layer rules: "SHIFT  LAYER ONLY", "OFF IN SHIFT"
    public List<string> LayerOnly => Split(Binding.Layers).Select(l => l.ToUpperInvariant() + "  MODE ONLY").ToList();
    public List<string> LayerOff => Split(Binding.NotLayers).Select(l => "OFF IN " + l.ToUpperInvariant()).ToList();
    public bool HasLayerTags => LayerOnly.Count + LayerOff.Count > 0;
    static IEnumerable<string> Split(string s) => (s ?? "").Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
    public string AttentionText { get; init; } // "This may need your attention": what the checks found, or null
    public bool HasAttention => !string.IsNullOrEmpty(AttentionText);
}

// A settings page with the searched word on it (opens it with the word lit up).
public class SettingsHitRow : ListRow
{
    public string Key { get; init; }
    public string Text { get; init; }
}

public class ItemRow : ListRow
{
    public WheelItemModel Item { get; init; }
    public List<WheelItemModel> Owner { get; init; }
    public string Open { get; init; } // page key its row opens
    public bool IsOn => Item.Enabled;
    public string OnGlyph => Item.Enabled ? "On" : "Off";
    public string Label => string.IsNullOrWhiteSpace(Item.Label) ? "(no label)" : Item.Label;
    public string What => EditorApi.Plain(Item.Action);
}

// What the WinForms AllBindingsPage.Fill does, without any UI: the rows for a search / sort / show choice.
public static class AllBindings
{
    public static string GestureWord(string g) => g switch
    {
        GestureKind.DoubleTap => "Double-tap", GestureKind.Hold => "Hold", GestureKind.WhileHeld => "While held",
        GestureKind.Tap => "Quick tap", GestureKind.DoubleTapHold => "2× hold", _ => "Press",
    };

    // Show: Layers. Per layer: what switches it on, the bindings that only work in it, and the ones it switches off.
    static List<ListRow> LayerRows(ProfileModel p, List<BindingModel> shown, Func<BindingModel, string> att)
    {
        var rows = new List<ListRow>();
        static IEnumerable<string> Names(string s) => (s ?? "").Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        bool Is(string a, string b) => string.Equals(a, b, StringComparison.OrdinalIgnoreCase);
        foreach (var layer in EditorApi.LayerNames(p))
        {
            bool Turns(ActionModel a) => a?.Type == ActionType.Layer && Is(a.Layer, layer);
            var by = new List<string>();
            foreach (var b in p.Bindings.Where(b => Turns(b.Action) || Turns(b.TapAction)))
                by.Add($"{(b.Gesture == GestureKind.WhileHeld ? "Hold" : GestureWord(b.Gesture))} {InputText.Friendly(b.Input)}{(b.Enabled ? "" : " (off)")}");
            foreach (var w in p.Wheels.Values)
                by.AddRange(w.Items.Where(i => Turns(i.Action)).Select(i => $"wheel '{w.Title ?? w.Name}': {i.Label}"));
            for (int h = 0; h < 2; h++)
                foreach (var w in p.WristFor(h).Pages.Values)
                    by.AddRange(w.Items.Where(i => Turns(i.Action)).Select(i => $"{(h == 0 ? "left" : "right")} wrist: {i.Label}"));
            var only = shown.Where(b => Names(b.Layers).Any(l => Is(l, layer))).ToList();
            var off = shown.Where(b => Names(b.NotLayers).Any(l => Is(l, layer))).ToList();
            rows.Add(new HeadingRow
            {
                Text = $"{layer} mode",
                Note = by.Count == 0 ? "⚠ Nothing turns this mode on, so the bindings below never work."
                                     : "Switched on by: " + string.Join(";  ", by),
            });
            if (only.Count == 0 && off.Count == 0)
                rows.Add(new HeadingRow { Text = "", Note = "No bindings only work in this mode." });
            rows.AddRange(only.Select(b => new BindingRow { Binding = b, AttentionText = att(b) }));
            if (off.Count > 0)
            {
                rows.Add(new HeadingRow { Text = "", Note = $"Switched off in {layer} mode:" });
                rows.AddRange(off.Select(b => new BindingRow { Binding = b, AttentionText = att(b) }));
            }
        }
        if (rows.Count == 0)
            rows.Add(new HeadingRow { Text = "No modes yet.", Note = "A mode makes buttons do something else while it is on (previously called layers). The Modes tab makes one." });
        return rows;
    }

    // sort: 0 = by hand and button, 1 = on first, 2 = by name. show: all, controller, wheels, wrist.
    public static List<ListRow> Rows(ProfileModel p, string q, int sort, string show)
    {
        var rows = new List<ListRow>();
        q = (q ?? "").Trim();
        var attention = Attention.Issues().GroupBy(i => i.Binding)
            .ToDictionary(g => g.Key, g => "This may need your attention:\n" + string.Join("\n", g.Select(i => "• " + i.Title)) + "\n(open it for details and fixes)");
        string Att(BindingModel b) => attention.TryGetValue(b, out var t) ? t : null;
        var layout = ControllerLayouts.ById(EditorApi.PictureChoice(p));
        var inputs = new List<string>();
        foreach (int h in new[] { 0, 1 })
            foreach (var part in layout.PartsFor(h).Where(x => x.IsUsable))
                foreach (var way in part.Ways)
                    inputs.Add(ControllerLayouts.InputOf(h, part, way));
        bool OnPicture(BindingModel b) => inputs.Any(i => QuickBindings.FiredBy(b, i));

        var all = p.Bindings.Where(b => q.Length == 0 || $"{b.Name} {InputText.Friendly(b.Input)} {EditorApi.Plain(b.Action)} {b.Layers} {b.NotLayers}".Contains(q, StringComparison.OrdinalIgnoreCase));
        all = sort switch
        {
            1 => all.OrderBy(b => b.Enabled ? 0 : 1),
            2 => all.OrderBy(b => b.Name, StringComparer.OrdinalIgnoreCase),
            _ => all.OrderBy(b => b.Input ?? "", StringComparer.OrdinalIgnoreCase),
        };
        if (show == "layers")
            return LayerRows(p, all.ToList(), Att);
        var list = show is "all" or "controller" ? all.ToList() : new List<BindingModel>();
        var off = list.Where(b => !OnPicture(b)).ToList();
        if (off.Count > 0)
        {
            rows.Add(new HeadingRow { Text = $"Not on the {layout.Name} picture ({off.Count})",
                                      Note = "Made for another controller, or an input this controller doesn't have. They still work wherever the input exists." });
            rows.AddRange(off.Select(b => new BindingRow { Binding = b, Lost = true, AttentionText = Att(b) }));
        }
        var on = list.Where(OnPicture).ToList();
        if (on.Count > 0 || (off.Count > 0 && show == "all"))
            rows.Add(new HeadingRow { Text = "What your controller buttons do" });
        rows.AddRange(on.Select(b => new BindingRow { Binding = b, AttentionText = Att(b) }));

        bool Match(WheelItemModel i, string from) => q.Length == 0 || $"{i.Label} {EditorApi.Plain(i.Action)} {from}".Contains(q, StringComparison.OrdinalIgnoreCase);
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
                if (items.Count == 0)
                    continue;
                rows.Add(new HeadingRow { Text = from });
                rows.AddRange(items.Select(i => new ItemRow { Item = i, Owner = w.Items, Open = "wheel:" + w.Name }));
            }
        if (show is "all" or "wrist")
            for (int h = 0; h < 2; h++)
                foreach (var w in p.WristFor(h).Pages.Values)
                {
                    string from = $"{(h == 0 ? "Left" : "Right")} wrist panel: {w.Title ?? w.Name}";
                    var items = Order(w.Items.Where(i => Match(i, from))).ToList();
                    if (items.Count == 0)
                        continue;
                    rows.Add(new HeadingRow { Text = from });
                    rows.AddRange(items.Select(i => new ItemRow { Item = i, Owner = w.Items, Open = "wrist" + h }));
                }
        if (q.Length > 1 && show == "all") // settings pages with that word: on top
        {
            var hits = SettingsPage.All.Where(sp => SettingsPageView.Ported.Contains(sp.Key) && (sp.Key != "experimental" || SettingsPageView.ExperimentalListed) && SettingsPageView.WordsOf(sp.Key).Contains(q, StringComparison.OrdinalIgnoreCase)).ToList();
            if (hits.Count > 0)
                rows.InsertRange(0, new ListRow[] { new HeadingRow { Text = "Settings" } }
                    .Concat(hits.Select(sp => new SettingsHitRow { Key = sp.Key, Text = sp.Title + "   ·   " + sp.Sub })));
        }
        if (rows.Count == 0)
            rows.Add(new HeadingRow { Text = q.Length == 0 ? "Nothing here yet." : "Nothing matches." });
        return rows;
    }
}
