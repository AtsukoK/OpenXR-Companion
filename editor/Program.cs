using Avalonia;

namespace OverviewPrototype;

static class Program
{
    public static bool TestRun; // started with a --test option: don't move/resize the window to the screen
    [STAThread]
    public static int Main(string[] args)
    {
        TestRun = args.Length > 0 && args[0].StartsWith("--") && args[0] != "--data" && args[0] != "--updated";
        // --printversion <file>: write this build's version and exit (the update test starts the swapped-in editor with it)
        if (args.Length == 2 && args[0] == "--printversion")
        {
            File.WriteAllText(args[1], Updater.AppVersionText);
            return 0;
        }
        // --updatetest <release zip> <result.txt>: install that zip over THIS folder (run it from a throwaway copy of a
        // release folder), check the files, then start the swapped-in editor. No GitHub, no window.
        // --presettest <out.txt>: every controller preset applied to the built-in default; lists the bindings it rewrote
        if (args.Length == 2 && args[0] == "--presettest")
        {
            var lines = new List<string>();
            foreach (var (id, label, how) in EditorApi.ControllerKinds())
            {
                var p = OpenXRCompanionEditor.ProfileIO.FromText(OpenXRCompanionEditor.Native.DefaultProfile());
                int n = OpenXRCompanionEditor.ControllerPresets.Apply(p, id);
                lines.Add($"{id}: {n} rewritten; detected afterwards as '{EditorApi.CurrentControllerKind(p)}'");
                foreach (var b in p.Bindings.Where(b => b.Action?.Type == "wheel"))
                    lines.Add($"    {b.Input} {b.Gesture} -> wheel '{b.Action.Wheel}'  ({b.Name})");
            }
            File.WriteAllLines(args[1], lines);
            return 0;
        }
        if (args.Length == 3 && args[0] == "--updatetest")
            return UpdateTest.Run(args[1], args[2]);
        if (args.Length == 0 || args[0] == "--updated")
            Updater.CleanUp(); // *.old files a past update left
        // --reference <data folder> <out.txt>: what the data side produces, to compare after restructuring it (reads only).
        if (args.Length == 3 && args[0] == "--reference")
        {
            ReferenceDump.Write(args[1], args[2]);
            return 0;
        }
        // --profiletest <copy of the data folder> <result file>: new game profile, rename, delete, on a copy (refuses the real one).
        if (args.Length == 3 && args[0] == "--profiletest"
            && !string.Equals(Path.GetFullPath(args[1]).TrimEnd('\\', '/'), Path.GetFullPath(ProfileStore.RealDataDir).TrimEnd('\\', '/'), StringComparison.OrdinalIgnoreCase))
        {
            var store = new ProfileStore(args[1], true);
            store.Load(store.ActiveName);
            var lines = new List<string>();
            string Active() => File.ReadAllText(Path.Combine(args[1], "active_profile.txt")).Trim();
            string Files() => string.Join(", ", store.ProfileNames());
            lines.Add("start: active=" + Active() + "; files: " + Files());
            lines.Add("new testgame.exe from default.json: " + (store.NewProfile("TestGame.exe", "default.json") ?? "ok") + "; current=" + store.CurrentName + "; active=" + Active());
            lines.Add("  same text as default.json: " + (File.ReadAllText(Path.Combine(store.ProfilesDir, "testgame.json")) == File.ReadAllText(Path.Combine(store.ProfilesDir, "default.json"))));
            lines.Add("new testgame again: " + (store.NewProfile("testgame", "default.json") ?? "ok"));
            lines.Add("rename to testgame2: " + (store.Rename("testgame2") ?? "ok") + "; current=" + store.CurrentName + "; active=" + Active() + "; files: " + Files());
            lines.Add("delete: " + (store.Delete() ?? "ok") + "; current=" + store.CurrentName + "; files: " + Files());
            store.Load("default.json");
            lines.Add("rename default.json: " + (store.Rename("x") ?? "ok"));
            lines.Add("delete default.json: " + (store.Delete() ?? "ok"));
            File.WriteAllLines(args[2], lines);
            return 0;
        }
        // --roundtrip <data folder> <result file>: load and re-serialize every profile (in memory, nothing written to the
        // profiles) and compare with the file on disk: saving in the port must write what the WinForms editor wrote.
        // Since the split the port no longer contains the WinForms editor: --roundtrip <data> <result> <reference.txt> compares
        // with a --reference dump made before the split (reference_before_split.txt, proven equal to WinForms), and
        // --reference + a text diff compares everything else. The WinForms editor's own --selftest covers its side.
        if (args.Length == 4 && args[0] == "--roundtrip")
        {
            var store = new ProfileStore(args[1], false);
            string reference = File.ReadAllText(args[3]).Replace("\r\n", "\n");
            var lines = new List<string>();
            foreach (var name in store.ProfileNames())
            {
                store.Load(name);
                string mine = store.Serialize().Replace("\r\n", "\n");
                int at = reference.IndexOf("## profile " + name + "\n");
                string theirs = at < 0 ? null : reference.Substring(at + ("## profile " + name + "\n").Length, Math.Min(mine.Length, reference.Length - at - ("## profile " + name + "\n").Length));
                lines.Add((mine == theirs ? "same     " : "DIFFERENT") + "  " + name);
            }
            File.WriteAllLines(args[2], lines);
            return 0;
        }
        AppBuilder.Configure<App>().UsePlatformDetect().LogToTrace().StartWithClassicDesktopLifetime(args);
        return 0;
    }
}
