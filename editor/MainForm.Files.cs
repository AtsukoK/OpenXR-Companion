using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Win32;

namespace OpenXRCompanionEditor;

// Profiles on disk: open, save, new, copy, delete, the standard and user defaults.
public partial class MainForm
{
    const string StandardFile = "default.json", UserDefaultsFile = "user_defaults.json";
    static bool IsStandard(string path) => string.Equals(Path.GetFileName(path), StandardFile, StringComparison.OrdinalIgnoreCase);
    // What games without their own profile use: the user's defaults if there are any, else the standard.
    internal static string StartProfile => File.Exists(Path.Combine(ProfilesDir, UserDefaultsFile)) ? UserDefaultsFile : StandardFile;
    // The profile selected here is the one games use (active_profile.txt, read by the layer).
    static string ActiveFile => Path.Combine(DataDir, "active_profile.txt");
    internal static string ActiveProfile
    {
        get { try { var n = File.ReadAllText(ActiveFile).Trim(); if (n.Length > 0 && File.Exists(Path.Combine(ProfilesDir, n))) return n; } catch { } return StartProfile; }
        set // only when it changes: every write makes a running game reload its profile (menus close)
        {
            try
            {
                Directory.CreateDirectory(DataDir);
                string cur = File.Exists(ActiveFile) ? File.ReadAllText(ActiveFile).Trim() : null;
                if (cur != value)
                    AtomicFile.WriteAllText(ActiveFile, value);
            }
            catch { }
        }
    }

    void RefreshProfileList(string select)
    {
        Native.PrepareDefaults();
        string def = Path.Combine(ProfilesDir, StandardFile);
        if (!File.Exists(def))
            File.WriteAllText(def, Native.DefaultProfile() ?? "{}");
        if (!File.Exists(Path.Combine(ProfilesDir, select)))
            select = StartProfile;
        loading = true;
        profileBox.Items.Clear();
        foreach (var f in Directory.GetFiles(ProfilesDir, "*.json").Select(Path.GetFileName)
                     .OrderBy(f => f != StandardFile).ThenBy(f => f != UserDefaultsFile).ThenBy(f => f))
            profileBox.Items.Add(f);
        profileBox.SelectedItem = select;
        loading = false;
        LoadFile(Path.Combine(ProfilesDir, select));
        ActiveProfile = select; // what is selected is what games use
    }

    // A profile's text as the editor's model (no wrist migration). Throws on text that isn't a profile.
    internal static WristModel DefaultWrist(int hand)
    {
        var d = ProfileFromText(Native.DefaultProfile() ?? "{}");
        return d.WristFor(hand) ?? new WristModel();
    }

    static ProfileModel ProfileFromText(string text)
    {
        var root = JsonNode.Parse(text, documentOptions: new JsonDocumentOptions { CommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true });
        // Accept a chord given as an array as well.
        foreach (var b in root?["bindings"]?.AsArray() ?? new JsonArray())
            if (b?["input"] is JsonArray arr)
                b["input"] = string.Join("+", arr.Select(x => x.ToString()));
        foreach (var b in root?["bindings"]?.AsArray() ?? new JsonArray())
            foreach (var k in new[] { "layers", "not_layers" })
                if (b?[k] is JsonArray la)
                    b[k] = string.Join(",", la.Select(x => x.ToString()));
        return root.Deserialize<ProfileModel>(JsonOpts) ?? new ProfileModel();
    }

    void LoadFile(string path)
    {
        try
        {
            Profile = ProfileFromText(File.ReadAllText(path));
            foreach (var (k, w) in Profile.Wheels)
                w.Name = k;
            Profile.WristLeft ??= new();
            Profile.WristRight ??= new() { Mirror = true };
            MigrateWrist();
            foreach (var wm in new[] { Profile.WristLeft, Profile.WristRight })
            {
                wm.Pages ??= new();
                foreach (var (k, w) in wm.Pages)
                    w.Name = k;
            }
            EnsureWristPages();
            currentFile = path;
            dirty = false;
            NoteFileTime();
            Text = "OpenXR Companion - Profile Editor - " + Path.GetFileName(path);
            BuildTree();
            var err = Native.Validate(File.ReadAllText(path));
            if (err == null && IsStandard(path))
                SetStatus("default.json is the standard profile (it updates with each release), and your games now use it. Changes you save go to user_defaults.json (or a new profile if you have one).", false);
            else if (err == null && string.Equals(Path.GetFileName(path), UserDefaultsFile, StringComparison.OrdinalIgnoreCase))
                SetStatus("user_defaults.json: your own defaults, and the profile your games now use. Select default.json to use the standard instead.", false);
            else
                SetStatus(err == null ? "Loaded " + path : "Loaded, but the layer reports: " + err, err != null);
        }
        catch (Exception e)
        {
            MessageBox.Show(this, "Could not read " + path + ":\n" + e.Message, "Load error");
        }
    }

