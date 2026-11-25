using OpenXRCompanionEditor;

namespace OverviewPrototype;

public partial class SettingsPageView
{
    // Like the WinForms SettingsEditor constructor: a profile without a section gets an empty one (with its defaults)
    // when a settings page opens. The user's profiles all have them; older or hand-written ones may not.
    public static void EnsureParts(ProfileModel p)
    {
        p.Thresholds ??= new();
        p.Style ??= new();
        p.Selection ??= new();
        p.Keyboard ??= new();
        p.Pointer ??= new();
        p.WristLeft ??= new();
        p.WristRight ??= new();
        p.Style.Colors ??= new();
        p.Screen ??= new();
        p.Offsets ??= new();
        p.Offsets.Left ??= new();
        p.Offsets.Right ??= new();
        p.Offsets.Left.Grip ??= new();
        p.Offsets.Left.Aim ??= new();
        p.Offsets.Right.Grip ??= new();
        p.Offsets.Right.Aim ??= new();
    }
}
