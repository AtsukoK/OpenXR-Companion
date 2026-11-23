namespace OpenXRCompanionEditor;

// Every wheel as a card: its picture, name, items and what opens it (from the dist3 design).
public sealed class WheelsHub : HubPage
{
    // Who opens a wheel: a controller binding, or an item on another wheel / wrist page (then it is a sub-wheel).
    static string OpenerOf(ProfileModel p, string name, out bool isSub)
    {
        isSub = false;
        var b = p.Bindings.FirstOrDefault(x => x.Enabled && x.Action?.Type == ActionType.Wheel && x.Action.Wheel == name);
        if (b != null)
            return "Opened by " + InputText.Friendly(b.Input) + Gesture(b.Gesture);
        foreach (var w in p.Wheels.Values)
            foreach (var it in w.Items)
                if (it.Enabled && it.Action?.Type == ActionType.Wheel && it.Action.Wheel == name)
                {
                    isSub = true;
                    return $"Sub-wheel of '{w.Title}' ({it.Label})";
                }
        return null;
    }

    static string Gesture(string g) => g switch
    {
        GestureKind.Hold => " (hold)",
        GestureKind.Tap => " (tap)",
        GestureKind.DoubleTap => " (double-tap)",
        GestureKind.DoubleTapHold => " (double-tap and hold)",
        GestureKind.WhileHeld => " (while held)",
        _ => "",
    };

    bool newShown;
    Action newWheelAction;
    void NewTile()
    {
        newShown = true;
        Tile("", "New wheel", "A new main wheel. Then give it items and pick the button that opens it.", null, Color.Empty, newWheelAction, 300, 250);
    }

    public static int OpenedCount(ProfileModel p) => p.Wheels.Keys.Count(n => OpenerOf(p, n, out _) != null);

    public WheelsHub(ProfileModel p, Action<object> go, Action newWheel) : base("Radial wheels", "Every wheel in this profile and what opens it. Click one to edit its items.")
    {
        newWheelAction = newWheel;
        var list = p.Wheels.Values.Select(w => (w, opener: OpenerOf(p, w.Name, out bool sub), sub)).ToList();
        if (list.Count == 0)
            Break("No wheels yet");
        foreach (var group in new[] { (false, "Main wheels"), (true, "Sub-wheels") })
        {
            var these = list.Where(x => x.sub == group.Item1).ToList();
            if (these.Count == 0)
                continue;
            Break(group.Item2);
            foreach (var (w, opener, _) in these)
            {
                int on = w.Items.Count(i => i.Enabled);
                var pic = MiniPics.Wheel(150, 110, 1, w, opener != null);
                Tile(null, string.IsNullOrWhiteSpace(w.Title) ? w.Name : w.Title,
                     $"{on} of {w.Items.Count} items on · name '{w.Name}'",
                     opener ?? "Not assigned: nothing opens it", opener != null ? Green : Orange, () => go(w), 380, 250, pic);
            }
            if (!group.Item1) // the main wheels: a "New wheel" card at the end of their row
                NewTile();
        }
        if (!newShown)
            NewTile();
    }
}

