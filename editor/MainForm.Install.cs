using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Win32;

namespace OpenXRCompanionEditor;

// Installing and removing the OpenXR layer.
public partial class MainForm
{
    static string ScriptDir => AppContext.BaseDirectory;

    static string X86Dir => Path.Combine(ScriptDir, "x86");
    static bool HasX86 => File.Exists(Path.Combine(X86Dir, LayerJson)) && File.Exists(Path.Combine(X86Dir, "OpenXRCompanion.dll"));

    // 0 = not installed, 1 = installed from this folder, 2 = installed from another folder
    static int LayerState(RegistryView view, string manifest)
    {
        bool installed = false, other = false;
        using (var hk = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, view))
        using (var k = hk.OpenSubKey(ImplicitKey))
            if (k != null)
                foreach (var v in k.GetValueNames())
                    if (v.EndsWith(LayerJson, StringComparison.OrdinalIgnoreCase))
                    {
                        if (string.Equals(v, manifest, StringComparison.OrdinalIgnoreCase) && k.GetValue(v) is int i && i == 0) installed = true;
                        else other = true;
                    }
        return installed ? 1 : other ? 2 : 0;
    }

    void UpdateLayerStatus()
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
        layerStatus.Text = "  " + text;
        layerStatus.ToolTipText = "64-bit games use the main layer. 32-bit games (Fallout: New Vegas / Fallout 3 with TTWVR) use the 32-bit one in the x86 folder. Install layer sets up both.";
        layerStatus.ForeColor = ok ? (Theme.IsDark ? Color.FromArgb(110, 205, 140) : Color.ForestGreen) : (Theme.IsDark ? Color.FromArgb(255, 125, 115) : Color.Firebrick);
    }

    void RunScript(string name)
    {
        string script = Path.Combine(ScriptDir, name);
        if (!File.Exists(script) || !File.Exists(Path.Combine(ScriptDir, LayerJson)) || !File.Exists(Path.Combine(ScriptDir, "OpenXRCompanion.dll")))
        {
            MessageBox.Show(this, "The layer files aren't next to the editor, so it can't install the layer.\n\n" +
                "Looked in: " + ScriptDir + "\n\n" +
                "Extract the WHOLE zip first (right-click it > Extract All), then run OpenXRCompanionEditor.exe from the extracted folder. " +
                "It needs install.ps1, OpenXRCompanion.dll and " + LayerJson + " in the same folder. " +
                "Running the exe straight from inside the zip doesn't work.", "Layer files not found", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }
        try
        {
            // One admin prompt for both: the 64-bit layer, then the 32-bit one (x86 folder) if it shipped.
            string cmd = $"& '{script.Replace("'", "''")}'";
            string script32 = Path.Combine(X86Dir, name);
            if (HasX86 && File.Exists(script32)) cmd += $"; & '{script32.Replace("'", "''")}'";
            var p = Process.Start(new ProcessStartInfo("powershell.exe", $"-NoProfile -ExecutionPolicy Bypass -Command \"{cmd}\"") { Verb = "runas", UseShellExecute = true });
            p?.WaitForExit();
        }
        catch (Exception e) { MessageBox.Show(this, e.Message); }
        UpdateLayerStatus();
    }
}
