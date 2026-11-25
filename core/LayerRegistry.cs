using Microsoft.Win32;

namespace OpenXRCompanionEditor;

// Is the OpenXR layer registered (read-only look at the registry)?
public static class LayerRegistry
{
    public const string LayerJson = "XR_APILAYER_NOVENDOR_openxr_companion.json";
    public const string ImplicitKey = @"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit";

    // 0 = not installed, 1 = installed from this folder, 2 = installed from another folder
    public static int State(RegistryView view, string manifest)
    {
        bool installed = false, other = false;
        using (var hk = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, view))
        using (var k = hk.OpenSubKey(ImplicitKey))
            if (k != null)
                foreach (var v in k.GetValueNames())
                    if (v.EndsWith(LayerJson, StringComparison.OrdinalIgnoreCase))
                    {
                        if (string.Equals(v, manifest, StringComparison.OrdinalIgnoreCase) && k.GetValue(v) is int i && i == 0)
                            installed = true;
                        else
                            other = true;
                    }
        return installed ? 1 : other ? 2 : 0;
    }
}
