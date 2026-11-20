using System.Diagnostics;
using System.IO.Compression;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text.Json;

namespace OpenXRCompanionEditor;

// One-click update from the GitHub release. Only runs when the user clicks "Update now".
// 1. downloads the release zip from github.com (HTTPS), 2. checks it against the SHA-256 GitHub lists
// for that file, 3. unpacks it to a temp folder, 4. copies the previous files to "_previous_version",
// 5. swaps the files in: each old file is first renamed to *.old (Windows allows that even while the
// editor or a game is using it), then the new one is copied in; on any failure everything is put back,
// 6. restarts the editor. Leftover *.old files are deleted the next time the editor starts.
public static class Updater
{
    public sealed class Release { public string Tag = "", ZipUrl = "", Sha256 = ""; public long Size; }

    public static string InstallDir => AppContext.BaseDirectory.TrimEnd('\\', '/');
    const string BackupName = "_previous_version";

    // Only a real release folder can update itself: the single-file editor next to the layer.
    // (A development build has OpenXRCompanionEditor.dll beside the exe and is left alone.)
    public static string CantUpdateReason()
    {
        string d = InstallDir;
        if (!File.Exists(Path.Combine(d, "OpenXRCompanion.dll")) || !File.Exists(Path.Combine(d, "XR_APILAYER_NOVENDOR_openxr_companion.json")))
            return "the editor is not in an OpenXR Companion folder";
        if (File.Exists(Path.Combine(d, "OpenXRCompanionEditor.dll"))) return "this is a development build";
        try { var t = Path.Combine(d, ".write_test"); File.WriteAllText(t, ""); File.Delete(t); }
        catch { return "the folder is read-only (for example under Program Files); download the zip instead"; }
        return null;
    }

    public static async Task<Release> LatestAsync()
    {
        using var http = Http(TimeSpan.FromSeconds(10));
        http.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github+json");
        using var doc = JsonDocument.Parse(await http.GetStringAsync("https://api.github.com/repos/AtsukoK/OpenXR-Companion/releases/latest"));
        var r = new Release { Tag = doc.RootElement.GetProperty("tag_name").GetString() ?? "" };
        foreach (var a in doc.RootElement.GetProperty("assets").EnumerateArray())
        {
            string name = a.GetProperty("name").GetString() ?? "";
            if (!name.StartsWith("OpenXRCompanion-", StringComparison.OrdinalIgnoreCase) || !name.EndsWith(".zip", StringComparison.OrdinalIgnoreCase)) continue;
            r.ZipUrl = a.GetProperty("browser_download_url").GetString() ?? "";
            r.Size = a.TryGetProperty("size", out var sz) ? sz.GetInt64() : 0;
            if (a.TryGetProperty("digest", out var dg) && dg.ValueKind == JsonValueKind.String && (dg.GetString() ?? "").StartsWith("sha256:"))
                r.Sha256 = dg.GetString()!.Substring(7).ToLowerInvariant();
        }
        if (!r.ZipUrl.StartsWith("https://github.com/AtsukoK/OpenXR-Companion/releases/download/", StringComparison.OrdinalIgnoreCase))
            throw new Exception("the release has no zip to download");
        if (r.Sha256.Length != 64) throw new Exception("GitHub lists no checksum for the zip, so it can't be checked");
        return r;
    }

    static HttpClient Http(TimeSpan timeout)
    {
        var h = new HttpClient { Timeout = timeout };
        h.DefaultRequestHeaders.UserAgent.ParseAdd("OpenXRCompanion-Editor/" + MainForm.AppVersion.ToString(3));
        return h;
    }

