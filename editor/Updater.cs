using System.Diagnostics;
using System.IO.Compression;
using System.Net.Http;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// One-click update from the GitHub release (port of the WinForms Updater). Only runs when the user clicks Update.
// 1. download the release zip from github.com (HTTPS), 2. check it against the SHA-256 GitHub lists for that file,
// 3. unpack to a temp folder, 4. copy the files it replaces to "_previous_version", 5. swap them in: each old file is
// renamed to *.old first (Windows allows that while the editor or a game uses it), then the new one copied in; any
// failure puts everything back, 6. restart. Leftover *.old files are deleted the next time the editor starts.
//
// Which editor restarts: a release zip that contains an exe with this editor's own name replaces it and that one starts;
// releases that only carry the WinForms editor (OpenXRCompanionEditor.exe) still update the layer and the other files,
// and this editor (unchanged) starts again.
public static class Updater
{
    public sealed class Release { public string Tag = "", ZipUrl = "", Sha256 = ""; public long Size; }

    public static string InstallDir => AppContext.BaseDirectory.TrimEnd('\\', '/');
    static string ExePath => Environment.ProcessPath ?? Path.Combine(InstallDir, "OpenXRCompanionEditor.exe");
    const string BackupName = "_previous_version";

    public static string AppVersionText => typeof(Updater).Assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion ?? "0.0.0";
    public static Version AppVersion => Version.TryParse(AppVersionText.Split('+', '-')[0], out var v) ? v : new Version(0, 0, 0);
    public static bool Experimental => AppVersionText.Contains("-experimental");

    // Only a real release folder can update itself: the single-file editor next to the layer.
    // (A development build has OpenXRCompanion.Core.dll beside the exe and is left alone.)
    public static string CantUpdateReason()
    {
        string d = InstallDir;
        if (!File.Exists(Path.Combine(d, "OpenXRCompanion.dll")) || !File.Exists(Path.Combine(d, "XR_APILAYER_NOVENDOR_openxr_companion.json")))
            return "the editor is not in an OpenXR Companion folder";
        if (File.Exists(Path.Combine(d, "OpenXRCompanion.Core.dll")))
            return "this is a development build";
        try
        { var t = Path.Combine(d, ".write_test"); File.WriteAllText(t, ""); File.Delete(t); }
        catch { return "the folder is read-only (for example under Program Files); download the zip instead"; }
        return null;
    }

    static HttpClient Http(TimeSpan timeout)
    {
        var h = new HttpClient { Timeout = timeout };
        h.DefaultRequestHeaders.UserAgent.ParseAdd("OpenXRCompanion-Editor/" + AppVersion.ToString(3));
        return h;
    }

