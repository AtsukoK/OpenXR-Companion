using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// "What should this button do?": big cards in groups (Keys, Open, Set up, ...). Returns the picked index, or -1.
public static class OptionPicker
{
    public static async Task<int> Show(Visual owner, string title, string sub, IReadOnlyList<PickOption> options)
    {
        int picked = -1;
        var win = new Window
        {
            Title = title, Width = 980, Height = 720, WindowStartupLocation = WindowStartupLocation.CenterOwner,
            Background = Brush.Parse("#1B1E23"),
        };
        var body = new StackPanel { Margin = new Thickness(20), Spacing = 6 };
        body.Children.Add(new TextBlock { Text = title, FontSize = 22, FontWeight = FontWeight.SemiBold });
        body.Children.Add(new TextBlock { Text = sub, Classes = { "desc" } });
        foreach (var group in options.Select((o, i) => (o, i)).GroupBy(x => x.o.Group))
        {
            body.Children.Add(new TextBlock { Text = group.Key.ToUpperInvariant(), FontWeight = FontWeight.Bold, FontSize = 12,
                                              Foreground = Brush.Parse("#7FA8F0"), Margin = new Thickness(0, 10, 0, 2) });
            var wrap = new WrapPanel();
            foreach (var (o, i) in group)
            {
                var card = new Button
                {
                    Classes = { "option" }, Width = 290, MinHeight = 96, Margin = new Thickness(0, 0, 10, 10),
                    HorizontalContentAlignment = HorizontalAlignment.Left, VerticalContentAlignment = VerticalAlignment.Top,
                    Content = new StackPanel
                    {
                        Spacing = 4,
                        Children =
                        {
                            new TextBlock { Text = o.Title, FontWeight = FontWeight.SemiBold, FontSize = 15, TextWrapping = TextWrapping.Wrap },
                            new TextBlock { Text = o.Does, TextWrapping = TextWrapping.Wrap, Foreground = Brush.Parse("#C8CDD6") },
                            new TextBlock { Text = o.Example == null ? null : "For example: " + o.Example, IsVisible = o.Example != null,
                                            TextWrapping = TextWrapping.Wrap, Foreground = Brush.Parse("#8F98A6"), FontStyle = FontStyle.Italic },
                        },
                    },
                };
                int index = i;
                card.Click += (s, e) => { picked = index; win.Close(); };
                wrap.Children.Add(card);
            }
            body.Children.Add(wrap);
        }
        win.Content = new ScrollViewer { Content = body };
        if (TopLevel.GetTopLevel(owner) is Window parent)
            await win.ShowDialog(parent);
        return picked;
    }
}
