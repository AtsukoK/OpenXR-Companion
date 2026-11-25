using Avalonia.Controls;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// The Steam Frame setting (every game on this PC, not one profile; WinForms SteamFrameSetting): the layer then asks
// SteamVR for the Frame controller itself, so all its buttons work. steam_frame.json, read when a game starts.
// Written straight away, and only when the user changes it.
public static class SteamFrameBox
{
    static string FilePath => Path.Combine(ProfileStore.RealDataDir, "steam_frame.json");

    public static bool Load()
    {
        try
        {
            if (!File.Exists(FilePath)) return false;
            using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(FilePath));
            return doc.RootElement.TryGetProperty("native", out var v) && v.ValueKind == System.Text.Json.JsonValueKind.True;
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or System.Text.Json.JsonException) { return false; }
    }

    static void Save(bool native)
    {
        try { AtomicFile.WriteAllText(FilePath, native ? "{ \"native\": true }" : "{ \"native\": false }"); }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }

    public static CheckBox New()
    {
        var box = new CheckBox { Content = "Use all the Steam Frame's buttons (restart the game after changing)", IsChecked = Load(), Margin = new Avalonia.Thickness(16, 0, 0, 0) };
        ToolTip.SetTip(box,
            "Off: SteamVR shows the Frame to games as Index controllers, so its right X / Y, left D-pad, View and bumpers can't be used here.\n" +
            "On: OpenXR Companion asks SteamVR for the Frame controller itself and gives it the game's own bindings moved onto the Frame " +
            "(left X / Y or A / B on the D-pad down / up, the menu on View / Menu). Games that support the Frame keep their own bindings.\n" +
            "Your SteamVR binding changes for the game are not used while this is on.");
        box.IsCheckedChanged += (s, e) => { bool v = box.IsChecked == true; if (v != Load()) Save(v); };
        return box;
    }
}