    public static async Task<Release> LatestAsync()
    {
        using var http = Http(TimeSpan.FromSeconds(10));
        http.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github+json");
        var rel = await Releases.PickAsync(http, Experimental) ?? throw new Exception("no release found");
        var r = new Release { Tag = rel.GetProperty("tag_name").GetString() ?? "" };
        foreach (var a in rel.GetProperty("assets").EnumerateArray())
        {
            string name = a.GetProperty("name").GetString() ?? "";
            if (!name.StartsWith("OpenXRCompanion-", StringComparison.OrdinalIgnoreCase) || !name.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
                continue;
            r.ZipUrl = a.GetProperty("browser_download_url").GetString() ?? "";
            r.Size = a.TryGetProperty("size", out var sz) ? sz.GetInt64() : 0;
            if (a.TryGetProperty("digest", out var dg) && dg.ValueKind == JsonValueKind.String && (dg.GetString() ?? "").StartsWith("sha256:"))
                r.Sha256 = dg.GetString()!.Substring(7).ToLowerInvariant();
        }
        if (!r.ZipUrl.StartsWith("https://github.com/AtsukoK/OpenXR-Companion/releases/download/", StringComparison.OrdinalIgnoreCase))
            throw new Exception("the release has no zip to download");
        if (r.Sha256.Length != 64)
            throw new Exception("GitHub lists no checksum for the zip, so it can't be checked");
        return r;
    }

    static string WorkDir => Path.Combine(Path.GetTempPath(), "OpenXRCompanionUpdate");

    // 1 + 2: download and check. Returns the zip's path. progress gets (percent or -1, text).
    public static async Task<string> DownloadAsync(Release r, Action<int, string> progress)
    {
        string work = WorkDir;
        try { if (Directory.Exists(work)) Directory.Delete(work, true); } catch { }
        Directory.CreateDirectory(work);
        string zip = Path.Combine(work, "update.zip");
        using (var http = Http(TimeSpan.FromMinutes(10)))
        using (var resp = await http.GetAsync(r.ZipUrl, HttpCompletionOption.ResponseHeadersRead))
        {
            resp.EnsureSuccessStatusCode();
            long total = resp.Content.Headers.ContentLength ?? r.Size;
            await using var src = await resp.Content.ReadAsStreamAsync();
            await using var dst = File.Create(zip);
            var buf = new byte[1 << 16];
            long got = 0;
            int n, lastPct = -1;
            while ((n = await src.ReadAsync(buf)) > 0)
            {
                await dst.WriteAsync(buf.AsMemory(0, n));
                got += n;
                int pct = total > 0 ? (int)(got * 100 / total) : -1;
                if (pct != lastPct)
                { lastPct = pct; progress(pct, "Downloading " + r.Tag + (pct >= 0 ? " (" + pct + "%)" : "") + "..."); }
            }
        }
        progress(-1, "Checking the download...");
        string hash;
        await using (var f = File.OpenRead(zip))
            hash = Convert.ToHexString(await SHA256.HashDataAsync(f)).ToLowerInvariant();
        if (hash != r.Sha256)
            throw new Exception("the download doesn't match GitHub's checksum (nothing was changed)");
        return zip;
    }

    // 3 - 5: unpack, back up, swap in. Returns the editor exe to start afterwards.
    public static string Install(string zip, Action<int, string> progress)
    {
        string work = Path.Combine(WorkDir, "files");
        try { if (Directory.Exists(work)) Directory.Delete(work, true); } catch { }
        progress(-1, "Unpacking...");
        ZipFile.ExtractToDirectory(zip, work);
        string root = Path.Combine(work, "OpenXRCompanion");
        if (!Directory.Exists(root))
            root = work;
        foreach (var need in new[] { "OpenXRCompanion.dll", "XR_APILAYER_NOVENDOR_openxr_companion.json" })
            if (!File.Exists(Path.Combine(root, need)))
                throw new Exception("the zip is missing " + need + " (nothing was changed)");
        var files = Directory.GetFiles(root, "*", SearchOption.AllDirectories)
            .Select(p => Path.GetRelativePath(root, p))
            .Where(rel => !rel.Contains("..") && !Path.IsPathRooted(rel))
            .ToList();
        // This editor has the normal program's name (OpenXRCompanionEditor.exe), and so does the WinForms editor in releases
        // made before the Avalonia editor ships. Don't swap this editor for that one: keep it, install everything else.
        string me = Path.GetFileName(ExePath);
        string theirs = Path.Combine(root, me);
        if (File.Exists(theirs) && !IsAvaloniaBuild(theirs))
        {
            files.RemoveAll(rel => string.Equals(rel, me, StringComparison.OrdinalIgnoreCase));
            progress(-1, "This release carries the older editor: keeping this one, updating the rest...");
        }

        // 4. back up what it replaces
        progress(-1, "Saving the current version to " + BackupName + "...");
        string dir = InstallDir, backup = Path.Combine(dir, BackupName);
        try { if (Directory.Exists(backup)) Directory.Delete(backup, true); } catch { }
        foreach (var rel in files)
        {
            string cur = Path.Combine(dir, rel);
            if (!File.Exists(cur))
                continue;
            string b = Path.Combine(backup, rel);
            Directory.CreateDirectory(Path.GetDirectoryName(b)!);
            File.Copy(cur, b, true);
        }

        // The editor is one file: .NET reads its parts from the exe the first time each is used. Once the exe is swapped,
        // a part not used yet can't be found, so load all of them now, while the old exe is in place.
        PreloadAll();

        // 5. swap in
        progress(-1, "Installing...");
        var done = new List<(string target, string old)>();
        try
        {
            foreach (var rel in files)
            {
                string target = Path.Combine(dir, rel), old = target + ".old";
                Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                if (File.Exists(old))
                { try { File.Delete(old); } catch { old = target + "." + Guid.NewGuid().ToString("N")[..8] + ".old"; } }
                bool had = File.Exists(target);
                if (had)
                    File.Move(target, old);
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
        try { Directory.Delete(WorkDir, true); } catch { }
        return ExePath; // replaced by the release's own copy when it had one with this name, else this editor as it is
    }

    // A single-file Avalonia editor lists "Avalonia.Base.dll" in its bundle (the names are stored as plain text);
    // the WinForms editor doesn't.
    public static bool IsAvaloniaBuild(string exe)
    {
        try
        {
            var needle = System.Text.Encoding.UTF8.GetBytes("Avalonia.Base.dll");
            return File.ReadAllBytes(exe).AsSpan().IndexOf(needle) >= 0;
        }
        catch { return false; }
    }

    // At start-up: remove the *.old files an update left (some may still be in use by a running game; next time then).
    public static void CleanUp()
    {
        try
        {
            if (CantUpdateReason() != null)
                return;
            foreach (var f in Directory.GetFiles(InstallDir, "*.old", SearchOption.AllDirectories))
                try { File.Delete(f); } catch { }
        }
        catch { }
    }

    static void PreloadAll()
    {
        var seen = new HashSet<string>();
        var queue = new Queue<Assembly>();
        foreach (var a in AppDomain.CurrentDomain.GetAssemblies())
            if (seen.Add(a.FullName ?? ""))
                queue.Enqueue(a);
        while (queue.Count > 0)
            foreach (var name in queue.Dequeue().GetReferencedAssemblies())
            {
                if (!seen.Add(name.FullName))
                    continue;
                try { queue.Enqueue(Assembly.Load(name)); }
                catch { }
            }
        _ = typeof(Process).TypeHandle;
        _ = typeof(ProcessStartInfo).TypeHandle;
    }

    public static void Restart(string exe, string fromVersion) =>
        Process.Start(new ProcessStartInfo(exe, "--updated " + fromVersion) { UseShellExecute = false, WorkingDirectory = InstallDir });

    // Tests: start the editor after an update and wait (Process first used here, after the swap).
    [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.NoInlining)]
    public static int StartAndWait(string exe, string args)
    {
        var p = Process.Start(new ProcessStartInfo(exe, args) { UseShellExecute = false, WorkingDirectory = InstallDir });
        if (p == null) return -1;
        p.WaitForExit(60000);
        return p.HasExited ? p.ExitCode : -2;
    }
}
