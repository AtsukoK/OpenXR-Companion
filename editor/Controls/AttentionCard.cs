using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// On the Overview: "N bindings may not do what you expect", one line each, a click opens the binding.
// Hidden when nothing needs a look.
public class AttentionCard : UserControl
{
    public AttentionCard()
    {
        AttachedToVisualTree += (s, e) => Fill();
    }

    void Fill()
    {
        var issues = Attention.Issues();
        IsVisible = issues.Count > 0;
        if (issues.Count == 0) { Content = null; return; }
        var byBinding = issues.GroupBy(i => i.Binding).ToList();
        var list = new StackPanel { Spacing = 4 };
        list.Children.Add(new TextBlock
        {
            Text = byBinding.Count == 1 ? "⚠  1 binding may not do what you expect" : $"⚠  {byBinding.Count} bindings may not do what you expect",
            FontWeight = FontWeight.Bold, FontSize = 15, Foreground = Brush.Parse("#F2C94C"),
        });
        list.Children.Add(new TextBlock
        {
            Text = "Click one to see why and fix it, or mark it as 'Looks right'.", Foreground = Brush.Parse("#C8CDD6"), Margin = new Thickness(0, 0, 0, 4),
        });
        foreach (var g in byBinding.Take(12))
        {
            var b = g.Key;
            var go = new Button
            {
                Classes = { "link" }, HorizontalAlignment = HorizontalAlignment.Left,
                Content = $"{b.Name}  ·  " + string.Join(", ", g.Select(i => i.Title).Distinct()),
            };
            go.Click += (s, e) => Shell.Go("binding:" + Shell.Store.Profile.Bindings.IndexOf(b));
            list.Children.Add(go);
        }
        if (byBinding.Count > 12)
            list.Children.Add(new TextBlock { Text = $"…and {byBinding.Count - 12} more (All bindings shows them all).", Foreground = Brush.Parse("#C8CDD6") });
        Content = new Border
        {
            Background = Brush.Parse("#3A3220"), BorderBrush = Attention.GlowBorder(), BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(8), Padding = new Thickness(14, 10), Margin = new Thickness(4, 0, 4, 10), Child = list,
        };
    }
}
