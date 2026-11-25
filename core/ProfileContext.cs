namespace OpenXRCompanionEditor;

// The profile being edited, for the dropdowns of the binding grid (wheel names, layer names). Set by the editor.
public static class ProfileContext
{
    public static Func<ProfileModel> Current;
}
