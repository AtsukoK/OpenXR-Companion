namespace OpenXRCompanionEditor;

// What an action does, in words ("Presses F5", "Opens wheel 'right'").
public static class ActionText
{
    public static string Plain(ActionModel a)
    {
        if (a == null)
            return "Does nothing";
        string mode = a.Mode switch { "toggle" => " (toggle on/off)", "hold" => " (held while the button is)", _ => "" };
        return a.Type switch
        {
            "key" => $"Presses {a.Keys}{mode}",
            "sequence" => $"Runs a macro: {a.Steps}",
            "wheel" => $"Opens wheel '{a.Wheel}'",
            "page" => $"Opens page '{a.Page}'",
            "keyboard" => "Opens the VR keyboard",
            "adjust" => "Adjust mode on/off: grip to move, stick to resize",
            "headset_mirror" => a.Mode switch { "on" => "Turns the headset mirror window on", "off" => "Turns the headset mirror window off", _ => "Headset mirror window on / off" },
            "settings_menu" => "Opens the in-game settings menu",
            "controller_offsets" => a.Mode switch
            {
                "enable" => "Controller offsets on / off",
                "grab" => "Controller offsets: grab to set",
                "reset" => "Controller offsets: reset all",
                _ => "Opens the controller offsets panel",
            },
            "layer" => a.Mode switch { "on" => $"Turns {a.Layer} mode on", "off" => $"Turns {a.Layer} mode off", "toggle" => $"Switches {a.Layer} mode on/off", _ => $"{a.Layer} mode while held" },
            "xr_input" => $"Presses game button {a.Target}{mode}",
            "gamepad" => $"Presses gamepad {Choices.PadName(a.Button)}{mode}",
            "back" => "Goes back",
            "close" => "Closes / hides",
            "reload" => "Reloads the profile",
            "screen" => a.Mode switch
            {
                "on" => "Shows the mirrored screen",
                "off" => "Hides the mirrored screen",
                "next_monitor" => "Screen: next monitor",
                "prev_monitor" => "Screen: previous monitor",
                "choose" => "Screen: opens the list of monitors and windows",
                "next_window" => "Screen: next window",
                "prev_window" => "Screen: previous window",
                "next_fps" => "Screen: next frame rate",
                "next_place" => "Screen: moves it to the next place",
                _ => "Shows / hides the mirrored screen" + (a.Place switch { "wrist_left" => " on the left wrist", "wrist_right" => " on the right wrist", "front" => " in front of you", "head" => " following your view", _ => "" }),
            },
            _ => "Does nothing",
        };
    }
}