    // Headless check: round-trip every profile through the model and validate with the layer.
    public static int SelfTest(string outFile)
    {
        var sb = new StringBuilder();
        int bad = 0;
        Directory.CreateDirectory(ProfilesDir);
        string def = Path.Combine(ProfilesDir, "default.json");
        if (!File.Exists(def))
            File.WriteAllText(def, Native.DefaultProfile() ?? "{}");
        var f = new MainForm();
        foreach (var file in Directory.GetFiles(ProfilesDir, "*.json"))
        {
            f.LoadFile(file);
            string json = f.Serialize();
            var err = Native.Validate(json);
            sb.AppendLine($"{Path.GetFileName(file)}: {(err == null ? "OK" : "ERROR " + err)} ({f.Profile.Bindings.Count} bindings, {f.Profile.Wheels.Count} wheels)");
            if (err != null)
                bad++;
        }
        // Controller choices applied to the built-in defaults must still pass the layer's parser.
        var builtin = Path.Combine(Path.GetTempPath(), "vro_builtin_default.json");
        File.WriteAllText(builtin, Native.DefaultProfile() ?? "{}");
        foreach (var (id, _, _) in ControllerKinds)
        {
            f.LoadFile(builtin);
            int n = ApplyControllerOpeners(f.Profile, id);
            var err = Native.Validate(f.Serialize());
            var openers = string.Join(", ", f.Profile.Bindings.Where(b => b.Action?.Type == ActionType.Wheel).Select(b => b.Input + " " + b.Gesture));
            sb.AppendLine($"controllers {id}: {(err == null && n == 2 ? "OK" : "ERROR " + err)} ({n} openers: {openers})");
            if (err != null || n != 2)
                bad++;
        }
        // Every quick choice the controller pages can make, for every controller and button, must pass the layer's parser.
        f.LoadFile(builtin);
        int quick = 0;
        foreach (var layout in ControllerLayouts.All)
            for (int hand = 0; hand < 2; hand++)
                foreach (var part in layout.PartsFor(hand).Where(p => p.IsUsable))
                    foreach (var way in part.Ways)
                    {
                        string input = ControllerLayouts.InputOf(hand, part, way);
                        f.Profile.Bindings.Add(QuickBindings.Key(input, "test", "F13"));
                        f.Profile.Bindings.Add(QuickBindings.GameButton(input, "test", hand == 0 ? "left.trigger" : "right.trigger", "test"));
                        f.Profile.Bindings.Add(QuickBindings.OffInGame(input, "test"));
                        f.Profile.Bindings.Add(QuickBindings.Blank(input, "test"));
                        f.Profile.Bindings.Add(QuickBindings.OnlyWhenHeld(QuickBindings.Key(input, "test", "F13")));
                        f.Profile.Bindings.Add(QuickBindings.DoubleFlickKey(input, "test", "F13"));
                        quick += 6;
                    }
        f.Profile.Sticks = new SticksModel { Left = new StickModel { InnerDeadzone = 0.12f, DirectionAngle = 30 }, Right = new StickModel { OuterDeadzone = 0.08f } };
        {
            var err = Native.Validate(f.Serialize());
            sb.AppendLine($"controller pages: {(err == null ? "OK" : "ERROR " + err)} ({quick} quick bindings, stick options)");
            if (err != null)
                bad++;
        }
        // ---- editor features added 2026-11-23: each must still give a profile the layer accepts
        void Check(string what, bool ok, string detail = "")
        { sb.AppendLine($"{what}: {(ok ? "OK" : "ERROR")} {detail}"); if (!ok) bad++; }
        string Valid() => Native.Validate(f.Serialize());
        {
            // every card in the three "What it does" pickers
            f.LoadFile(builtin);
            var wrist = f.Profile.WristFor(0);
            var page = wrist.Pages.Values.First();
            var wheel = f.Profile.Wheels.Values.First();
            int n = 0;
            foreach (var kind in new[] { "wrist", "wheel", "binding" })
                foreach (var code in ActionEditor.PickerCodes(kind))
                {
                    var a = ActionEditor.ActionAfterPick(kind, code, wrist);
                    if (kind == "wrist") page.Items.Add(new WheelItemModel { Label = "t", Action = a, Enabled = false });
                    else if (kind == "wheel") wheel.Items.Add(new WheelItemModel { Label = "t", Action = a, Enabled = false });
                    else f.Profile.Bindings.Add(new BindingModel { Name = "t", Input = "right.primary", Gesture = GestureKind.Press, Action = a, Enabled = false });
                    n++;
                    var err = Valid();
                    if (err != null) { Check($"picker {kind}/{code}", false, err); break; }
                    if (kind != "binding") (kind == "wrist" ? page : wheel).Items.RemoveAt((kind == "wrist" ? page : wheel).Items.Count - 1);
                }
            Check("pickers (wrist, wheel, binding)", true, $"({n} cards)");
        }
        {
            // touch holds, and "+ Add a button that opens it" for every input of every controller and every gesture
            f.LoadFile(builtin);
            var wheel = f.Profile.Wheels.Values.First();
            int n = 0;
            foreach (var layout in ControllerLayouts.All)
                for (int hand = 0; hand < 2; hand++)
                    foreach (var part in layout.PartsFor(hand).Where(p => p.IsUsable))
                        foreach (var way in part.Ways)
                        {
                            string input = ControllerLayouts.InputOf(hand, part, way);
                            f.Profile.Bindings.Add(QuickBindings.OnlyWhenHeld(QuickBindings.Key(input, "t", "F13"), touch: true));
                            foreach (var g in WheelPageEditor.OpenerGestures)
                                f.Profile.Bindings.Add(WheelPageEditor.Opener(wheel, input, g));
                            n += 1 + WheelPageEditor.OpenerGestures.Length;
                        }
            var err = Valid();
            Check("touch holds + wheel openers", err == null, $"({n} bindings) {err}");
            var touch = QuickBindings.OnlyWhenHeld(QuickBindings.Key("left.thumbrest", "t", "F13"), touch: true);
            Check("touch hold hides nothing", touch.Block == "none" && !touch.TapPassthrough && touch.Gesture == GestureKind.Hold);
        }
        {
            // duplicate = a separate copy; sort puts switched-on first, keeping their order
            f.LoadFile(builtin);
            var b = f.Profile.Bindings.First();
            var c = ItemLists.Clone(b);
            c.Action.Keys = "changed"; c.Name = "copy";
            Check("duplicate binding is a separate copy", b.Name != "copy" && b.Action?.Keys != "changed");
            var page = f.Profile.WristFor(0).Pages.Values.First();
            var pc = ItemLists.Clone(page);
            pc.Items.Clear();
            Check("duplicate page is a separate copy", page.Items.Count > 0);
            var items = new List<WheelItemModel> { new() { Label = "a", Enabled = false }, new() { Label = "b" }, new() { Label = "c", Enabled = false }, new() { Label = "d" } };
            int keep = ItemLists.Sort;
            ItemLists.Sort = 1;
            string order = string.Concat(ItemLists.Sorted(items).Select(i => i.Label));
            ItemLists.Sort = 0;
            string place = string.Concat(ItemLists.Sorted(items).Select(i => i.Label));
            ItemLists.Sort = keep;
            Check("sort on first / by place", order == "bdac" && place == "abcd", order + " " + place);
        }
        sb.AppendLine("--- round-trip of default ---");
        f.LoadFile(def);
        sb.AppendLine(f.Serialize());
        File.WriteAllText(outFile, sb.ToString());
        return bad;
    }

