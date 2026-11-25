using System.Diagnostics;
using System.Reflection;
using Microsoft.Win32;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// The toolbar's layer status, Install / Uninstall layer and the update check (WinForms MainForm.Install.cs and
// UpdateCheck.cs). Status and the update check only read (the registry, GitHub's public release list); Install and
// Uninstall run the same elevated scripts as WinForms, from the folder the editor runs from.
public static class LayerTools
{
    const string LayerJson = "XR_APILAYER_NOVENDOR_openxr_companion.json";
    static string ScriptDir => AppContext.BaseDirectory;
    static string X86Dir => Path.Combine(ScriptDir, "x86");
    static bool HasX86 => File.Exists(Path.Combine(X86Dir, LayerJson)) && File.Exists(Path.Combine(X86Dir, "OpenXRCompanion.dll"));

    // 0 = not installed, 1 = installed from this folder, 2 = installed from another folder (WinForms MainForm.LayerState)
    static int LayerState(RegistryView view, string manifest) =>
        LayerRegistry.State(view, manifest);

    // ("Layer: installed (+ 32-bit)", all fine)
    public static (string text, bool ok) Status()
    {
        int s64 = LayerState(RegistryView.Registry64, Path.Combine(ScriptDir, LayerJson));
        string text = s64 == 1 ? "Layer: installed" : s64 == 2 ? "Layer: installed from another folder" : "Layer: NOT installed";
        bool ok = s64 == 1;
        if (HasX86)
        {
            int s32 = LayerState(RegistryView.Registry32, Path.Combine(X86Dir, LayerJson));
            text += s32 == 1 ? " (+ 32-bit)" : s32 == 2 ? " (32-bit: from another folder)" : " (32-bit: not installed)";
            ok &= s32 == 1;
        }
        return (text, ok);
    }

    // Runs install.ps1 / uninstall.ps1 (and the x86 one) with one admin prompt. Null when it ran, else why not.
    public static string RunScript(string name)
    {
        string script = Path.Combine(ScriptDir, name);
        if (!File.Exists(script) || !File.Exists(Path.Combine(ScriptDir, LayerJson)) || !File.Exists(Path.Combine(ScriptDir, "OpenXRCompanion.dll")))
            return "The layer files aren't next to the editor (looked in " + ScriptDir + "). Extract the whole zip and run the editor from that folder.";
        string cmd = $"& '{script.Replace("'", "''")}'";
        string script32 = Path.Combine(X86Dir, name);
        if (HasX86 && File.Exists(script32))
            cmd += $"; & '{script32.Replace("'", "''")}'";
        try
        {
            Process.Start(new ProcessStartInfo("powershell.exe", $"-NoProfile -ExecutionPolicy Bypass -Command \"{cmd}\"") { Verb = "runas", UseShellExecute = true })?.WaitForExit();
            return null;
        }
        catch (System.ComponentModel.Win32Exception) { return "Cancelled (the admin prompt was declined)."; }
    }

    // The newer release's tag, or null (none newer, or offline). Only reads GitHub's public release list.
    public static async Task<string> NewerReleaseAsync()
    {
        try
        {
            var me = Version.Parse(AppVersionText.Split('-', '+')[0]);
            var latest = await Releases.LatestAsync(AppVersionText.Contains("-experimental"), me.ToString(3));
            return latest is { } l && l.ver > me ? l.tag : null;
        }
        catch { return null; }
    }

    // this editor's version (the csproj Version), e.g. "0.2.16-experimental+commit"
    static string AppVersionText => typeof(LayerTools).Assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion ?? "0.0.0";

    // The release the update offers (its own page, with the zip to download), else the list of releases.
    public static void OpenReleasesPage(string tag = null)
    {
        string url = "https://github.com/AtsukoK/OpenXR-Companion/releases" + (string.IsNullOrWhiteSpace(tag) ? "" : "/tag/" + Uri.EscapeDataString(tag));
        try { Process.Start(new ProcessStartInfo(url) { UseShellExecute = true }); }
        catch (System.ComponentModel.Win32Exception) { }
    }
}
