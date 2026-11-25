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

    public Control AddRow(string label, Control control, string help = null)
    {
        var row = new Grid { ColumnDefinitions = new ColumnDefinitions { new ColumnDefinition(new GridLength(LabelWidth)), new ColumnDefinition(GridLength.Star) } };
        var l = new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center, TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 0, 14, 0) };
        var right = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6 };
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
