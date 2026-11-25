using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;

namespace OverviewPrototype;

public partial class OverviewPage : UserControl
{
    public static readonly StyledProperty<int> ColumnsProperty = AvaloniaProperty.Register<OverviewPage, int>(nameof(Columns), 2);
    public int Columns { get => GetValue(ColumnsProperty); set => SetValue(ColumnsProperty, value); }

    public List<Card> Cards { get; }
    public string ProfileName { get; }

    public OverviewPage()
    {
        var store = Shell.Store;
        ProfileName = "Profile: " + Path.GetFileNameWithoutExtension(store.CurrentName);
        Cards = Overview.Build(store.Profile);
        AvaloniaXamlLoader.Load(this);
        DataContext = this;
        // device-independent units: the same physical width at any Windows scaling
        SizeChanged += (s, e) => Columns = e.NewSize.Width >= 900 ? 2 : 1;
    }

    void Go_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button { Tag: string target })
            Shell.Go(target);
    }
}
