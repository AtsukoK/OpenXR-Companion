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

    public const int DefaultHoldMs = 400, DoubleFlickMs = 350;

    // A double flick of the stick (or double tap of a button), then held: the key is held until you let go.
    // Like sprinting on a double flick forward. The game keeps getting the stick or button as usual.
    public static BindingModel DoubleFlickKey(string input, string partName, string keys) => new()
    {
        Name = $"{partName}, double flick: {keys}",
        Input = input,
        Gesture = GestureKind.DoubleTapHold,
        DoubleTapMs = DoubleFlickMs,
        ReleaseAt = (input ?? "").EndsWith(".up") || (input ?? "").EndsWith(".down") || (input ?? "").EndsWith(".left") || (input ?? "").EndsWith(".right") ? 0.3 : 0,
        Block = "none",
        Action = new ActionModel { Type = ActionType.Key, Keys = keys, Mode = "hold" },
    };

    // Only when held for a moment, like the Bannerlord VR radial menu: the game never sees the button while you hold it,
    // and a quick tap still reaches the game as a normal press of the button. The action starts once it has been held.
    // touch: nothing is hidden from the game and there is no tap to pass on (a touch can't be hidden without its presses)
    public static BindingModel OnlyWhenHeld(BindingModel b, bool touch = false)
    {
        b.Gesture = GestureKind.Hold;
        b.HoldMs = DefaultHoldMs;
        b.TapPassthrough = !touch;
        b.Block = touch ? "none" : "trigger";
        b.Name += " (hold)";
        return b;
    }

    // The bindings on a part of one hand's controller, switched on and off (none on a part that can't be used).
    public static PartUse UseOf(IEnumerable<BindingModel> bindings, int hand, ControllerPart part)
    {
        if (!part.IsUsable)
            return default;
        var on = bindings.Where(b => part.Ways.Any(w => FiredBy(b, ControllerLayouts.InputOf(hand, part, w)))).ToList();
        return new PartUse(on.Count(b => b.Enabled), on.Count(b => !b.Enabled));
    }

    // The binding is fired by this input (its last button; any others of a combo must be held first).
    public static bool FiredBy(BindingModel b, string input)
    {
        string last = (b.Input ?? "").Split('+').Last();
        return last.Trim().Length > 0 && InputText.Split(last) == InputText.Split(input);
    }
}
