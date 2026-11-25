using Avalonia.Controls;

namespace OverviewPrototype;

// Where the pages live: which page is shown, the Back history, and the profile they all edit.
// Pages are named by a key ("overview", "settings", "bindings", "wheels", "wrist0", "wrist1", "controllers", ...)
// and made fresh each time they are shown, like the WinForms editor's pages.
public static class Shell
{
    public static ProfileStore Store { get; set; }
    public static event Action<string> Navigated; // key of the page now shown
    public static event Action<string, bool> Status; // text, is it an error

    static readonly Stack<string> back = new();
    public static string Current { get; private set; }
    public static bool CanGoBack => back.Count > 0;

    // The pages, as they get ported. Anything else shows a "not ported yet" page.
    static readonly Dictionary<string, (string title, string tip, Func<Control> make)> pages = new()
    {
        ["overview"] = ("Overview", "Tip: the switches on each card turn things on or off right here. The blue buttons jump to where you edit them.",
                        () => new OverviewPage()),
        ["bindings"] = ("All bindings", "Tip: everything in this profile. The circle switches one on or off; click a line for all its settings.",
                        () => new AllBindingsPage()),
        ["controllers"] = ("Controllers", "Tip: click a button on the picture (or its tag) to see what it does; the + on a tag adds something to it.",
                           () => new ControllersPage()),
        ["wrists"] = ("Wrist panels", "Tip: click a panel to edit it. 'Copy to left / right' puts a whole panel from another profile on that arm.",
                      () => new WristsPage()),
        ["wheels"] = ("Wheels", "Tip: click a wheel to open it. On its page, '+ Add sub-wheel' makes a submenu that is linked to it automatically.",
                      () => new WheelsPage()),
    };

    public static readonly (string key, string glyph, string name)[] RailItems =
    {
        ("overview", "", "Overview"),
        ("settings", "", "Settings"),
        ("controllers", "", "Controllers"),
        ("bindings", "", "All bindings"),
        ("wheels", "", "Wheels"),
        ("wrists", "", "Wrist panels"),
        ("tutorials", "", "Tutorials: the full tour, how-tos and a tour of each page"),
    };

    public static string TitleOf(string key) =>
        pages.TryGetValue(key, out var p) ? p.title : RailItems.FirstOrDefault(r => r.key == key).name ?? key;

    public static string TipOf(string key) =>
        pages.TryGetValue(key, out var p) ? p.tip
        : key == "tutorials" ? "Tip: new here? Start with 'The full tour'. Each page also has its own short tour: the orange 'Show me this page' button."
        : key == "settings:experimental" ? "Tip: head smoothing is for every game and profile, and each change applies straight away (no Save needed). Read the warning first."
        : key == "settings:mirror" ? "Tip: the headset mirror is for every game and profile, and each change applies straight away (no Save needed). Hover the ⓘ for help on a setting."
        : key == "settings" || key.StartsWith("settings:") ? "Tip: settings here are saved with the profile and reach the game when you save (Ctrl+S). Hover the ⓘ for help on a setting."
        : key is "wrist0" or "wrist1" ? "Tip: turn your wrist toward your face and look at it to show the panel. Poke a button with your other hand's finger, or point at it and pull the trigger."
        : key.StartsWith("wheel:") ? "Tip: click an item in the wheel or the list to change it. The tabs at the top go to its sub-wheels."
        : key.StartsWith("binding:") ? "Tip: 1 How you press it, 2 the Button, 3 What happens. The Details are set for you; 'Record key' binds a key from your keyboard."
        : null;

    public static Control Make(string key)
    {
        if (pages.TryGetValue(key, out var p))
            return p.make();
        // pages for one thing: binding:<index in the profile's bindings>
        var (kind, arg) = key.Contains(':') ? (key[..key.IndexOf(':')], key[(key.IndexOf(':') + 1)..]) : (key, "");
        var prof = Store.Profile;
        if (kind == "binding" && int.TryParse(arg, out int i) && i >= 0 && i < prof.Bindings.Count)
            return new BindingPage(prof.Bindings[i]);
        if (kind == "wheel" && prof.Wheels.TryGetValue(arg, out var wheel))
            return new WheelPage(wheel);
        if (kind == "controllers" && arg.Length > 2 && int.TryParse(arg[..1], out int ch))
            return new ControllersPage(ch, arg[2..]);
        if (key is "wrist0" or "wrist1")
            return new WristPage(key == "wrist0" ? 0 : 1);
        if (kind is "wrist0" or "wrist1" && arg == "settings")
            return new WristPage(kind == "wrist0" ? 0 : 1, settingsTab: true);
        if (kind == "settings")
        {
            if (arg.Contains('?')) // settings:<page>?<word>: the page with that word lit up
            { SettingsPageView.Highlight = arg[(arg.IndexOf('?') + 1)..]; arg = arg[..arg.IndexOf('?')]; }
            return arg == "" ? new SettingsHubPage() : new SettingsPageView(arg);
        }
        if (key == "tutorials")
            return new TutorialsPage();
        if (kind == "bindings") // bindings:<word>: All bindings, searched
            return new AllBindingsPage(arg);
        return new NotPortedPage(TitleOf(key));
    }

    public static void Go(string key)
    {
        if (key == Current)
            return;
        if (Current != null)
            back.Push(Current);
        Current = key;
        Navigated?.Invoke(key);
    }

    public static void GoBack()
    {
        if (back.Count == 0)
            return;
        Current = back.Pop();
        Navigated?.Invoke(Current);
    }

    // Reload the page shown (after a profile load or revert).
    public static void Refresh() => Navigated?.Invoke(Current);

    public static void SetStatus(string text, bool error = false) => Status?.Invoke(text, error);
}

// Placeholder for the pages not ported yet.
public class NotPortedPage : UserControl
{
    public NotPortedPage(string title)
    {
        Content = new StackPanel
        {
            Margin = new Avalonia.Thickness(24),
            Children =
            {
                new TextBlock { Text = title, FontSize = 26, FontWeight = Avalonia.Media.FontWeight.SemiBold },
                new TextBlock { Text = "Not ported to Avalonia yet. Use the WinForms editor for this page.", Margin = new Avalonia.Thickness(0, 8, 0, 0),
                                Foreground = Avalonia.Media.Brush.Parse("#AAB2BF") },
            },
        };
    }
}
