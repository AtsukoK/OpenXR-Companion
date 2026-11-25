using OpenXRCompanionEditor;

namespace OverviewPrototype;

// The data side's helpers the pages use, in one place. Since the split (2026-11-24) these are plain calls into the
// shared core library (OpenXRCompanion.Core), no longer reflection into the WinForms editor.
public static class EditorApi
{
    // "Presses F5", "Opens wheel 'right'" ... : what an action does, in words.
    public static string Plain(ActionModel a) => ActionText.Plain(a);

    // The controller picked for the pictures (Quest 3, Index ...): its layout id. Reads the app-wide choice only.
    public static string PictureChoice(ProfileModel p) => ControllerPictureChoice.Load(p);

    // null when the key text is fine, else the unknown part ("Ctrl+Q", or macro steps when sequence).
    public static string CheckKeys(string text, bool sequence) => Native.CheckKeys(text, sequence);

    // The "What should it do?" cards: kind "binding", "wheel" or "wrist".
    public static IReadOnlyList<(string code, PickOption option)> ActionOptions(string kind) =>
        kind == "wrist" ? OpenXRCompanionEditor.ActionOptions.Wrist : kind == "wheel" ? OpenXRCompanionEditor.ActionOptions.Wheel : OpenXRCompanionEditor.ActionOptions.Binding;

    // Layer names used anywhere in the profile (the WinForms LayerConv list).
    public static IEnumerable<string> LayerNames(ProfileModel p)
    {
        var s = new SortedSet<string>();
        foreach (var b in p.Bindings)
        {
            if (!string.IsNullOrEmpty(b.Action?.Layer))
                s.Add(b.Action.Layer);
            foreach (var l in (b.Layers ?? "").Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                s.Add(l);
        }
        return s;
    }

    // Where a wheel appears.
    public static readonly Choice[] Anchors = Choices.Anchors;

    // The controller presets ("touch", "index", "wmr"...): id, label, what it sets.
    public static IReadOnlyList<(string id, string label, string how)> ControllerKinds() => ControllerPresets.Kinds;

    // The preset this profile looks like it uses now (or the one picked last time), else null.
    public static string CurrentControllerKind(ProfileModel p) => ControllerPresets.CurrentKind(p);
}
