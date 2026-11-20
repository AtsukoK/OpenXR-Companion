using System.Text.Json;
using System.Text.Json.Nodes;

namespace OpenXRCompanionEditor;

// The headset mirror window's settings: app-wide (not per profile), shared with the layer, which
// polls this file every second. The layer also writes "on": false when the window's X is clicked.
public static class HeadsetMirrorFile
{
    public static string Path => System.IO.Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "OpenXRCompanion", "headset_mirror.json");

    static JsonObject Read()
    {
        try
        { return JsonNode.Parse(File.ReadAllText(Path)) as JsonObject ?? new JsonObject(); }
        catch { return new JsonObject(); }
    }
    static void Write(JsonObject j)
    {
        Directory.CreateDirectory(System.IO.Path.GetDirectoryName(Path)!);
        AtomicFile.WriteAllText(Path, j.ToJsonString(new JsonSerializerOptions { WriteIndented = true }));
    }
    static T Get<T>(string k, T def) { try { var n = Read()[k]; return n == null ? def : n.GetValue<T>(); } catch { return def; } }
    static void Set(string k, JsonNode v) { var j = Read(); j[k] = v; Write(j); }

    public static bool On { get => Get("on", false); set => Set("on", value); }
    public static string Eye { get => Get("eye", "right"); set => Set("eye", value); }
    public static double Zoom { get => Get("zoom", 1.3); set => Set("zoom", Math.Round(value, 2)); }
    public static double Smoothing { get => Get("smoothing", 0.5); set => Set("smoothing", Math.Round(value, 2)); }
    public static bool OnTop { get => Get("on_top", false); set => Set("on_top", value); }
    public static int Fps { get => Get("fps", 60); set => Set("fps", value); }
}
