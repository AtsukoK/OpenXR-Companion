using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;

namespace OverviewPrototype;

// Tutorials as a page of its own in the rail (was a popup menu). Every tour from Tours.All as a card, grouped like the
// old menu; Start runs it (the coach card frames each step). "Show me this page" on a page still starts that page's tour.
public class TutorialsPage : UserControl
{
    public TutorialsPage()
    {
        var body = new StackPanel { Margin = new Thickness(16, 12), Spacing = 6 };
        body.Children.Add(new TextBlock { Text = "Tutorials", FontSize = 24, FontWeight = FontWeight.SemiBold });
        body.Children.Add(new TextBlock { Classes = { "desc" }, Text = "Guided tours in this window: an orange frame shows where to click. Stop any time; at the end you can keep or throw away what you made." });
        foreach (var g in Tours.All.GroupBy(t => t.Group))
        {
            body.Children.Add(new TextBlock
            {
                Text = g.Key.ToUpperInvariant(), FontWeight = FontWeight.Bold, FontSize = 12, Foreground = Brush.Parse("#7FA8F0"),
                Margin = new Thickness(0, 12, 0, 4),
            });
            var row = new WrapPanel();
            foreach (var t in g)
                row.Children.Add(Card(t, first: t.Id == "main"));
            body.Children.Add(row);
        }
        Content = new ScrollViewer { Content = body };
    }

    static Control Card(TourInfo t, bool first)
    {
        var start = new Button
        {
            Content = "▶  Start", Classes = { "go" }, FontWeight = FontWeight.SemiBold,
            HorizontalAlignment = HorizontalAlignment.Left, Margin = new Thickness(0, 6, 0, 0),
        };
        if (first) { start.Background = Brush.Parse("#F08C1E"); start.Foreground = Brushes.White; }
        start.Click += (s, e) => (TopLevel.GetTopLevel(start) as MainWindow)?.StartTutorial(t.Id);
        var text = new StackPanel
        {
            Spacing = 4,
            Children =
            {
                new TextBlock { Text = t.Title, FontSize = 16, FontWeight = FontWeight.SemiBold, TextWrapping = TextWrapping.Wrap },
                new TextBlock { Text = t.Does, Foreground = Brush.Parse("#C8D0DC"), TextWrapping = TextWrapping.Wrap },
            },
        };
        if (!string.IsNullOrEmpty(t.Example))
            text.Children.Add(new TextBlock { Text = "Good for " + t.Example, Foreground = Brush.Parse("#8A93A0"), FontStyle = FontStyle.Italic, TextWrapping = TextWrapping.Wrap });
        text.Children.Add(start);
        return new Border
        {
            Classes = { "card" }, Width = 330, MinHeight = 150, Margin = new Thickness(0, 0, 12, 12),
            BorderBrush = Brush.Parse(first ? "#F08C1E" : "#3A4250"), BorderThickness = new Thickness(first ? 2 : 1),
            Child = text,
        };
    }
}
