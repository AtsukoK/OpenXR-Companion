using System.Net.Http;
using System.Text.Json;

namespace OpenXRCompanionEditor;

// The GitHub releases (read-only): which one an update would install.
public static class Releases
{
    // The release an update would install: stable builds only look at the latest stable release; experimental builds
    // take the newest of all releases, prerelease or not. Null when there is none.
    public static async Task<JsonElement?> PickAsync(HttpClient http, bool experimental)
    {
        const string api = "https://api.github.com/repos/AtsukoK/OpenXR-Companion/releases";
        if (!experimental)
        {
            using var doc = JsonDocument.Parse(await http.GetStringAsync(api + "/latest"));
            return doc.RootElement.Clone();
        }
        using var all = JsonDocument.Parse(await http.GetStringAsync(api + "?per_page=30"));
        JsonElement? best = null;
        Version bestVer = null;
        foreach (var r in all.RootElement.EnumerateArray())
        {
            if (r.TryGetProperty("draft", out var d) && d.GetBoolean())
                continue;
            string tag = r.GetProperty("tag_name").GetString() ?? "";
            if (!Version.TryParse(tag.TrimStart('v', 'V').Split('-')[0], out var v))
                continue;
            if (bestVer == null || v > bestVer)
            { bestVer = v; best = r.Clone(); }
        }
        return best;
    }

    // The newest release an app of this version would update to (null: offline or anything goes wrong).
    public static async Task<(Version ver, string tag)?> LatestAsync(bool experimental, string appVersion)
    {
        try
        {
            using var http = new HttpClient { Timeout = TimeSpan.FromSeconds(6) };
            http.DefaultRequestHeaders.UserAgent.ParseAdd("OpenXRCompanion-Editor/" + appVersion);
            http.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github+json");
            if (await PickAsync(http, experimental) is not JsonElement rel)
                return null;
            string tag = rel.GetProperty("tag_name").GetString() ?? "";
            return Version.TryParse(tag.TrimStart('v', 'V').Split('-')[0], out var v) ? (v, tag) : null;
        }
        catch { return null; }
    }
}