    // Older profiles used a wheel as the wrist panel. Move it (a copy if something else opens it) into the panel's own pages.
    // A panel's first page: named after the hand, with example buttons that start switched off.
    static WheelModel DefaultPage(int h)
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

    void EnsureWristPages()
    {
        for (int h = 0; h < 2; h++)
        {
            var wm = Profile.WristFor(h);
            if (wm.Pages.Count > 0)
                continue;
            var page = DefaultPage(h);
            wm.Pages[page.Name] = page;
            wm.Start = page.Name;
        }
    }

    // Older profiles: one "wrist_panel" (with a hand), possibly pointing at a wheel. Move it to wrist_left / wrist_right.
    void MigrateWrist()
    {
        var old = Profile.LegacyWrist;
        Profile.LegacyWrist = null;
        if (old == null)
            return;
        int h = (old.LegacyHand ?? "left").ToLowerInvariant() == "right" ? 1 : 0;
        old.LegacyHand = null;
        old.Mirror = false;
        old.Pages ??= new();
        if (old.Pages.Count == 0 && old.LegacyWheel != null && Profile.Wheels.TryGetValue(old.LegacyWheel, out var w))
        {
            string legacy = old.LegacyWheel;
            bool usedElsewhere = Profile.Bindings.Any(b => b.Action?.Type == ActionType.Wheel && b.Action.Wheel == legacy)
                || Profile.Wheels.Values.Any(o => o.Items.Any(i => i.Action?.Type == ActionType.Wheel && i.Action.Wheel == legacy));
            var page = usedElsewhere ? Clone(w) : w;
            page.Name = legacy;
            old.Pages[legacy] = page;
            old.Start = legacy;
            if (!usedElsewhere)
                Profile.Wheels.Remove(legacy);
        }
        old.LegacyWheel = null;
        if (h == 0)
            Profile.WristLeft = old;
        else
        { Profile.WristRight = old; }
    }

