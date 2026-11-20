using System.Diagnostics;
using System.Drawing;
using System.Net.Http;
using System.Reflection;
using System.Text.Json;

namespace OpenXRCompanionEditor;

// Asks GitHub once per start whether a newer release exists. Only reads the public
// "latest release" record; nothing is sent about the user and nothing is downloaded.
// Turn it off with the "Check for updates" option (a flag file in the settings folder).
public partial class MainForm
{
    const string RepoApi = "https://api.github.com/repos/AtsukoK/OpenXR-Companion/releases/latest";
    const string ReleasesPage = "https://github.com/AtsukoK/OpenXR-Companion/releases/latest";
    static string NoUpdateFlag => Path.Combine(DataDir, "no_update_check");
    public static bool UpdateCheckOn
    {
        get => !File.Exists(NoUpdateFlag);
        set { try { if (value) File.Delete(NoUpdateFlag); else { Directory.CreateDirectory(DataDir); File.WriteAllText(NoUpdateFlag, "off"); } } catch { } }
    }

    readonly ToolStripButton updateBtn = new() { Visible = false, Alignment = ToolStripItemAlignment.Right };

    public static Version AppVersion
    {
        get
        {
            var s = Assembly.GetExecutingAssembly().GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion ?? "0.0.0";
            s = s.Split('+', '-')[0];
            return Version.TryParse(s, out var v) ? v : new Version(0, 0, 0);
        }
    }

    void AddUpdateButton(ToolStrip ts)
    {
        updateBtn.BackColor = Color.FromArgb(240, 140, 30);
        updateBtn.ForeColor = Color.White;
        updateBtn.Font = new Font(Font, FontStyle.Bold);
        updateBtn.Click += async (s, e) => await UpdateClicked();
        ts.Items.Add(updateBtn);
        var args = Environment.GetCommandLineArgs();
        if (args.Length == 3 && args[1] == "--updated") // started by the updater
        {
            Shown += (s, e) => SetStatus("Updated from v" + args[2] + " to v" + AppVersion.ToString(3) + ". The previous files are in the _previous_version folder. A running VR game keeps the old version until you restart it.", false);
            return;
        }
        if (args.Length > 1 || !UpdateCheckOn) return; // tests and screenshots never go online
        Shown += async (s, e) =>
        {
            var latest = await LatestReleaseAsync();
            if (latest == null || IsDisposed) return;
            var (ver, tag) = latest.Value;
            if (ver <= AppVersion) return;
            updateBtn.Text = "Update available: " + tag;
            updateBtn.ToolTipText = "You have v" + AppVersion.ToString(3) + ". Click to update.";
            updateBtn.Visible = true;
            SetStatus("A new version (" + tag + ") is available: click the orange button at the top right to update.", false);
        };
    }

    bool updating;
    async Task UpdateClicked()
    {
        if (updating) return;
        string why = Updater.CantUpdateReason();
        if (why != null)
        {
            if (MessageBox.Show(this, "The editor can't update itself here: " + why + ".\n\nOpen the download page instead?", "Update",
                    MessageBoxButtons.YesNo, MessageBoxIcon.Information) == DialogResult.Yes) OpenReleasesPage();
            return;
        }
        var ask = MessageBox.Show(this,
            "Update to " + updateBtn.Text.Replace("Update available: ", "") + " now?\n\n" +
            "The editor downloads it from GitHub, checks it, keeps your current version in the _previous_version folder, and restarts. " +
            "Your profiles and settings are not touched.\n\nBest with your VR games closed: a running game keeps the old version until you restart it.\n\n" +
            "Yes = update now, No = open the download page instead.", "Update", MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question);
        if (ask == DialogResult.No) { OpenReleasesPage(); return; }
        if (ask != DialogResult.Yes) return;
        if (dirty && MessageBox.Show(this, "You have unsaved changes. Save them before updating?", "Update", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
            Save();
        updating = true; updateBtn.Enabled = false;
        string was = AppVersion.ToString(3);
        try
        {
            var r = await Updater.LatestAsync();
            string exe = await Updater.RunAsync(r, (pct, text) => BeginInvoke(() => { updateBtn.Text = pct >= 0 ? "Downloading " + pct + "%" : "Updating..."; SetStatus(text, false); }));
            Updater.Restart(exe, was);
            dirty = false;
            Close();
        }
        catch (Exception ex)
        {
            updating = false; updateBtn.Enabled = true; updateBtn.Text = "Update available";
            SetStatus("Update failed: " + ex.Message, true);
            if (MessageBox.Show(this, "The update didn't finish: " + ex.Message + "\n\nYour current version is unchanged. Open the download page instead?", "Update",
                    MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.Yes) OpenReleasesPage();
        }
    }
    static void OpenReleasesPage() { try { Process.Start(new ProcessStartInfo(ReleasesPage) { UseShellExecute = true }); } catch { } }

    // Returns the newest release version, or null when offline or anything goes wrong.
    public static async Task<(Version ver, string tag)?> LatestReleaseAsync()
    {
        try
        {
            using var http = new HttpClient { Timeout = TimeSpan.FromSeconds(6) };
            http.DefaultRequestHeaders.UserAgent.ParseAdd("OpenXRCompanion-Editor/" + AppVersion.ToString(3));
            http.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github+json");
            using var doc = JsonDocument.Parse(await http.GetStringAsync(RepoApi));
            string tag = doc.RootElement.GetProperty("tag_name").GetString() ?? "";
            return Version.TryParse(tag.TrimStart('v', 'V').Split('-')[0], out var v) ? (v, tag) : null;
        }
        catch { return null; }
    }
}
