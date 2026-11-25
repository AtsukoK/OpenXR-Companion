using Avalonia.Controls;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Pick a mode (a layer in the file) from the ones the profile has, or "+ New mode…" to name a new one.
// emptyText: the first choice for "no mode" ("Always", "Never off"); null = a mode must be picked.
// A value naming several modes ("a, b", from a hand-edited file) is kept as its own choice.
public class ModePicker : ComboBox
{
    protected override Type StyleKeyOverride => typeof(ComboBox);
    public event Action Changed;
    const string NewMode = "+ New mode…";
    readonly string emptyText;
    bool filling;
    string value = "";

    public ModePicker(string emptyText, double width = 380)
    {
        this.emptyText = emptyText;
        Width = width;
        SelectionChanged += async (s, e) =>
        {
            if (filling || SelectedItem is not string pick) return;
            if (pick == NewMode)
            {
                var name = new TextBox { Width = 300, Watermark = "e.g. combat, looting, driving" };
                string made = await Dialogs.Ask(this, "New mode", "What's the mode called?", name) ? (name.Text ?? "").Trim().ToLowerInvariant().Replace(",", "") : "";
                if (made.Length > 0)
                {
                    var p = Shell.Store.Profile;
                    p.Modes ??= new();
                    if (!p.Modes.ContainsKey(made)) p.Modes[made] = new ModeModel();
                    Shell.Store.MarkDirty();
                    Value = made;
                    Changed?.Invoke();
                }
                else
                    Value = value; // cancelled: back to what it was
                return;
            }
            value = pick == emptyText ? "" : pick;
            Changed?.Invoke();
        };
    }

    public string Value
    {
        get => value;
        set
        {
            this.value = (value ?? "").Trim();
            var p = Shell.Store.Profile;
            var names = EditorApi.LayerNames(p).Concat(p.Modes?.Keys ?? Enumerable.Empty<string>())
                                 .Distinct(StringComparer.OrdinalIgnoreCase).OrderBy(x => x).ToList();
            if (this.value.Length > 0 && !names.Contains(this.value, StringComparer.OrdinalIgnoreCase))
                names.Add(this.value);
            var items = new List<string>();
            if (emptyText != null) items.Add(emptyText);
            items.AddRange(names);
            items.Add(NewMode);
            filling = true;
            ItemsSource = items;
            SelectedItem = this.value.Length == 0 ? emptyText : items.FirstOrDefault(i => string.Equals(i, this.value, StringComparison.OrdinalIgnoreCase));
            filling = false;
        }
    }
}
