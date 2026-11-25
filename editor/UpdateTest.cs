namespace OverviewPrototype;

// --updatetest: Updater.Install on a local zip, over the folder this exe runs from, then checks what it did.
static class UpdateTest
{
    public static int Run(string zip, string result)
    {
        var lines = new List<string>();
        try
        {
            string why = Updater.CantUpdateReason();
            lines.Add("can update here: " + (why ?? "yes"));
            if (why != null) { File.WriteAllLines(result, lines); return 1; }
            string dir = Updater.InstallDir;
            var before = Directory.GetFiles(dir, "*", SearchOption.AllDirectories).ToDictionary(f => Path.GetRelativePath(dir, f), File.ReadAllBytes);
            string exe = Updater.Install(zip, (p, t) => lines.Add("  progress: " + t));
            // every file in the zip is now in the folder, byte for byte
            using var z = System.IO.Compression.ZipFile.OpenRead(zip);
            int same = 0, diff = 0;
            foreach (var en in z.Entries.Where(en => !en.FullName.EndsWith("/")))
            {
                string name = en.FullName.Replace('\\', '/'); // Windows PowerShell zips use backslashes
                string rel = name.StartsWith("OpenXRCompanion/") ? name["OpenXRCompanion/".Length..] : name;
                using var ms = new MemoryStream();
                using (var s = en.Open()) s.CopyTo(ms);
                string target = Path.Combine(dir, rel.Replace('/', Path.DirectorySeparatorChar));
                bool keptOlderEditor = string.Equals(Path.GetFileName(target), Path.GetFileName(Environment.ProcessPath), StringComparison.OrdinalIgnoreCase)
                    && ms.ToArray().AsSpan().IndexOf(System.Text.Encoding.UTF8.GetBytes("Avalonia.Base.dll")) < 0;
                if (keptOlderEditor) { lines.Add("  kept this editor (the zip's " + rel + " is the older WinForms editor)"); same++; }
                else if (File.Exists(target) && File.ReadAllBytes(target).AsSpan().SequenceEqual(ms.ToArray())) same++;
                else { diff++; lines.Add("  NOT INSTALLED: " + rel); }
            }
            lines.Add($"files from the zip in place: {same}/{same + diff}");
            int backed = before.Keys.Count(k => File.Exists(Path.Combine(dir, "_previous_version", k)) &&
                File.ReadAllBytes(Path.Combine(dir, "_previous_version", k)).AsSpan().SequenceEqual(before[k]));
            lines.Add($"old files kept in _previous_version: {backed}");
            // the editor that would restart starts and runs (after its own exe was swapped)
            string outFile = Path.Combine(Path.GetTempPath(), "vro_printversion.txt");
            try { File.Delete(outFile); } catch { }
            int code = Updater.StartAndWait(exe, "--printversion \"" + outFile + "\"");
            lines.Add("restarted editor: exit " + code + ", version " + (File.Exists(outFile) ? File.ReadAllText(outFile) : "(none)"));
            lines.Add(diff == 0 && code == 0 && File.Exists(outFile) ? "RESULT ok" : "RESULT FAILED");
        }
        catch (Exception ex) { lines.Add("RESULT FAILED: " + ex); }
        File.WriteAllLines(result, lines);
        return 0;
    }
}
