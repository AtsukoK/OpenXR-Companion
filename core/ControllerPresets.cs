namespace OpenXRCompanionEditor;

// "Your controllers": the presets for which buttons open the two main wheels.
public static class ControllerPresets
{
    // "asked already", and the kind picked last time (app-wide)
    public static string FlagFile => Path.Combine(Paths.DataDir, "controller_chosen");

    public static readonly (string id, string label, string how)[] Kinds =
    {
        ("touch", "Meta Quest / Rift, Pico (Touch-style controllers)",
                  "Double-tap the thumbrest (just touch it, no press) to open that hand's wheel."),
        ("index", "Valve Index, Pimax Crystal, Bigscreen (Index-style)",
                  "Double-click the thumbstick (press it in twice quickly) to open that hand's wheel."),
        ("frame", "Steam Frame",
                  "Double-click the thumbstick (press it in twice quickly) to open that hand's wheel."),
        ("wmr",   "Windows Mixed Reality / HP Reverb",
                  "Hold the menu button to open that hand's wheel, point the stick, let go to pick. A quick tap still reaches the game."),
        ("vive",  "HTC Vive / Vive Pro wands",
                  "Hold the menu button to open that hand's wheel, point at an item on the trackpad, let go to pick. A quick tap still reaches the game."),
    };

    // The controller kind to preselect: the one chosen last time, else what the profile's wheel openers look like.
    public static string CurrentKind(ProfileModel p)
    {
        try
        {
            string saved = File.Exists(FlagFile) ? File.ReadAllText(FlagFile).Trim() : "";
            if (Kinds.Any(k => k.id == saved))
                return saved;
        }
        catch { }
        foreach (var b in p.Bindings)
        {
            if (OpenerHand(p, b) == null)
                continue;
            if (b.Input.EndsWith(".thumbrest"))
                return "touch";
            if (b.Input.EndsWith(".thumbstick") && b.Gesture == GestureKind.DoubleTap)
                return "index";
            if (b.Input.EndsWith(".menu"))
                return "wmr";
        }
        return null;
    }

    // The binding that opens each hand's main wheel: one opening a wheel named 'right' / 'left' (older profiles), else,
    // for profiles whose wheels have other names ('Right Hand Default'...), each hand's first wheel binding without
    // layers, by the hand of its button. "right" / "left", or null for any other binding.
    public static string OpenerHand(ProfileModel p, BindingModel b)
    {
        if (b.Action?.Type != ActionType.Wheel || !string.IsNullOrEmpty(b.Layers) || b.Input == null)
            return null;
        bool named = p.Bindings.Any(x => x.Action?.Type == ActionType.Wheel && (x.Action.Wheel == "right" || x.Action.Wheel == "left"));
        if (named)
            return b.Action.Wheel == "right" ? "right" : b.Action.Wheel == "left" ? "left" : null;
        string hand = b.Input.StartsWith("right.") ? "right" : b.Input.StartsWith("left.") ? "left" : null;
        if (hand == null)
            return null;
        var first = p.Bindings.FirstOrDefault(x => x.Action?.Type == ActionType.Wheel && string.IsNullOrEmpty(x.Layers) && x.Input != null && x.Input.StartsWith(hand + "."));
        return ReferenceEquals(first, b) ? hand : null;
    }

    // Rewrites the bindings that open the main 'right' / 'left' wheels for this controller kind.
    // Returns how many bindings were changed. Also used by the --selftest check.
    public static int Apply(ProfileModel p, string kind)
    {
        int n = 0;
        foreach (var b in p.Bindings)
        {
            string hand = OpenerHand(p, b);
            if (hand == null)
                continue;
            string side = hand == "right" ? "Right" : "Left";
            switch (kind)
            {
                case "touch":
                    b.Input = hand + ".thumbrest";
                    b.Gesture = GestureKind.DoubleTap;
                    b.DoubleTapMs = 400;
                    b.Block = "none";
                    b.TapPassthrough = false;
                    b.Action.Confirm = "stick_return";
                    b.Name = side + " hand wheel";
                    break;
                case "index":
                case "frame":
                    b.Input = hand + ".thumbstick";
                    b.Gesture = GestureKind.DoubleTap;
                    b.DoubleTapMs = 400;
                    b.Block = "none";
                    b.TapPassthrough = false;
                    b.Action.Confirm = "stick_return";
                    b.Name = side + " hand wheel (double-click stick)";
                    break;
                default: // wmr, vive
                    b.Input = hand + ".menu";
                    b.Gesture = GestureKind.Hold;
                    b.HoldMs = 500;
                    b.Block = "trigger";
                    b.TapPassthrough = true;
                    b.Action.Confirm = "release";
                    b.Name = side + " hand wheel (hold menu)";
                    break;
            }
            n++;
        }
        // WMR and Vive have no B/Y: the grip goes back / closes instead.
        if (n > 0)
            p.Selection.CancelButton = kind == "wmr" || kind == "vive" ? "grip" : "secondary";
        return n;
    }
}

// Which controller the Right / Left controller pages draw (editor only): the one last picked there, else the one
// that fits the choice in "Your controllers".
public static class ControllerPictureChoice
{
    static string FilePath => Path.Combine(Paths.DataDir, "controller_picture.txt");

    public static string Load(ProfileModel profile)
    {
        try
        {
            string saved = File.Exists(FilePath) ? File.ReadAllText(FilePath).Trim() : "";
            if (ControllerLayouts.All.Any(l => l.Id == saved))
                return saved;
        }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
        return ControllerLayouts.ForSetupKind(ControllerPresets.CurrentKind(profile));
    }

    public static void Save(string layoutId)
    {
        try
        { AtomicFile.WriteAllText(FilePath, layoutId); }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }
}
