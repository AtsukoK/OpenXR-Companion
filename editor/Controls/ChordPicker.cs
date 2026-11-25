using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Which button(s): one row per button, "Hold X and Y, then Z" for a combo. Same as the WinForms ChordEditor:
// combo buttons go first, the button that fires stays last. Value is "left.grip+right.a".
public class ChordPicker : StackPanel
{
    public event Action Changed;
    readonly Button add = new() { Content = "+ add another button (makes a combo)", Classes = { "link" } };
    readonly List<(StackPanel row, TextBlock lead, ChoiceCombo hand, ChoiceCombo button, Button x)> rows = new();
    bool loading;

    public ChordPicker()
    {
        Spacing = 4;
        add.Click += (s, e) => { AddRow("left", "grip", true); Fire(); };
        Children.Add(add);
    }

    void Fire()
    {
        Relabel();
        if (!loading)
            Changed?.Invoke();
    }

    void AddRow(string hand, string button, bool atFront)
    {
        var lead = new TextBlock { Width = 44, VerticalAlignment = VerticalAlignment.Center };
        var h = new ChoiceCombo(Choices.Hands, 130) { Code = hand };
        var b = new ChoiceCombo(Choices.Buttons, 340) { Code = button };
        var x = new Button { Content = "✕" };
        var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6, Children = { lead, h, b, x } };
        var entry = (row, lead, h, b, x);
        h.SelectionChanged += (s, e) => Fire();
        b.SelectionChanged += (s, e) => Fire();
        x.Click += (s, e) =>
        {
            if (rows.Count < 2)
                return;
            rows.Remove(entry);
            Children.Remove(row);
            Fire();
        };
        if (atFront)
        { rows.Insert(0, entry); Children.Insert(0, row); }
        else
        { rows.Add(entry); Children.Insert(Children.Count - 1, row); }
    }

    void Relabel()
    {
        for (int i = 0; i < rows.Count; i++)
        {
            rows[i].lead.Text = rows.Count == 1 ? "" : i == rows.Count - 1 ? "then" : i == 0 ? "Hold" : "and";
            rows[i].x.IsVisible = rows.Count > 1;
        }
    }

    public string Value
    {
        get => string.Join("+", rows.Select(r => InputText.Join(r.hand.Code, r.button.Code)));
        set
        {
            loading = true;
            foreach (var r in rows)
                Children.Remove(r.row);
            rows.Clear();
            var parts = (value ?? "").Split('+', StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length == 0)
                parts = new[] { "right.trigger" };
            foreach (var p in parts)
            {
                var (h, b) = InputText.Split(p);
                AddRow(h, b, false);
            }
            Relabel();
            loading = false;
        }
    }
}
