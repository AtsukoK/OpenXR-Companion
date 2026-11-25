using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;

namespace OverviewPrototype;

// Overview: "Set up your menu positions at the start of the next game?" The switch writes setup_positions.json
// { "pending": true } next to the profiles (every game, not one profile); the layer reads it a few seconds after a game
// starts, walks you through placing everything in the headset (guide card, A = next, B = skip), saves, and sets it to
// false so it runs once. Written only when the switch is changed (and by the first start for a new user).
public class SetupCard : Border
{
    protected override Type StyleKeyOverride => typeof(Border); // styled as a card

    public static string FilePath => Path.Combine(ProfileStore.RealDataDir, "setup_positions.json");

    public static bool Pending
    {
        get
        {
            try { return File.Exists(FilePath) && File.ReadAllText(FilePath).Replace(" ", "").Contains("\"pending\":true"); }
            catch { return false; }
        }
        set
        {
            try { Directory.CreateDirectory(ProfileStore.RealDataDir); File.WriteAllText(FilePath, value ? "{ \"pending\": true }" : "{ \"pending\": false }"); }
            catch { }
        }
    }

    // Done at least once (the layer wrote pending false), or never asked.
    public static bool EverRun => File.Exists(FilePath) && !Pending;

    public SetupCard()
    {
        Classes.Add("card");
        BorderBrush = Brush.Parse("#F08C1E");
        BorderThickness = new Thickness(2);
        Margin = new Thickness(0, 0, 0, 12);
        HorizontalAlignment = HorizontalAlignment.Stretch;
        var sw = new ToggleSwitch { IsChecked = Pending, OnContent = "On: at the start of the next game", OffContent = "Off", FontWeight = FontWeight.SemiBold };
        var state = new TextBlock { TextWrapping = TextWrapping.Wrap, Foreground = Brush.Parse("#AAB2BF") };
        void Show() => state.Text = sw.IsChecked == true
            ? "Next time you start a VR game, a card in the headset walks you through it: grab each thing with a grip, let go, press A for the next. B skips one. It runs once, then switches itself off."
            : EverRun ? "Done before. Switch it on to go through it again at the start of the next game (or use 'Calibrate Menu Positions' on your wrist panel any time)."
                      : "Switch it on and the next game starts with a short walk-through in the headset.";
        sw.IsCheckedChanged += (s, e) =>
        {
            bool v = sw.IsChecked == true;
            if (v != Pending) Pending = v; // only a real change writes
            Show();
            Shell.SetStatus(v ? "Setup of positions: it starts a few seconds into your next game." : "Setup of positions switched off.");
        };
        Show();
        // the layer switches it off when the setup is done in the headset: follow that while the card is on screen
        var poll = new Avalonia.Threading.DispatcherTimer { Interval = TimeSpan.FromSeconds(2) };
        poll.Tick += (s, e) => { bool p = Pending; if ((sw.IsChecked == true) != p) { sw.IsChecked = p; Show(); } };
        AttachedToVisualTree += (s, e) => poll.Start();
        DetachedFromVisualTree += (s, e) => poll.Stop();
        Child = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*"),
            Children =
            {
                new TextBlock { Text = "", FontFamily = "Segoe MDL2 Assets", FontSize = 34, Foreground = Brush.Parse("#F08C1E"), Margin = new Thickness(4, 2, 16, 0), VerticalAlignment = VerticalAlignment.Top },
                new StackPanel
                {
                    [Grid.ColumnProperty] = 1, Spacing = 8,
                    Children =
                    {
                        new TextBlock { Text = "Set up your menu positions at the start of the next game?", FontSize = 18, FontWeight = FontWeight.SemiBold, TextWrapping = TextWrapping.Wrap },
                        new TextBlock { TextWrapping = TextWrapping.Wrap, Foreground = Brush.Parse("#DCE4F0"),
                            Text = "Everyone's arms and play space are different. Place your wrist panels, wheel, screen mirror, VR keyboard and fingertips once, by hand, and they'll be where you expect them in every game." },
                        sw,
                        state,
                    },
                },
            },
        };
    }
}
