using System.Runtime.InteropServices;
using System.Text;

namespace OpenXRCompanionEditor;

// The layer's own checks (profile, key names) and its built-in default profile, from OpenXRCompanion.dll.
public static class Native
{
    [DllImport("OpenXRCompanion.dll", CallingConvention = CallingConvention.Cdecl)]
    static extern int vro_validate([MarshalAs(UnmanagedType.LPUTF8Str)] string text, byte[] err, int errLen);
    [DllImport("OpenXRCompanion.dll", CallingConvention = CallingConvention.Cdecl)]
    static extern IntPtr vro_default_profile();

    public static string Validate(string json)
    {
        try
        {
            var buf = new byte[1024];
            if (vro_validate(json, buf, buf.Length) != 0)
                return null;
            return Encoding.UTF8.GetString(buf, 0, Array.IndexOf(buf, (byte)0) is int n && n >= 0 ? n : buf.Length);
        }
        catch (Exception e) { return "(could not load OpenXRCompanion.dll to check: " + e.Message + ")"; }
    }
    [DllImport("OpenXRCompanion.dll", CallingConvention = CallingConvention.Cdecl)]
    static extern int vro_check_keys([MarshalAs(UnmanagedType.LPUTF8Str)] string text, int seq, byte[] err, int errLen);

    /// <summary>null = fine (or cannot check), otherwise the layer's reason.</summary>
    public static string CheckKeys(string text, bool sequence)
    {
        if (string.IsNullOrWhiteSpace(text))
            return null;
        try
        {
            var buf = new byte[512];
            if (vro_check_keys(text, sequence ? 1 : 0, buf, buf.Length) != 0)
                return null;
            int n = Array.IndexOf(buf, (byte)0);
            return Encoding.UTF8.GetString(buf, 0, n >= 0 ? n : buf.Length);
        }
        catch { return null; }
    }
    [DllImport("OpenXRCompanion.dll", CallingConvention = CallingConvention.Cdecl)]
    static extern void vro_prepare_defaults();
    // default.json = the built-in standard (refreshed); an older changed one becomes user_defaults.json (once).
    public static void PrepareDefaults() { try { vro_prepare_defaults(); } catch { } }
    public static string DefaultProfile()
    {
        try
        { return Marshal.PtrToStringUTF8(vro_default_profile()); }
        catch { return null; }
    }
}
