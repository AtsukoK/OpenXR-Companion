using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// A dropdown of Choices (code + text), like the WinForms ChoiceBox: Code gets / sets by code.
public class ChoiceCombo : ComboBox
{
    protected override Type StyleKeyOverride => typeof(ComboBox);
    Choice[] choices;

    public ChoiceCombo(IEnumerable<Choice> choices, double width = 400)
    {
        Width = width;
        SetChoices(choices);
    }

    public void SetChoices(IEnumerable<Choice> list)
    {
        choices = list.ToArray();
        ItemsSource = choices.Select(c => c.Text).ToList();
    }

    public string Code
    {
        get => SelectedIndex >= 0 && SelectedIndex < choices.Length ? choices[SelectedIndex].Code : null;
        set => SelectedIndex = Array.FindIndex(choices, c => c.Code == value);
    }
}

// Label / control / help rows, like the WinForms FormPanel. A row can be hidden (Show) and then takes no space.
// Rows are separate grids with the same label width, so they line up.
public class Form : StackPanel
{
    // device-independent units, so the same physical width at any Windows scaling
    public double LabelWidth { get; set; } = 240;

    public Form()
    {
        Spacing = 8;
    }

    // desc: a short plain explanation shown under the label (the ⓘ help keeps the longer details).
    public Control AddRow(string label, Control control, string help = null, string desc = null)
    {
        var row = new Grid { ColumnDefinitions = new ColumnDefinitions { new ColumnDefinition(new GridLength(LabelWidth)), new ColumnDefinition(GridLength.Star) } };
        Control l = new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center, TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 0, 14, 0) };
        if (desc != null)
        {
            row.Margin = new Thickness(0, 6, 0, 6);
            l = new StackPanel
            {
                Margin = new Thickness(0, 0, 14, 0), Spacing = 2, VerticalAlignment = VerticalAlignment.Top,
                Children =
                {
                    new TextBlock { Text = label, FontWeight = FontWeight.SemiBold, TextWrapping = TextWrapping.Wrap },
                    new TextBlock { Text = desc, FontSize = 12, Foreground = Brush.Parse("#9AA3B0"), TextWrapping = TextWrapping.Wrap },
                },
            };
        }
        var right = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6 };
        if (desc != null) right.VerticalAlignment = VerticalAlignment.Top;
        right.Children.Add(control);
        if (help != null)
            right.Children.Add(new TextBlock
            {
                Text = "ⓘ", FontSize = 15, Foreground = Brush.Parse("#7FA8F0"), VerticalAlignment = VerticalAlignment.Center,
                [ToolTip.TipProperty] = new TextBlock { Text = help, TextWrapping = TextWrapping.Wrap, MaxWidth = 420 },
            });
        Grid.SetColumn(right, 1);
        row.Children.Add(l);
        row.Children.Add(right);
        Children.Add(row);
        return row;
    }

    public static void Show(Control row, bool on) => row.IsVisible = on;

    // Changes a row's label and its description (rows made with a desc).
    public static void SetLabel(Control row, string label, string desc = null)
    {
        if (row is not Grid g || g.Children.Count == 0) return;
        if (g.Children[0] is TextBlock t) { t.Text = label; return; }
        if (g.Children[0] is StackPanel sp && sp.Children.Count > 1 && sp.Children[0] is TextBlock a && sp.Children[1] is TextBlock d)
        {
            a.Text = label;
            if (desc != null) d.Text = desc;
        }
    }
}

// A card with a heading (1 How, 2 Button, ...).
public static class Cards
{
    public static Border Make(string title, Control body) => new()
    {
        Classes = { "card" },
        Child = new StackPanel
        {
            Spacing = 8,
            Children = { new TextBlock { Text = title, Classes = { "title" } }, body },
        },
    };
}