    // Downloads, checks and installs. progress gets (percent or -1, text). Returns the new editor's path.
    public static async Task<string> RunAsync(Release r, Action<int, string> progress)
    {
        string work = Path.Combine(Path.GetTempPath(), "OpenXRCompanionUpdate");
        try { if (Directory.Exists(work)) Directory.Delete(work, true); } catch { }
        Directory.CreateDirectory(work);
        string zip = Path.Combine(work, "update.zip");

        // 1. download
        using (var http = Http(TimeSpan.FromMinutes(10)))
        using (var resp = await http.GetAsync(r.ZipUrl, HttpCompletionOption.ResponseHeadersRead))
        {
            resp.EnsureSuccessStatusCode();
            long total = resp.Content.Headers.ContentLength ?? r.Size;
            await using var src = await resp.Content.ReadAsStreamAsync();
            await using var dst = File.Create(zip);
            var buf = new byte[1 << 16]; long got = 0; int n, lastPct = -1;
            while ((n = await src.ReadAsync(buf)) > 0)
            {
                await dst.WriteAsync(buf.AsMemory(0, n)); got += n;
                int pct = total > 0 ? (int)(got * 100 / total) : -1;
                if (pct != lastPct) { lastPct = pct; progress(pct, "Downloading " + r.Tag + (pct >= 0 ? " (" + pct + "%)" : "") + "..."); }
            }
        }

        // 2. checksum
        progress(-1, "Checking the download...");
        string hash;
        await using (var f = File.OpenRead(zip)) hash = Convert.ToHexString(await SHA256.HashDataAsync(f)).ToLowerInvariant();
        if (hash != r.Sha256) throw new Exception("the download doesn't match GitHub's checksum (nothing was changed)");

        // 3. unpack and check it looks right
        progress(-1, "Unpacking...");
        string stage = Path.Combine(work, "files");
        ZipFile.ExtractToDirectory(zip, stage);
        string root = Path.Combine(stage, "OpenXRCompanion");
        if (!Directory.Exists(root)) root = stage;
        foreach (var need in new[] { "OpenXRCompanionEditor.exe", "OpenXRCompanion.dll", "XR_APILAYER_NOVENDOR_openxr_companion.json" })
            if (!File.Exists(Path.Combine(root, need))) throw new Exception("the zip is missing " + need + " (nothing was changed)");
        var files = Directory.GetFiles(root, "*", SearchOption.AllDirectories)
            .Select(p => Path.GetRelativePath(root, p))
            .Where(rel => !rel.Contains("..") && !Path.IsPathRooted(rel))
            .ToList();

        // 4. back up what is there now
        progress(-1, "Saving the current version to " + BackupName + "...");
        string dir = InstallDir, backup = Path.Combine(dir, BackupName);
        try { if (Directory.Exists(backup)) Directory.Delete(backup, true); } catch { }
        foreach (var rel in files)
        {
            string cur = Path.Combine(dir, rel);
            if (!File.Exists(cur)) continue;
            string b = Path.Combine(backup, rel);
            Directory.CreateDirectory(Path.GetDirectoryName(b)!);
            File.Copy(cur, b, true);
        }

        // 5. swap in: rename the old file aside (works while in use), copy the new one; undo everything on failure
        progress(-1, "Installing " + r.Tag + "...");
        var done = new List<(string target, string old)>();
        try
        {
            foreach (var rel in files)
            {
                string target = Path.Combine(dir, rel), old = target + ".old";
                Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                if (File.Exists(old)) { try { File.Delete(old); } catch { old = target + "." + Guid.NewGuid().ToString("N")[..8] + ".old"; } }
                bool had = File.Exists(target);
                if (had) File.Move(target, old);
                done.Add((target, had ? old : null));
                File.Copy(Path.Combine(root, rel), target);
            }
        }
        catch
        {
            for (int i = done.Count - 1; i >= 0; i--)
            {
                var (target, old) = done[i];
                try { if (File.Exists(target)) File.Delete(target); } catch { }
                try { if (old != null) File.Move(old, target); } catch { }
            }
            throw;
        }
        try { Directory.Delete(work, true); } catch { }
        return Path.Combine(dir, "OpenXRCompanionEditor.exe");
    }

    // At start-up: remove the *.old files an update left (some may still be in use by a running game; next time then).
    public static void CleanUp()
    {
        try
        {
            if (CantUpdateReason() != null) return;
            foreach (var f in Directory.GetFiles(InstallDir, "*.old", SearchOption.AllDirectories))
                try { File.Delete(f); } catch { }
        }
        catch { }
    }

    public static void Restart(string exe, string fromVersion)
    {
        Process.Start(new ProcessStartInfo(exe, "--updated " + fromVersion) { UseShellExecute = false, WorkingDirectory = InstallDir });
    }
}
