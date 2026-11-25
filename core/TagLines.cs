namespace OpenXRCompanionEditor;

// The lines in a controller part's tag, and the short words for how a binding is used and what it does.
public static class TagLines
{
    // What a part's tag lists, SteamVR style: every way of using it, with what it does or "Default" (the game's own control); then the bindings
    // that only work in a layer.
    public static IReadOnlyList<TagLine> Of(IEnumerable<BindingModel> bindings, int hand, ControllerPart part)
    {
        var lines = new List<TagLine>();
        if (!part.IsUsable)
            return lines;
        foreach (bool layerOnes in new[] { false, true })
            foreach (var way in part.Ways)
            {
                string input = ControllerLayouts.InputOf(hand, part, way);
                var bound = bindings.Where(b => b.Enabled && IsLayerOnly(b) == layerOnes && QuickBindings.FiredBy(b, input)).ToList();
                foreach (var b in bound)
                    lines.Add(new TagLine(ShortWhen(part, way, b), DoesText(b.Action), layerOnes ? b.Layers : null));
                if (bound.Count == 0 && !layerOnes)
                    lines.Add(new TagLine(WayWord(part, way), "Default", IsUnused: true)); // what the game does with it
            }
        return lines;
    }

    // A way of using a part in one word: "Press", "Pull", "Squeeze", "Click", "Touch", "Up"...
    public static string WayWord(ControllerPart part, string way) => way switch
    {
        "touch" => "Touch",
        "full" => part.Comp == "grip" ? "Full squeeze" : "Full pull",
        "up" => "Up",
        "down" => "Down",
        "left" => "Left",
        "right" => "Right",
        _ => part.Comp switch { "trigger" => "Pull", "grip" => "Squeeze", "thumbrest" => "Touch", "thumbstick" => "Click", _ => "Press" },
    };

    // "Press", "Hold", "Touch", "Up", "Up, hold", "Pull"...
    public static string ShortWhen(ControllerPart part, string way, BindingModel b)
    {
        string gesture = WhenText(b);
        string wayWord = way switch
        {
            "touch" => "Touch",
            "full" => part.Comp == "grip" ? "Full squeeze" : "Full pull",
            "up" => "Up",
            "down" => "Down",
            "left" => "Left",
            "right" => "Right",
            _ => null,
        };
        if (wayWord == null && b.Gesture == GestureKind.Press)
            return WayWord(part, way);
        if (wayWord == null)
            return gesture;
        return b.Gesture == GestureKind.Press ? wayWord : wayWord + ", " + gesture.ToLowerInvariant();
    }

    public static bool IsTouchOnly(ControllerPart part, string way) => way == "touch" || part.Comp == "thumbrest";

    public static bool IsLayerOnly(BindingModel b) => !string.IsNullOrWhiteSpace(b.Layers);

    // How it is used, in two words: "Press", "Hold", "With Left grip: Double-tap".
    public static string WhenText(BindingModel b)
    {
        string gesture = b.Gesture switch
        {
            GestureKind.Tap => "Quick tap",
            GestureKind.Hold => "Hold",
            GestureKind.DoubleTap => "Double-tap",
            GestureKind.WhileHeld => "While held",
            GestureKind.DoubleTapHold => "2× hold",
            _ => "Press",
        };
        var parts = (b.Input ?? "").Split('+').Select(p => p.Trim()).Where(p => p.Length > 0).ToList();
        return parts.Count > 1 ? "With " + InputText.Friendly(string.Join("+", parts.Take(parts.Count - 1))) + ": " + gesture : gesture;
    }

    // What it does, short: "Key: Space", "Key: Ctrl (hold)", "Controller: Right B", "Wheel 'right'".
    public static string DoesText(ActionModel a)
    {
        string mode = a?.Mode switch { "hold" => " (hold)", "toggle" => " (toggle)", _ => "" };
        return a?.Type switch
        {
            ActionType.Key => "Key: " + (a.Keys ?? "?") + mode,
            ActionType.XrInput => "Controller: " + InputText.Friendly(a.Target) + mode,
            ActionType.None => "Disabled in the game",
            ActionType.Wheel => $"Wheel '{a.Wheel}'",
            ActionType.Keyboard => "VR keyboard",
            ActionType.Layer => $"Layer '{a.Layer}'" + mode,
            ActionType.Sequence => "Macro",
            _ => ActionText.Plain(a),
        };
    }
}
