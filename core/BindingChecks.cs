using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace OpenXRCompanionEditor;

// "This may need your attention": bindings that will likely do something the player doesn't expect.
// Each rule looks at the whole profile, so it can see two bindings that undo each other. No window code here:
// the editor draws the results (a glow on the setting, a dot in the lists, a card on the Overview).

// Field: which setting it is about, so the page can light it up: "block", "short_press", "how", "button", "layers", "action".
public sealed record BindingIssue(BindingModel Binding, string Rule, string Field, string Title, string Text,
                                  string FixLabel = null, Action Fix = null, BindingModel Other = null)
{
    // Changes when either binding changes, so a "Looks right" is asked again after an edit.
    public string Key => Rule + "|" + BindingChecks.Fingerprint(Binding) + (Other == null ? "" : "|" + BindingChecks.Fingerprint(Other));
}

public sealed record CheckContext(ControllerLayout Controller, bool ViGEmBusInstalled);

public static class BindingChecks
{
    public static string Fingerprint(BindingModel b)
    {
        var json = JsonSerializer.Serialize(b, ProfileIO.JsonOpts);
        return Convert.ToHexString(SHA1.HashData(Encoding.UTF8.GetBytes(json)))[..12];
    }

    // the button a binding fires on (its last chord part), as (hand, "thumbstick.up")
    static (string hand, string button) Trigger(BindingModel b) => InputText.Split((b.Input ?? "").Split('+', StringSplitOptions.RemoveEmptyEntries).LastOrDefault() ?? "");
    static bool Single(BindingModel b) => !string.IsNullOrWhiteSpace(b.Input) && !b.Input.Contains('+');
    static bool Keeps(BindingModel b) => (b.Block ?? "none") != "none";
    static bool Leaks(BindingModel b) => b.Gesture == GestureKind.Hold && b.TapPassthrough && b.TapAction == null;
    static string Btn(BindingModel b) => InputText.Friendly((b.Input ?? "").Split('+').Last());
    static string Set(string s) => string.Join(",", (s ?? "").Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).OrderBy(x => x, StringComparer.OrdinalIgnoreCase));
    static string Chord(BindingModel b) => string.Join("+", (b.Input ?? "").Split('+', StringSplitOptions.RemoveEmptyEntries)
                                                                .Select(p => { var (h, x) = InputText.Split(p); return h + "." + x; }).OrderBy(x => x));
    // press and while-held both start the moment the button goes down
    static string StartGroup(string g) => g is GestureKind.Press or GestureKind.WhileHeld ? "press" : g;

    public static List<BindingIssue> Check(ProfileModel p, CheckContext ctx)
    {
        var issues = new List<BindingIssue>();
        var on = p.Bindings.Where(b => b.Enabled && b.Action != null).ToList();

        foreach (var b in on)
        {
            var trig = Trigger(b);
            string btn = Btn(b);
            var same = on.Where(o => !ReferenceEquals(o, b) && Single(o) && Trigger(o) == trig).ToList();

            // 1. A Hold lets short presses through while another binding keeps the button from the game.
            if (Keeps(b))
                foreach (var o in same.Where(Leaks))
                    issues.Add(new(b, "short_press_leak", "block", "Single presses still reach the game",
                        $"'{o.Name}' (Hold) on {btn} sends short presses to the game, so they still reach it even though this binding keeps the button from the game.",
                        "Stop that binding's short presses", () => o.TapPassthrough = false, o));
            if (Leaks(b) && same.Any(Keeps))
            {
                var o = same.First(Keeps);
                issues.Add(new(b, "short_press_leak", "short_press", "Short presses get past another binding",
                    $"This binding sends short presses of {btn} to the game, but '{o.Name}' keeps the button from the game. Short presses still reach it.",
                    "Stop this binding's short presses", () => b.TapPassthrough = false, o));
            }

            // 2. Two bindings start on the very same press (same buttons, same How, same layers): both run.
            foreach (var o in on.Where(o => !ReferenceEquals(o, b) && Chord(o) == Chord(b) && StartGroup(o.Gesture) == StartGroup(b.Gesture)
                                            && Set(o.Layers) == Set(b.Layers) && Set(o.NotLayers) == Set(b.NotLayers)))
                issues.Add(new(b, "double_fire", "how", "Another binding fires on the same press",
                    $"'{o.Name}' uses the same button{(b.Input.Contains('+') ? "s" : "")} the same way, so both run at once.",
                    "Switch this one off", () => b.Enabled = false, o));

            // 3. A double-tap on a button the game still gets: each of the two taps reaches the game too.
            bool touchy = trig.button.EndsWith(".touch") || trig.button == "thumbrest";
            bool direction = trig.button.Contains('.') && !touchy && !trig.button.EndsWith(".full");
            if (b.Gesture == GestureKind.DoubleTap && !Keeps(b) && !touchy && !direction)
                issues.Add(new(b, "double_tap_reaches_game", "block", "The game gets both taps",
                    $"The game still gets {btn}, so a double tap also presses it twice in the game (and a single tap once).",
                    "Keep the button from the game", () => b.Block = "trigger"));

            // 4. Keeping a stick click from the game also holds back the stick's movement while you press it.
            if (Keeps(b) && (b.Input ?? "").Split('+').Any(x => InputText.Split(x).button == "thumbstick"))
                issues.Add(new(b, "stick_click_holds_movement", "block", "Movement pauses while you click",
                    "This binding keeps the stick from the game, so walking or turning with that stick stops for a moment each time you click it."));

            // 5. "Only while layer" names a layer that nothing switches on: the binding can never work.
            foreach (var layer in (b.Layers ?? "").Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                if (!LayerSwitchedOn(p, layer))
                    issues.Add(new(b, "layer_never_on", "layers", "This mode is never switched on",
                        $"It only works in {layer} mode, but no binding, wheel item or wrist button turns {layer} mode on."));

            // 6. A button the chosen controller doesn't have (still works on controllers that do).
            if (ctx?.Controller is { } lay)
            {
                foreach (var part in (b.Input ?? "").Split('+', StringSplitOptions.RemoveEmptyEntries))
                    if (!Has(lay, part))
                    {
                        issues.Add(new(b, "missing_button", "button", "Your controller doesn't have this button",
                            $"{InputText.Friendly(part)} isn't on a {lay.Name}, so this binding never fires on it (it does on controllers that have it)."));
                        break;
                    }
                foreach (var a in new[] { b.Action, b.TapAction })
                    if (a?.Type == ActionType.XrInput && !string.IsNullOrWhiteSpace(a.Target) && !Has(lay, a.Target))
                        issues.Add(new(b, "missing_target", "action", "The game button isn't on your controller",
                            $"It presses {InputText.Friendly(a.Target)}, which a {lay.Name} doesn't have. A game made for your controller likely ignores it."));
            }

            // 7. Xbox 360 buttons without the driver do nothing.
            if (ctx is { ViGEmBusInstalled: false } && (b.Action?.Type == ActionType.Gamepad || b.TapAction?.Type == ActionType.Gamepad))
                issues.Add(new(b, "no_vigembus", "action", "Xbox 360 buttons need ViGEmBus",
                    "It presses an Xbox 360 button, but the optional ViGEmBus driver isn't installed, so nothing happens."));
        }
        return issues;
    }

    static bool Has(ControllerLayout lay, string input)
    {
        var (hand, button) = InputText.Split(input);
        string comp = button.Split('.')[0];
        int h = hand == "left" ? 0 : 1;
        return lay.PartsFor(h).Any(x => x.Comp == comp && x.IsUsable);
    }

    static bool LayerSwitchedOn(ProfileModel p, string layer)
    {
        bool Turns(ActionModel a) => a?.Type == ActionType.Layer && string.Equals(a.Layer, layer, StringComparison.OrdinalIgnoreCase) && a.Mode != "off";
        if (p.Bindings.Any(b => b.Enabled && (Turns(b.Action) || Turns(b.TapAction)))) return true;
        if (p.Wheels.Values.Any(w => w.Items.Any(i => i.Enabled && Turns(i.Action)))) return true;
        for (int h = 0; h < 2; h++)
            if (p.WristFor(h).Pages.Values.Any(w => w.Items.Any(i => i.Enabled && Turns(i.Action)))) return true;
        return false;
    }
}
