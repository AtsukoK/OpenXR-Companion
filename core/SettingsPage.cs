namespace OpenXRCompanionEditor;

// The Settings pages: key, title, what is on it, icon; Live = applies straight away (app-wide, not the profile).
public sealed class SettingsPage
{
    public string Key, Title, Sub, Icon;
    public bool Live; // applies straight away (not saved with the profile)
    public static readonly SettingsPage[] All =
    {
        new() { Key = "general", Title = "General", Sub = "On/off, vibration, messages, updates", Icon = "\uE713" },
        new() { Key = "wheel", Title = "Wheel look and picking", Sub = "Where it appears, size, colours, how items are picked", Icon = "\uE790" },
        new() { Key = "pointing", Title = "Pointing", Sub = "Laser direction, fingertip, steadying", Icon = "\uE7C9" },
        new() { Key = "keyboard", Title = "VR keyboard", Sub = "Size and position", Icon = "\uE765" },
        new() { Key = "screen", Title = "Screen mirror", Sub = "A monitor or window in VR", Icon = "\uE7F4" },
        new() { Key = "offsets", Title = "Controller offsets", Sub = "How the game's hands and guns point", Icon = "\uE7FC" },
        new() { Key = "mirror", Title = "Headset mirror", Sub = "What the headset shows, on the desktop", Icon = "\uE890", Live = true },
        new() { Key = "experimental", Title = "Experimental", Sub = "Head smoothing (can cause motion sickness)", Icon = "\uE7BA", Live = true },
        new() { Key = "advanced", Title = "Advanced", Sub = "Thresholds, wheel details, focus", Icon = "\uE90F" },
    };
    public static SettingsPage ByKey(string k) => All.FirstOrDefault(p => p.Key == k) ?? All[0];
}
