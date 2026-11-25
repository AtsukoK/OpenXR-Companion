using System.Text.Json;
using Avalonia.Media;
using Avalonia.Threading;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// The "may need your attention" checks (core BindingChecks) for the open profile, minus the ones marked "Looks right".
// "Looks right" is kept per profile in attention_ok.json in the data folder; an edit to the binding asks again.
public static class Attention
{
    static bool? vigem;
    static string ackFile => Path.Combine(Shell.Store.DataDir, "attention_ok.json");
    static Dictionary<string, HashSet<string>> acks;

    public static List<BindingIssue> Issues()
    {
        var p = Shell.Store?.Profile;
        if (p == null) return new();
        vigem ??= LayerTools.ViGEmBusInstalled();
        var ctx = new CheckContext(ControllerLayouts.ById(EditorApi.PictureChoice(p)), vigem.Value);
        var mine = Acks();
        return BindingChecks.Check(p, ctx).Where(i => !mine.Contains(i.Key)).ToList();
    }

    public static List<BindingIssue> For(BindingModel b) => Issues().Where(i => ReferenceEquals(i.Binding, b)).ToList();

    public static void LooksRight(BindingIssue i)
    {
        Acks().Add(i.Key);
        try { File.WriteAllText(ackFile, JsonSerializer.Serialize(acks, new JsonSerializerOptions { WriteIndented = true })); } catch { }
    }

    static HashSet<string> Acks()
    {
        if (acks == null)
        {
            try { acks = JsonSerializer.Deserialize<Dictionary<string, HashSet<string>>>(File.ReadAllText(ackFile)); } catch { }
            acks ??= new();
        }
        string name = Shell.Store.CurrentName ?? "";
        if (!acks.TryGetValue(name, out var set))
            acks[name] = set = new HashSet<string>();
        return set;
    }

    // One slow, gentle pulse (not flashing). Every glowing thing gets a brush of its own (never one brush shared by
    // many controls while it changes); the timer only changes brushes still in use and forgets the rest.
    static readonly List<(WeakReference<SolidColorBrush> brush, bool fill)> glows = new();
    static DispatcherTimer pulse;
    static readonly DateTime started = DateTime.Now;
    static double Level => 0.5 + 0.5 * Math.Sin((DateTime.Now - started).TotalSeconds * Math.PI / 1.2); // 2.4 s a breath
    static Color ColorOf(bool fill, double k) => fill ? Color.FromArgb((byte)(18 + 40 * k), 0xF2, 0xC9, 0x4C)
                                                      : Color.FromArgb((byte)(110 + 145 * k), 0xF2, 0xC9, 0x4C);
    public static SolidColorBrush GlowBorder() => NewGlow(false);
    public static SolidColorBrush GlowFill() => NewGlow(true);
    static SolidColorBrush NewGlow(bool fill)
    {
        var b = new SolidColorBrush(ColorOf(fill, Level));
        glows.Add((new WeakReference<SolidColorBrush>(b), fill));
        if (pulse == null)
        {
            pulse = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(80) };
            pulse.Tick += (s, e) =>
            {
                double k = Level;
                glows.RemoveAll(g => !g.brush.TryGetTarget(out _));
                foreach (var (w, f) in glows)
                    if (w.TryGetTarget(out var br)) br.Color = ColorOf(f, k);
                if (glows.Count == 0) { pulse.Stop(); pulse = null; }
            };
            pulse.Start();
        }
        return b;
    }
}
