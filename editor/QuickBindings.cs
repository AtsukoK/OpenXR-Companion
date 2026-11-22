namespace OpenXRCompanionEditor;

// Ready-made bindings for one way of using a controller button, like the choices SteamVR's controller bindings
// offer: press a key, act as another of the game's buttons, or turn the button off for the game.
// They are ordinary bindings, so they can be changed afterwards in the binding editor.
public static class QuickBindings
{
    static bool IsTouch(string input) => input.EndsWith(".touch") || input.Contains(".thumbrest");

    // A touch is felt before every press, so a touch binding that hid the button would hide its presses too.
    static string BlockFor(string input) => IsTouch(input) ? "none" : "trigger";

    // While the button is used, the key is held down (released with the button).
    public static BindingModel Key(string input, string partName, string keys) => new()
    {
        Name = $"{partName}: {keys}",
        Input = input,
        Gesture = GestureKind.WhileHeld,
        Block = BlockFor(input),
        Action = new ActionModel { Type = ActionType.Key, Keys = keys, Mode = "hold" },
    };

    // While the button is used, the game sees another of its buttons instead (held with it).
    public static BindingModel GameButton(string input, string partName, string target, string targetName) => new()
    {
        Name = $"{partName} acts as {targetName}",
        Input = input,
        Gesture = GestureKind.WhileHeld,
        Block = "trigger",
        Action = new ActionModel { Type = ActionType.XrInput, Target = target, Mode = "hold" },
    };

    // The game never sees the button pressed (it is hidden for as long as it is held).
    public static BindingModel OffInGame(string input, string partName) => new()
    {
        Name = $"{partName}: off in the game",
        Input = input,
        Gesture = GestureKind.WhileHeld,
        Block = "trigger",
        Action = new ActionModel { Type = ActionType.None },
    };

    // A plain binding to finish in the binding editor.
    public static BindingModel Blank(string input, string partName) => new()
    {
        Name = partName,
        Input = input,
        Gesture = GestureKind.Press,
        Block = BlockFor(input),
        Action = new ActionModel { Type = ActionType.Key, Keys = "F1" },
    };

    // The binding is fired by this input (its last button; any others of a combo must be held first).
    public static bool FiredBy(BindingModel b, string input)
    {
        string last = (b.Input ?? "").Split('+').Last();
        return last.Trim().Length > 0 && InputText.Split(last) == InputText.Split(input);
    }
}