    string Serialize()
    {
        var wheels = new Dictionary<string, WheelModel>();
        foreach (var w in Profile.Wheels.Values)
            wheels[w.Name] = w;
        Profile.Wheels = wheels;
        foreach (var wm in new[] { Profile.WristLeft, Profile.WristRight })
        {
            var pages = new Dictionary<string, WheelModel>();
            foreach (var w in wm.Pages.Values)
                pages[w.Name] = w;
            wm.Pages = pages;
        }
        return JsonSerializer.Serialize(Profile, JsonOpts);
    }

    bool Save()
    {
        if (Profile == null)
            return false;
        var names = Profile.Wheels.Values.GroupBy(w => w.Name).Where(g => g.Count() > 1).Select(g => g.Key).ToList();
        if (names.Count > 0)
        { MessageBox.Show(this, "Two wheels share the name " + names[0] + "."); return false; }
        var pnames = Profile.WristLeft.Pages.Values.Concat(Profile.WristRight.Pages.Values).GroupBy(w => w.Name).Where(g => g.Count() > 1).Select(g => g.Key).ToList();
        if (pnames.Count > 0)
        { MessageBox.Show(this, "Two wrist pages share the name " + pnames[0] + "."); return false; }
        string json = Serialize();
        var err = Native.Validate(json);
        if (err != null && MessageBox.Show(this, "The layer would reject this profile:\n\n" + err + "\n\nSave anyway? (the game keeps the last good version until fixed)", "Problem", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes)
            return false;
        bool forked = false;
        if (IsStandard(currentFile)) // never into the standard profile: your changes become user_defaults.json
        {
            string user = Path.Combine(ProfilesDir, UserDefaultsFile);
            if (File.Exists(user)) // never replace the user's own defaults: a new profile instead
            {
                string name = "user_defaults 2.json";
                for (int i = 3; File.Exists(Path.Combine(ProfilesDir, name)); i++)
                    name = "user_defaults " + i + ".json";
                if (MessageBox.Show(this,
                        "You changed default.json, the standard profile. It stays as it is, and you already have your own user_defaults.json, which won't be replaced.\n\n" +
                        "These changes will be saved as a new profile: " + name + "\n\n" +
                        "The new profile becomes the selected one, so your games use it. Pick user_defaults.json in the profile list to go back to it.",
                        "Saving changes to the standard profile", MessageBoxButtons.OKCancel, MessageBoxIcon.Warning) != DialogResult.OK)
                    return false;
                user = Path.Combine(ProfilesDir, name);
            }
            currentFile = user;
            forked = true;
        }
        AtomicFile.WriteAllText(currentFile, json);
        NoteFileTime();
        dirty = false;
        if (forked)
        {
            string saved = Path.GetFileName(currentFile);
            ActiveProfile = saved;
            bool isUser = string.Equals(saved, UserDefaultsFile, StringComparison.OrdinalIgnoreCase);
            loading = true;
            if (!profileBox.Items.Contains(saved))
                profileBox.Items.Insert(Math.Min(isUser ? 1 : profileBox.Items.Count, profileBox.Items.Count), saved);
            profileBox.SelectedItem = saved;
            loading = false;
            Text = "OpenXR Companion - Profile Editor - " + saved;
            SetStatus(isUser ? "Saved as user_defaults.json (your own defaults; default.json stays the standard). A running game switches to it within a second."
                             : "Saved as the new profile " + saved + ", now the selected profile that games use. default.json and your user_defaults.json are unchanged.", false);
            BuildTree();
            return true;
        }
        Text = "OpenXR Companion - Profile Editor - " + Path.GetFileName(currentFile);
        SetStatus(err == null ? "Saved. A running game reloads it within a second." : "Saved with errors: " + err, err != null);
        BuildTree();
        return true;
    }

    void NewProfile()
    {
        if (!ConfirmDiscard())
            return;
        var seen = Directory.Exists(Path.Combine(DataDir, "logs"))
            ? Directory.GetFiles(Path.Combine(DataDir, "logs"), "*.log").Select(Path.GetFileNameWithoutExtension).ToArray() : Array.Empty<string>();
        using var dlg = new NewProfileDialog(seen);
        ThemeWalker.Apply(dlg);
        dlg.HandleCreated += (s, e) => ThemeWalker.TitleBar(dlg, Theme.IsDark);
        if (dlg.ShowDialog(this) != DialogResult.OK || string.IsNullOrWhiteSpace(dlg.ExeName))
            return;
        string name = dlg.ExeName.Trim().ToLowerInvariant();
        if (name.EndsWith(".exe"))
            name = name[..^4];
        string path = Path.Combine(ProfilesDir, name + ".json");
        if (File.Exists(path))
        { MessageBox.Show(this, "A profile for " + name + " already exists."); RefreshProfileList(name + ".json"); return; }
        File.Copy(Path.Combine(ProfilesDir, dlg.CopyFrom ?? StartProfile), path);
        RefreshProfileList(name + ".json");
    }

    void RenameProfile()
    {
        string file = Path.GetFileName(currentFile);
        if (IsStandard(currentFile) || string.Equals(file, UserDefaultsFile, StringComparison.OrdinalIgnoreCase))
        { MessageBox.Show(this, file + " has a fixed name (it is what games without their own profile use). Make a copy with 'New game profile...' instead."); return; }
        if (!ConfirmDiscard())
            return;
        var box = new TextBox { Width = 320, Text = Path.GetFileNameWithoutExtension(file) };
        string answer = QuickBindingDialogs.Ask(this, "Rename profile",
            "New name for " + file + ".\n\nA game profile is named after the game's program (bannerlord.json is used by Bannerlord.exe): after renaming, it applies to the program with the new name.",
            box, () => box.Text);
        if (string.IsNullOrWhiteSpace(answer))
            return;
        string name = answer.Trim().ToLowerInvariant();
        if (name.EndsWith(".json")) name = name[..^5];
        if (name.EndsWith(".exe")) name = name[..^4];
        if (name.Length == 0 || name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
        { MessageBox.Show(this, "That name can't be used for a file."); return; }
        string to = Path.Combine(ProfilesDir, name + ".json");
        if (string.Equals(to, currentFile, StringComparison.OrdinalIgnoreCase))
            return;
        if (File.Exists(to))
        { MessageBox.Show(this, "A profile called " + name + " already exists."); return; }
        bool wasActive = string.Equals(ActiveProfile, file, StringComparison.OrdinalIgnoreCase);
        File.Move(currentFile, to);
        if (wasActive)
            ActiveProfile = name + ".json";
        dirty = false;
        RefreshProfileList(name + ".json");
    }

    void DeleteProfile()
    {
        if (IsStandard(currentFile))
        { MessageBox.Show(this, "default.json is the standard profile and can't be deleted."); return; }
        bool userDefs = string.Equals(Path.GetFileName(currentFile), UserDefaultsFile, StringComparison.OrdinalIgnoreCase);
        if (MessageBox.Show(this, userDefs
                ? "Delete user_defaults.json? Your games switch to the standard default.json."
                : "Delete " + Path.GetFileName(currentFile) + "? Your games switch to " + StartProfile + ".", "Delete", MessageBoxButtons.YesNo) != DialogResult.Yes)
            return;
        File.Delete(currentFile);
        dirty = false;
        RefreshProfileList(ActiveProfile);
    }
}
