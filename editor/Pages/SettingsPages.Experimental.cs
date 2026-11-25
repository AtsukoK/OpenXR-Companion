using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Settings > Experimental (WinForms SettingsEditor.BuildExperimental): head smoothing. App-wide, in head_smoothing.json
// shared with the layer, written straight away (never marks the profile unsaved), and only when the user changes a value.
// Listed in Settings only when "Show experimental features" is ticked on the Advanced page, or while it is on.
public partial class SettingsPageView
{
    public static bool ExperimentalListed => HeadSmoothingFile.ShowPage || HeadSmoothingFile.On;

    Form Experimental()
    {
        var f = new Form { LabelWidth = 160 };
        f.AddRow("", new TextBlock
        {
            Foreground = Brush.Parse("#E8603F"), FontWeight = FontWeight.Bold, TextWrapping = TextWrapping.Wrap, MaxWidth = 600,
            Text = "⚠ ⚠ MOTION SICKNESS WARNING ⚠ ⚠\n" +
                   "Head smoothing makes the game's view lag slightly behind your real head. In VR that mismatch can cause nausea, dizziness or headaches, quickly and without warning.\n" +
                   "• Start with \"For recording\" and a low strength.\n" +
                   "• Take the headset off at the first sign of feeling unwell, and don't use it if you are prone to motion sickness.\n" +
                   "• It switches itself off whenever a game starts (unless you tick Keep it on below).",
        });
        var on = new CheckBox { Content = "⚠ Smooth the head (off by default)", IsChecked = HeadSmoothingFile.On };
        on.IsCheckedChanged += async (s, e) =>
        {
            bool v = on.IsChecked == true;
            if (v == HeadSmoothingFile.On) return;
            if (v && !await Dialogs.Confirm(this, "⚠ Experimental: head smoothing",
                    "Head smoothing can cause motion sickness: nausea, dizziness or headaches, sometimes quickly.\n\n" +
                    "Start with \"For recording\" and a low strength, and take the headset off at the first sign of feeling unwell.\n\nTurn it on?", "Turn it on"))
            { on.IsChecked = false; return; }
            HeadSmoothingFile.On = v;
        };
        f.AddRow("", on, "Steadies the direction the GAME looks from, like a camera stabiliser, so small head shakes don't show. Real turns still follow. Position isn't changed. For using the headset as a camera.");
        var mode = new ChoiceCombo(new Choice[] { new("recording", "For recording (the headset view stays normal)"), new("headset", "⚠ In the headset too (highest sickness risk)") }, 360) { Code = HeadSmoothingFile.Mode };
        mode.SelectionChanged += (s, e) => { if (mode.Code != null && mode.Code != HeadSmoothingFile.Mode) HeadSmoothingFile.Mode = mode.Code; };
        f.AddRow("Where", mode, "For recording: the game's own desktop view and recordings are steadied; the headset corrects back to your real head, so the world still feels solid (the picture's edge can show briefly during shakes). In the headset too: you see the steadied picture yourself; the world moves a little with your head. Much more likely to make you feel sick.");
        f.AddRow("", MCheck("Keep it on when a game starts", () => HeadSmoothingFile.KeepOn, v => HeadSmoothingFile.KeepOn = v),
            "Off (the default): head smoothing switches itself off every time a game starts, so you never put the headset on with it already running. Turn it on again here while the game runs.");
        f.AddRow("Strength", MNum(0, 100, 0.5m, 2, () => HeadSmoothingFile.Strength, v => HeadSmoothingFile.Strength = v),
            "How heavy the camera is. It smooths every movement the same way, easing into and out of turns like a weighted camera: about 0.12 s of lag per step, so 0.5 = light, 2 = smooth, 5 = heavy (0.6 s), 10 = very heavy (1.2 s), up to 100 (12 s, for slow drifting shots). ⚠ In the headset, anything above about 1 is very likely to make you sick; use For recording.");
        f.AddRow("", new TextBlock { Classes = { "desc" }, Text = "These apply straight away, in every game and profile (no need to save)." });
        return f;
    }

    // On the Advanced page: list the Experimental page in Settings (asks first).
    Control ShowExperimentalBox()
    {
        var box = new CheckBox { Content = "Show experimental features in the list (⚠ Experimental page)", IsChecked = HeadSmoothingFile.ShowPage };
        box.IsCheckedChanged += async (s, e) =>
        {
            bool v = box.IsChecked == true;
            if (v == HeadSmoothingFile.ShowPage) return;
            if (v && !await Dialogs.Confirm(this, "⚠ Experimental features",
                    "Experimental features are unfinished and some can cause motion sickness. They are all off until you switch them on.\n\nShow them?", "Show them"))
            { box.IsChecked = false; return; }
            HeadSmoothingFile.ShowPage = v;
        };
        return box;
    }

    // On the General page: the update check at start (app-wide flag file "no_update_check", like WinForms).
    static string NoUpdateFlag => Path.Combine(ProfileStore.RealDataDir, "no_update_check");
    public static bool UpdateCheckOn
    {
        get => !File.Exists(NoUpdateFlag);
        set { try { if (value) File.Delete(NoUpdateFlag); else { Directory.CreateDirectory(ProfileStore.RealDataDir); File.WriteAllText(NoUpdateFlag, "off"); } } catch { } }
    }
}
