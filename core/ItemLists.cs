namespace OpenXRCompanionEditor;

// How wheel / wrist lists are sorted, and copying things (the data half of the editors' item lists).
public static class ItemLists
{
    // 0 = where they sit on the panel / wheel, 1 = switched-on ones first (each group still in place order)
    public static int Sort;

    public static IEnumerable<WheelItemModel> Sorted(IEnumerable<WheelItemModel> items) =>
        Sort == 1 ? items.OrderBy(i => i.Enabled ? 0 : 1) : items;

    static readonly System.Text.Json.JsonSerializerOptions Opts = new() { PropertyNameCaseInsensitive = true };
    public static T Clone<T>(T o) => System.Text.Json.JsonSerializer.Deserialize<T>(System.Text.Json.JsonSerializer.Serialize(o, Opts), Opts);
    public static ActionModel Copy(ActionModel a) => a == null ? new ActionModel() : System.Text.Json.JsonSerializer.Deserialize<ActionModel>(System.Text.Json.JsonSerializer.Serialize(a, Opts), Opts);
}
