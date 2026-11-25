namespace OpenXRCompanionEditor;

public record PickOption(string Group, string Title, string Does, string Example);

// The "What should it do?" cards for a wrist button, a wheel item and a controller binding.
public static class ActionOptions
{
    // What a wrist button can do, in groups, worded short. Same picker as the controllers' "+ Add".
    public static readonly (string code, PickOption option)[] Wrist =
    {
        (ActionType.Key, new("Keys", "Press a key", "A keyboard key, key combo or mouse button.", "quick save, map, push-to-talk.")),
        (ActionType.Sequence, new("Keys", "Type a few keys in a row", "Several keys one after another, with waits if needed (a macro).", "copy-paste, a chat command.")),
        (ActionType.XrInput, new("Keys", "Press a controller button", "The game thinks one of its controller buttons is pressed.", "a game action that has no keyboard key.")),
        (ActionType.Gamepad, new("Keys", "Press an Xbox 360 button", "Emulates an Xbox 360 controller button, for games that read a 360 pad. Optional: needs the free ViGEmBus driver.", "Fallout New Vegas, flat games on the desktop screen.")),
        (ActionType.Page, new("This panel", "Go to another page", "Shows another page of this wrist panel.", "a 'Next page' button.")),
        (ActionType.Back, new("This panel", "Go back a page", "Back to the page before. On the first page it hides the panel.", null)),
        (ActionType.Close, new("This panel", "Hide the panel", "Hides it until you look away from your arm.", null)),
        (ActionType.Keyboard, new("Open", "VR keyboard", "Opens the keyboard you type on in the headset.", "chat, names, searching.")),
        (ActionType.Screen, new("Open", "Desktop screen", "Shows a monitor or window in the headset, or switches which one.", "Discord, a guide, a video.")),
        (ActionType.Wheel, new("Open", "Radial wheel", "Opens one of your radial wheels.", null)),
        (ActionType.SettingsMenu, new("Open", "Settings menu", "Change bindings and sticks inside the headset.", null)),
        (ActionType.Adjust, new("Set up", "Move panels and wheels", "Grab wrist panels and wheels and put them where you like.", null)),
        (ActionType.ControllerOffsets, new("Set up", "Fix hand / gun aim", "Opens the controller offsets, for games whose hands point wrong.", null)),
        (ActionType.HeadsetMirror, new("Set up", "Headset view on the desktop", "A window on the monitor showing what the headset shows.", "streaming or showing someone.")),
        (ActionType.Reload, new("Set up", "Reload profile", "Loads the profile again after you saved changes.", null)),
        (ActionType.Layer, new("Advanced", "Switch a layer", "Changes what other buttons do while the layer is on.", null)),
        (ActionType.None, new("Advanced", "Nothing", "The button is shown but does nothing.", null)),
    };

    // The same for a radial wheel's items: "This wheel" instead of "This panel".
    public static readonly (string code, PickOption option)[] Wheel = Wrist
        .Where(x => x.code is not (ActionType.Page or ActionType.Back or ActionType.Close or ActionType.Wheel))
        .SelectMany(x => x.code == ActionType.Keyboard
            ? new[] {
                (ActionType.Wheel, new PickOption("This wheel", "Open a sub-wheel", "Opens another wheel in its place (or any of your wheels).", "a 'More' item.")),
                (ActionType.Back, new PickOption("This wheel", "Go back a level", "Back to the wheel before; on the first wheel it closes.", null)),
                (ActionType.Close, new PickOption("This wheel", "Close the wheel", "Closes it without doing anything else.", null)),
                x }
            : new[] { x })
        .Select(x => (code: x.Item1, option: x.Item2))
        .Select(x => x.code == ActionType.None ? (x.code, x.option with { Does = "The item is shown but does nothing." }) : x)
        .ToArray();

    // For a controller binding: no panel / wheel navigation; Nothing = the game just doesn't get the button.
    public static readonly (string code, PickOption option)[] Binding = Wrist
        .Where(x => x.code is not (ActionType.Page or ActionType.Back or ActionType.Close))
        .Select(x => x.code == ActionType.None ? (x.code, x.option with { Title = "Nothing", Does = "Does nothing itself. Details decides whether the game still gets the button (No = the button is blocked)." })
                   : x.code == ActionType.XrInput ? (x.code, x.option with { Title = "Act as another controller button" }) : x)
        .ToArray();
}
