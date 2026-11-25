namespace OpenXRCompanionEditor;

public static class Paths
{
    // %LOCALAPPDATA%/OpenXRCompanion (moved from the older names on first use)
    public static readonly string DataDir = MigrateDataDir();
    public static string ProfilesDir => Path.Combine(DataDir, "profiles");
    // First run after the rename from VR Radial Overlay: copy the old settings folder across.
    static string MigrateDataDir()
    {
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var dir = Path.Combine(local, "OpenXRCompanion");
        // OpenXR Hand Menus first, before that VR Radial Overlay
        var old = Path.Combine(local, "OpenXRHandMenus");
        if (!Directory.Exists(old))
            old = Path.Combine(local, "VRRadialOverlay");
        try
        {
            if (!Directory.Exists(dir) && Directory.Exists(old))
                foreach (var f in Directory.GetFiles(old, "*", SearchOption.AllDirectories))
                {
                    var t = Path.Combine(dir, Path.GetRelativePath(old, f));
                    Directory.CreateDirectory(Path.GetDirectoryName(t));
                    File.Copy(f, t, false);
                }
        }
        catch { }
        return dir;
    }
}
