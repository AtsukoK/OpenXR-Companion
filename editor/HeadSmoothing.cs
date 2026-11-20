using System.Text.Json;
using System.Text.Json.Nodes;

namespace OpenXRCompanionEditor;

// Head smoothing (experimental): app-wide, shared with the layer, which polls this file every second.
public static class HeadSmoothingFile
{
    public static string Path => System.IO.Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "OpenXRCompanion", "head_smoothing.json");

    static JsonObject Read()
    {
        try { return JsonNode.Parse(File.ReadAllText(Path)) as JsonObject ?? new JsonObject(); }
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
    public static string Mode { get => Get("mode", "recording"); set => Set("mode", value); } // "recording" or "headset"
    public static bool KeepOn { get => Get("keep_on", false); set => Set("keep_on", value); }
    public static bool ShowPage { get => Get("show_page", false); set => Set("show_page", value); } // the Experimental page is listed
    public static double Strength { get => Get("strength", 0.5); set => Set("strength", Math.Round(value, 2)); }
}
