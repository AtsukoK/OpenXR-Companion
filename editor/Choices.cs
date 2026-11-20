using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

// ------------------------------------------------------------ plain-English choices
public record Choice(string Code, string Text) { public override string ToString() => Text; }

public static class Choices
{
    public static readonly Choice[] Hands = { new("left", "Left hand"), new("right", "Right hand") };
    public static readonly Choice[] Buttons =
    {
        new("primary", "A / X button"), new("secondary", "B / Y button"),
        new("trigger", "Trigger"), new("grip", "Grip"),
        new("thumbstick", "Thumbstick click"),
        new("thumbstick.up", "Thumbstick pushed up"), new("thumbstick.down", "Thumbstick pushed down"),
        new("thumbstick.left", "Thumbstick pushed left"), new("thumbstick.right", "Thumbstick pushed right"),
        new("thumbrest", "Thumbrest  (touch: thumb on the pad)"), new("menu", "Menu button"),
        new("primary.touch", "A / X  (just touching)"), new("secondary.touch", "B / Y  (just touching)"),
        new("trigger.touch", "Trigger  (just touching)"), new("thumbstick.touch", "Thumbstick  (just touching)"),
    };
    public static readonly Choice[] Gestures =
    {
        new("press", "Pressed"), new("tap", "Tapped quickly (so a Hold on the same button can still work)"),
        new("hold", "Held down for a moment"), new("double_tap", "Double-tapped"),
        new("while_held", "Held  (starts on press, stops on release)"),
    };
    public static readonly Choice[] Blocks =
    {
        new("none", "No, the game still gets the button"), new("trigger", "Yes, hide the last button from the game"),
        new("all", "Yes, hide all buttons of the combo from the game"),
    };
    public static readonly Choice[] ActionTypes =
    {
        new("key", "Press a keyboard key or mouse button"), new("sequence", "Run a macro (several keys with waits)"),
        new("wheel", "Open a wheel"), new("keyboard", "Open the VR keyboard"),
        new("layer", "Switch a layer (changes what other buttons do)"),
        new("xr_input", "Press a controller button inside the game"),
        new("back", "Wheel: go back one level"), new("close", "Wheel: close"),
        new("reload", "Reload this profile"), new("adjust", "Adjust positions: move wheels and wrist panels by hand"),
        new("screen", "Mirror a monitor: show / hide it, switch monitor, frame rate or place"),
        new("controller_offsets", "Controller offsets: fix how the game's hands and guns point"),
        new("headset_mirror", "Headset mirror: a desktop window showing what the headset shows"), new("none", "Do nothing"),
    };
    public static readonly Choice[] KeyModes = { new("tap", "Tap it once"), new("hold", "Keep it held while my button is held"), new("toggle", "Toggle: first time down, next time up") };
    public static readonly Choice[] ScreenModes =
    {
        new("toggle", "Show / hide it (pressing it again hides it)"), new("on", "Show it"), new("off", "Hide it"),
        new("next_monitor", "Switch to the next monitor"), new("prev_monitor", "Switch to the previous monitor"),
        new("choose", "Open the list of monitors and windows to pick from"),
        new("next_window", "Switch to the next open window (browser, Discord, ...)"), new("prev_window", "Switch to the previous open window"),
        new("next_fps", "Next frame rate (from Settings > Screen mirror)"), new("next_place", "Move it: left wrist, right wrist, in front, following view"),
    };
    public static readonly Choice[] OffsetModes =
    {
        new("panel", "Open / close the offsets panel (+/- buttons, grab to set, reset)"),
        new("enable", "Turn the offsets on / off (compare with the plain controllers)"),
        new("grab", "Grab to set: hold grip, turn the controller to where it feels right, let go"),
        new("reset", "Reset every offset to 0"),
    };
    public static readonly Choice[] MirrorModes = { new("toggle", "Turn it on / off"), new("on", "Turn it on"), new("off", "Turn it off") };
    public static readonly Choice[] ScreenPlaces =
    {
        new("", "Where Settings says"), new("wrist_left", "On my left wrist"), new("wrist_right", "On my right wrist"),
        new("front", "In front of me (stays where it appeared)"), new("head", "In front of me, following my view"),
    };
    public static readonly Choice[] LayerModes = { new("hold", "On while held"), new("toggle", "Toggle on / off"), new("on", "Turn on"), new("off", "Turn off") };
    public static readonly Choice[] Confirms =
    {
        new("auto", "Automatic (best fit for the opening button)"), new("stick_return", "Push the stick to an item, let it spring back"),
        new("release", "Let go of the opening button"), new("button", "Press the confirm button (trigger)"),
    };
}

public class ChoiceBox : ComboBox
{
    public ChoiceBox(IEnumerable<Choice> items, int width = 340)
    {
        DropDownStyle = ComboBoxStyle.DropDownList; Width = width;
        foreach (var c in items) Items.Add(c);
        if (Items.Count > 0) SelectedIndex = 0;
    }
    public string Code
    {
        get => (SelectedItem as Choice)?.Code;
        set
        {
            foreach (Choice c in Items) if (c.Code == value) { SelectedItem = c; return; }
            if (!string.IsNullOrEmpty(value)) { var extra = new Choice(value, value); Items.Add(extra); SelectedItem = extra; }
        }
    }
}

// ------------------------------------------------------------ input parsing
public static class InputText
{
    public static (string hand, string button) Split(string part)
    {
        var bits = part.Trim().ToLowerInvariant().Split('.');
        string hand = bits[0] is "left" or "l" ? "left" : "right";
        string comp = bits.Length > 1 ? bits[1] : "trigger";
        comp = comp switch { "a" or "x" or "btn1" => "primary", "b" or "y" or "btn2" => "secondary", "stick" or "trackpad" => "thumbstick", "squeeze" => "grip", _ => comp };
        string feat = bits.Length > 2 && bits[2] is not ("click" or "press") ? "." + bits[2] : "";
        if (comp == "thumbrest") feat = "";
        return (hand, comp + feat);
    }
    public static string Join(string hand, string button)
    {
        var bits = button.Split('.');
        string comp = bits[0] switch { "primary" => hand == "left" ? "x" : "a", "secondary" => hand == "left" ? "y" : "b", _ => bits[0] };
        return hand + "." + comp + (bits.Length > 1 ? "." + bits[1] : "");
    }
    public static string Friendly(string input)
    {
        if (string.IsNullOrWhiteSpace(input)) return "(nothing)";
        return string.Join(" + ", input.Split('+').Select(p =>
        {
            var (h, b) = Split(p);
            string name = Choices.Buttons.FirstOrDefault(c => c.Code == b)?.Text ?? b;
            if (b.StartsWith("primary")) name = name.Replace("A / X", h == "left" ? "X" : "A");
            if (b.StartsWith("secondary")) name = name.Replace("B / Y", h == "left" ? "Y" : "B");
            return (h == "left" ? "Left " : "Right ") + name.Split("  (")[0];
        }));
    }
}
