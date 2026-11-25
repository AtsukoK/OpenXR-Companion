using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// One wheel item or wrist button: on/off, label, description (wheels), icon, colour, and what it does.
// Port of the WinForms ItemEditor, without its own picture (the page shows the wheel beside it).
public class ItemPanel : StackPanel
{
    public event Action Changed;
    readonly WheelItemModel it;
    readonly Border swatch = new() { Width = 28, Height = 28, CornerRadius = new CornerRadius(4), BorderBrush = Brush.Parse("#4A5260"), BorderThickness = new Thickness(1) };
    readonly TextBox colour = new() { Width = 140, Watermark = "#RRGGBB" };
    bool loading = true;

    // A few colours to pick with one click (#RRGGBB + E0, the alpha the WinForms colour dialog used).
    static readonly string[] Presets = { "#2E7D32", "#1565C0", "#6A1B9A", "#C62828", "#EF6C00", "#F9A825", "#00838F", "#4E342E", "#37474F" };

    public ItemPanel(WheelModel w, WheelItemModel item, WristModel wrist = null)
    {
        it = item;
        bool isWrist = wrist != null;
        Spacing = 8;
        var on = new CheckBox { Content = isWrist ? "On (untick to hide it from the page)" : "On (untick to hide it from the wheel)", IsChecked = it.Enabled };
        var label = new TextBox { Width = 260, Text = it.Label };
        var desc = new TextBox { Width = 360, Text = it.Description ?? "" };
        var icon = new TextBox { Width = 260, Text = it.Icon ?? "" };
        var browse = new Button { Content = "Browse..." };
        var clear = new Button { Content = "Default" };
        var presets = new WrapPanel { MaxWidth = 360 };
        foreach (var hex in Presets)
        {
            var b = new Button { Width = 26, Height = 26, Margin = new Thickness(0, 0, 4, 4), Background = Brush.Parse(hex), Padding = new Thickness(0) };
            b.Click += (s, e) => SetColour(hex + "E0");
            presets.Children.Add(b);
        }

        var f = new Form { LabelWidth = 120 };
        f.AddRow("", on);
        f.AddRow("Label", label);
        if (!isWrist)
            f.AddRow("Description", desc, "Optional, shown in the middle while this item is pointed at.");
        f.AddRow("Icon", new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6, Children = { icon, browse } }, "Optional PNG.");
        f.AddRow("Colour", new StackPanel
        {
            Spacing = 4,
            Children = { new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6, Children = { ColourButton.Wrap(swatch, () => it.Color, v => colour.Text = v), colour, clear } }, presets },
        });
        var action = new ActionPanel(isWrist ? "wrist" : "wheel", wrist);
        action.Bind(it.Action ??= new ActionModel());

        Children.Add(new TextBlock { Text = isWrist ? "Wrist button" : "Wheel item", FontSize = 18, FontWeight = FontWeight.SemiBold });
        Children.Add(f);
        Children.Add(new TextBlock { Text = isWrist ? "When pressed:" : "When picked:", FontWeight = FontWeight.SemiBold, Margin = new Thickness(0, 6, 0, 0) });
        Children.Add(action);
        colour.Text = it.Color ?? "";
        ShowSwatch();
        loading = false;

        on.IsCheckedChanged += (s, e) => { if (it.Enabled == (on.IsChecked == true)) return; it.Enabled = on.IsChecked == true; Fire(); };
        label.TextChanged += (s, e) => { if ((it.Label ?? "") == (label.Text ?? "")) return; it.Label = label.Text; Fire(); };
        desc.TextChanged += (s, e) => { string v = string.IsNullOrEmpty(desc.Text) ? null : desc.Text; if (it.Description == v) return; it.Description = v; Fire(); };
        icon.TextChanged += (s, e) => { string v = string.IsNullOrEmpty(icon.Text) ? null : icon.Text; if (it.Icon == v) return; it.Icon = v; Fire(); };
        colour.TextChanged += (s, e) =>
        {
            string v = string.IsNullOrWhiteSpace(colour.Text) ? null : colour.Text.Trim();
            if (it.Color == v || (v != null && !WheelView.TryColor(v, out _)))
                return; // unchanged, or not a colour yet (still typing)
            it.Color = v;
            ShowSwatch();
            Fire();
        };
        clear.Click += (s, e) => SetColour(null);
        browse.Click += async (s, e) =>
        {
            var top = TopLevel.GetTopLevel(this);
            if (top == null)
                return;
            var files = await top.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
            {
                Title = "Icon", AllowMultiple = false,
                FileTypeFilter = new[] { new FilePickerFileType("PNG images") { Patterns = new[] { "*.png" } } },
            });
            if (files.Count > 0 && files[0].TryGetLocalPath() is string path)
                icon.Text = path;
        };
        action.Changed += Fire;
    }

    void SetColour(string v)
    {
        it.Color = v;
        loading = true;
        colour.Text = v ?? "";
        loading = false;
        ShowSwatch();
        Fire();
    }

    void ShowSwatch() => swatch.Background = WheelView.TryColor(it.Color, out var c) ? new SolidColorBrush(c) : Brush.Parse("#2C3440");

    void Fire()
    {
        if (!loading)
            Changed?.Invoke();
    }
}
