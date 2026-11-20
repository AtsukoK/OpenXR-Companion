namespace OpenXRCompanionEditor;

static class Program
{
    [STAThread]
    static int Main(string[] args)
    {
        ApplicationConfiguration.Initialize();
        Theme.Set(Environment.GetEnvironmentVariable("VRO_THEME") is string th ? th == "dark" : ThemeMode.IsDark); // before any window is built: some parts take their colours when made
        if (args.Length == 2 && args[0] == "--tourshots")
        { MainForm.TourShots(args[1]); return 0; }
        if (args.Length == 2 && args[0] == "--shots")
        { MainForm.Shots(args[1]); return 0; }
        if (args.Length == 3 && args[0] == "--perf")
        { MainForm.Perf(args[1], args[2]); return 0; }
        if (args.Length == 2 && args[0] == "--selftest")
            return MainForm.SelfTest(args[1]);
        if (args.Length == 2 && args[0] == "--updatecheck") // writes this version and the latest GitHub release to a file
        {
            var r = MainForm.LatestReleaseAsync().GetAwaiter().GetResult();
            File.WriteAllText(args[1], "this=" + MainForm.AppVersion.ToString(3) + " latest=" + (r == null ? "unknown" : r.Value.tag) +
                " update=" + (r != null && r.Value.ver > MainForm.AppVersion));
            return 0;
        }
        if (args.Length == 2 && args[0] == "--update-now") // updates this folder from the latest release without asking (tests, scripts)
        {
            try
            {
                var why = Updater.CantUpdateReason();
                if (why != null)
                { File.WriteAllText(args[1], "refused: " + why); return 2; }
                var r = Updater.LatestAsync().GetAwaiter().GetResult();
                Updater.RunAsync(r, (p, t) => { }).GetAwaiter().GetResult();
                File.WriteAllText(args[1], "updated to " + r.Tag + " from v" + MainForm.AppVersion.ToString(3));
                return 0;
            }
            catch (Exception ex) { File.WriteAllText(args[1], "failed: " + ex.Message); return 1; }
        }
        Updater.CleanUp();
        WheelScrollFix.Install();
        Application.Run(new MainForm());
        return 0;
    }
}
