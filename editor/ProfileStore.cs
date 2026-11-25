using System.Reflection;
using System.Text.Json;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Loading and saving profiles, the way the WinForms editor does it.
// Reading and writing go through the shared core (ProfileIO), the same code the WinForms editor uses.
public class ProfileStore
{
    const string StandardFile = "default.json", UserDefaultsFile = "user_defaults.json";


    public string DataDir { get; }
    public string ProfilesDir => Path.Combine(DataDir, "profiles");
    // Saving writes files: off unless the port was started on a copy of the data folder (--data <folder>).
    public bool CanSave { get; }

    public ProfileModel Profile { get; private set; }
    public string CurrentFile { get; private set; }
    public string CurrentName => Path.GetFileName(CurrentFile ?? "");
    public bool Dirty { get; private set; }
    public event Action Changed; // dirty flag or loaded profile changed

    public ProfileStore(string dataDir, bool canSave)
    {
        DataDir = dataDir;
        CanSave = canSave;
    }

    public static string RealDataDir =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "OpenXRCompanion");

    public IEnumerable<string> ProfileNames() =>
        Directory.Exists(ProfilesDir)
            ? Directory.GetFiles(ProfilesDir, "*.json").Select(Path.GetFileName).OrderBy(n => n, StringComparer.OrdinalIgnoreCase)
            : Enumerable.Empty<string>();

    // The profile games use (active_profile.txt), else user_defaults.json, else default.json.
    public string ActiveName
    {
        get
        {
            try
            {
                var n = File.ReadAllText(Path.Combine(DataDir, "active_profile.txt")).Trim();
                if (n.Length > 0 && File.Exists(Path.Combine(ProfilesDir, n)))
                    return n;
            }
            catch (IOException) { }
            return File.Exists(Path.Combine(ProfilesDir, UserDefaultsFile)) ? UserDefaultsFile : StandardFile;
        }
    }

    public void Load(string name)
    {
        string path = Path.Combine(ProfilesDir, name);
        Profile = FromText(File.ReadAllText(path));
        CurrentFile = path;
        Dirty = false;
        Remember();
        Changed?.Invoke();
    }

    // ---- changes made in the headset (the in-game menu writes the profile file itself)
    DateTime stamp;
    string activeSeen;

    void Remember()
    {
        try { stamp = File.GetLastWriteTimeUtc(CurrentFile); } catch (IOException) { stamp = default; }
        activeSeen = ActiveName;
    }

    // Called every second. Returns a status line (and whether it's a warning) when the profile changed on disk, else null.
    // Nothing unsaved here: the file is loaded again. Unsaved edits here: only a warning (Save keeps yours, Revert takes the headset's).
    public (string text, bool warn)? WatchDisk()
    {
        if (CurrentFile == null || !CanSave)
            return null;
        // A change made in the headset to the standard default.json goes to a new user_defaults file, which games use from then on.
        string act = ActiveName;
        if (!string.Equals(act, activeSeen, StringComparison.OrdinalIgnoreCase))
        {
            activeSeen = act;
            if (IsStandard(CurrentName) && !IsStandard(act))
            {
                if (Dirty)
                    return ("The headset saved its changes as " + act + " (now the profile games use). Pick it in the profile list to see them.", true);
                return TryReload(act, "The headset saved its changes as " + act + ": showing it now.");
            }
        }
        DateTime t;
        try { t = File.GetLastWriteTimeUtc(CurrentFile); } catch (IOException) { return null; }
        if (t == stamp)
            return null;
        if (Dirty)
        {
            stamp = t; // warn once
            return ("This profile was changed in the headset, but you have unsaved changes here. Save keeps yours (and overwrites the headset's), Revert loads the headset's.", true);
        }
        return TryReload(CurrentName, "Loaded the changes made in the headset.");
    }

    (string, bool)? TryReload(string name, string ok)
    {
        try
        {
            Load(name);
            return (ok, false);
        }
        catch (Exception) // half written: try again next second
        {
            return null;
        }
    }

    // Back to a snapshot taken with Serialize() (a tutorial's "throw it away"), keeping the file and the dirty flag given.
    public void Restore(string snapshot, bool dirty)
    {
        Profile = FromText(snapshot);
        Dirty = dirty;
        Changed?.Invoke();
    }

    static ProfileModel FromText(string text)
    {
        var p = ProfileIO.FromText(text);
        foreach (var (k, w) in p.Wheels)
            w.Name = k;
        p.WristLeft ??= new();
        p.WristRight ??= new() { Mirror = true };
        MigrateWrist(p);
        foreach (var wm in new[] { p.WristLeft, p.WristRight })
        {
            wm.Pages ??= new();
            foreach (var (k, w) in wm.Pages)
                w.Name = k;
        }
        EnsureWristPages(p);
        return p;
    }

    // Like the WinForms MainForm.Files: older profiles had one "wrist_panel" (with a hand), possibly pointing at a wheel.
    // Move it to wrist_left / wrist_right; the wheel becomes its page (a copy if something else opens that wheel).
    static void MigrateWrist(ProfileModel p)
    {
        var old = p.LegacyWrist;
        p.LegacyWrist = null;
        if (old == null)
            return;
        int h = (old.LegacyHand ?? "left").ToLowerInvariant() == "right" ? 1 : 0;
        old.LegacyHand = null;
        old.Mirror = false;
        old.Pages ??= new();
        if (old.Pages.Count == 0 && old.LegacyWheel != null && p.Wheels.TryGetValue(old.LegacyWheel, out var w))
        {
            string legacy = old.LegacyWheel;
            bool usedElsewhere = p.Bindings.Any(b => b.Action?.Type == ActionType.Wheel && b.Action.Wheel == legacy)
                || p.Wheels.Values.Any(o => o.Items.Any(i => i.Action?.Type == ActionType.Wheel && i.Action.Wheel == legacy));
            var page = usedElsewhere ? ItemLists.Clone(w) : w;
            page.Name = legacy;
            old.Pages[legacy] = page;
            old.Start = legacy;
            if (!usedElsewhere)
                p.Wheels.Remove(legacy);
        }
        old.LegacyWheel = null;
        if (h == 0) p.WristLeft = old;
        else p.WristRight = old;
    }

    // A wrist panel with no pages gets a first page (example buttons, switched off), like WinForms.
    static void EnsureWristPages(ProfileModel p)
    {
        for (int h = 0; h < 2; h++)
        {
            var wm = p.WristFor(h);
            if (wm.Pages.Count > 0)
                continue;
            var page = DefaultPage(h);
            wm.Pages[page.Name] = page;
            wm.Start = page.Name;
        }
    }

    public static WheelModel DefaultPage(int h)
    {
        string side = h == 0 ? "Left" : "Right";
        WheelItemModel Ex(string label, ActionModel a) => new() { Label = label, Enabled = false, Action = a };
        return new WheelModel
        {
            Name = side + " hand main",
            Title = side + " hand",
            Items = new()
            {
                Ex("Keyboard", new ActionModel { Type = ActionType.Keyboard }),
                Ex("Esc", new ActionModel { Type = ActionType.Key, Keys = "Esc" }),
                Ex("Map", new ActionModel { Type = ActionType.Key, Keys = "M" }),
                Ex("Inventory", new ActionModel { Type = ActionType.Key, Keys = "I" }),
                Ex("Walk toggle", new ActionModel { Type = ActionType.Key, Keys = "LCtrl", Mode = "toggle" }),
                Ex("Quick save", new ActionModel { Type = ActionType.Key, Keys = "F5" }),
            },
        };
    }

    public event Action Edited; // every change (pictures that show settings redraw on it)

    public void MarkDirty()
    {
        Edited?.Invoke();
        if (Dirty)
            return;
        Dirty = true;
        Changed?.Invoke();
    }

    public void Revert() => Load(CurrentName);

    // What the layer thinks of a profile text (null = fine).
    public static string Validate(string json) =>
        Native.Validate(json);

    // ---- the profiles themselves (WinForms NewProfile / RenameProfile / DeleteProfile). All write files: copies only.
    // Each returns null when done, else why not.

    static bool IsStandard(string name) => string.Equals(name, StandardFile, StringComparison.OrdinalIgnoreCase);
    static bool IsUserDefaults(string name) => string.Equals(name, UserDefaultsFile, StringComparison.OrdinalIgnoreCase);
    const string Off = "Saving is off (the editor was started in preview mode).";

    // The profile games use from now on (active_profile.txt, read by the layer).
    public void SetActive(string name)
    {
        if (CanSave)
            File.WriteAllText(Path.Combine(DataDir, "active_profile.txt"), name);
    }

    // A profile for a game: named after its program (bannerlord.json is used by Bannerlord.exe), copied from another.
    public string NewProfile(string exeName, string copyFrom)
    {
        if (!CanSave) return Off;
        string name = (exeName ?? "").Trim().ToLowerInvariant();
        if (name.EndsWith(".exe")) name = name[..^4];
        if (name.Length == 0 || name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
            return "That name can't be used for a file.";
        string path = Path.Combine(ProfilesDir, name + ".json");
        if (File.Exists(path))
            return "A profile for " + name + " already exists.";
        File.Copy(Path.Combine(ProfilesDir, copyFrom ?? ActiveName), path);
        Load(name + ".json");
        SetActive(CurrentName);
        return null;
    }

    public string Rename(string newName)
    {
        if (!CanSave) return Off;
        string file = CurrentName;
        if (IsStandard(file) || IsUserDefaults(file))
            return file + " has a fixed name (it is what games without their own profile use). Make a copy with 'New game profile...' instead.";
        string name = (newName ?? "").Trim().ToLowerInvariant();
        if (name.EndsWith(".json")) name = name[..^5];
        if (name.EndsWith(".exe")) name = name[..^4];
        if (name.Length == 0 || name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
            return "That name can't be used for a file.";
        string to = Path.Combine(ProfilesDir, name + ".json");
        if (string.Equals(to, CurrentFile, StringComparison.OrdinalIgnoreCase))
            return null;
        if (File.Exists(to))
            return "A profile called " + name + " already exists.";
        bool wasActive = string.Equals(ActiveName, file, StringComparison.OrdinalIgnoreCase);
        File.Move(CurrentFile, to);
        if (wasActive)
            SetActive(name + ".json");
        Load(name + ".json");
        return null;
    }

    public string Delete()
    {
        if (!CanSave) return Off;
        if (IsStandard(CurrentName))
            return "default.json is the standard profile and can't be deleted.";
        File.Delete(CurrentFile);
        Load(ActiveName); // the active one if it's still there, else user_defaults.json, else default.json
        return null;
    }

    // What deleting the current profile means for games, for the question before it.
    public string DeleteQuestion => IsUserDefaults(CurrentName)
        ? "Delete user_defaults.json? Your games switch to the standard default.json."
        : "Delete " + CurrentName + "? Your games switch to " + (File.Exists(Path.Combine(ProfilesDir, UserDefaultsFile)) ? UserDefaultsFile : StandardFile) + ".";

    // The same text the WinForms editor writes (both use the core's ProfileIO).
    public string Serialize() => ProfileIO.Serialize(Profile);

    // Saving, as the WinForms editor does it, in two steps so the window can ask in between:
    // 1. PlanSave: a problem that stops it (Error), what the layer would say about it (LayerSays, a question), and for
    //    the standard default.json the new file the changes go to instead (ForkTo: user_defaults.json, or
    //    "user_defaults N.json" when you already have your own, which is asked about first).
    // 2. Save(plan): writes it safely (temp file + swap, so a running game never reads half a file); a fork becomes the
    //    profile games use.
    public sealed record SavePlan(string Error, string LayerSays, string ForkTo, bool AskFork);

    public SavePlan PlanSave()
    {
        if (!CanSave)
            return new("Saving is off (the editor was started in preview mode).", null, null, false);
        var dupe = Profile.Wheels.Values.GroupBy(w => w.Name).FirstOrDefault(g => g.Count() > 1);
        if (dupe != null)
            return new("Two wheels share the name " + dupe.Key + ".", null, null, false);
        var pdupe = Profile.WristLeft.Pages.Values.Concat(Profile.WristRight.Pages.Values).GroupBy(w => w.Name).FirstOrDefault(g => g.Count() > 1);
        if (pdupe != null)
            return new("Two wrist pages share the name " + pdupe.Key + ".", null, null, false);
        string layerSays = Validate(Serialize());
        if (!string.Equals(CurrentName, StandardFile, StringComparison.OrdinalIgnoreCase))
            return new(null, layerSays, null, false);
        if (!File.Exists(Path.Combine(ProfilesDir, UserDefaultsFile)))
            return new(null, layerSays, UserDefaultsFile, false);
        string name = "user_defaults 2.json";
        for (int i = 3; File.Exists(Path.Combine(ProfilesDir, name)); i++)
            name = "user_defaults " + i + ".json";
        return new(null, layerSays, name, true);
    }

    // Returns what to show in the status bar.
    public string Save(SavePlan plan)
    {
        string json = Serialize();
        if (plan.ForkTo != null)
            CurrentFile = Path.Combine(ProfilesDir, plan.ForkTo);
        AtomicFile.WriteAllText(CurrentFile, json);
        Dirty = false;
        if (plan.ForkTo != null)
            SetActive(plan.ForkTo); // the new profile is the one games use now
        Remember();
        Changed?.Invoke();
        if (plan.ForkTo == UserDefaultsFile)
            return "Saved as user_defaults.json (your own defaults; default.json stays the standard). A running game switches to it within a second.";
        if (plan.ForkTo != null)
            return "Saved as the new profile " + plan.ForkTo + ", now the selected profile that games use. default.json and your user_defaults.json are unchanged.";
        return plan.LayerSays == null ? "Saved. A running game reloads it within a second." : "Saved with errors: " + plan.LayerSays;
    }

    // Kept for the tests: save without questions (a fork happens as planned).
    public string Save()
    {
        var plan = PlanSave();
        if (plan.Error != null)
            return plan.Error;
        Save(plan);
        return null;
    }
}
