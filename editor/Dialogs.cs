using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;

namespace OverviewPrototype;

// Avalonia has no MessageBox: a small one for questions like "Delete 'X'?".
public static class Dialogs
{
    public static async Task<bool> Confirm(Visual owner, string title, string text, string ok = "OK")
    {
        var win = new Window
        {
            Title = title, SizeToContent = SizeToContent.WidthAndHeight, CanResize = false, MaxWidth = 520,
            WindowStartupLocation = WindowStartupLocation.CenterOwner, Background = Brush.Parse("#23272E"),
        };
        bool result = false;
        var okButton = new Button { Content = ok, IsDefault = true, Classes = { "go" }, MinWidth = 90 };
        var cancel = new Button { Content = "Cancel", IsCancel = true, MinWidth = 90, Margin = new Thickness(8, 6, 0, 0) };
        okButton.Click += (s, e) => { result = true; win.Close(); };
        cancel.Click += (s, e) => win.Close();
        win.Content = new StackPanel
        {
            Margin = new Thickness(20), Spacing = 14,
            Children =
            {
                new TextBlock { Text = text, TextWrapping = TextWrapping.Wrap },
                new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right, Children = { okButton, cancel } },
            },
        };
        if (TopLevel.GetTopLevel(owner) is Window parent)
            await win.ShowDialog(parent);
        return result;
    }

    // A question with several answers (buttons left to right; the first is the default). The index picked, -1 when closed.
    public static async Task<int> Choose(Visual owner, string title, string text, params string[] buttons)
    {
        var win = new Window
        {
            Title = title, SizeToContent = SizeToContent.WidthAndHeight, CanResize = false, MaxWidth = 560,
            WindowStartupLocation = WindowStartupLocation.CenterOwner, Background = Brush.Parse("#23272E"),
        };
        int result = -1;
        var row = new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right, Spacing = 8 };
        for (int i = 0; i < buttons.Length; i++)
        {
            int k = i;
            var b = new Button { Content = buttons[i], MinWidth = 90, IsDefault = i == 0, IsCancel = i == buttons.Length - 1 };
            if (i == 0) b.Classes.Add("go");
            b.Click += (s, e) => { result = k; win.Close(); };
            row.Children.Add(b);
        }
        win.Content = new StackPanel
        {
            Margin = new Thickness(20), Spacing = 14,
            Children = { new TextBlock { Text = text, TextWrapping = TextWrapping.Wrap }, row },
        };
        if (TopLevel.GetTopLevel(owner) is Window parent)
            await win.ShowDialog(parent);
        return result;
    }

    // A question with your own controls in it (OK / Cancel). True on OK.
    public static async Task<bool> Ask(Visual owner, string title, string text, Control content)
    {
        var win = new Window
        {
            Title = title, SizeToContent = SizeToContent.WidthAndHeight, CanResize = false, MaxWidth = 560,
            WindowStartupLocation = WindowStartupLocation.CenterOwner, Background = Brush.Parse("#23272E"),
        };
        bool result = false;
        var okButton = new Button { Content = "OK", IsDefault = true, Classes = { "go" }, MinWidth = 90 };
        var cancel = new Button { Content = "Cancel", IsCancel = true, MinWidth = 90, Margin = new Thickness(8, 6, 0, 0) };
        okButton.Click += (s, e) => { result = true; win.Close(); };
        cancel.Click += (s, e) => win.Close();
        win.Content = new StackPanel
        {
            Margin = new Thickness(20), Spacing = 14,
            Children =
            {
                new TextBlock { Text = text, TextWrapping = TextWrapping.Wrap },
                content,
                new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right, Children = { okButton, cancel } },
            },
        };
        if (TopLevel.GetTopLevel(owner) is Window parent)
            await win.ShowDialog(parent);
        return result;
    }
}
